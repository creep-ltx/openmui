/*
    Copyright (C) 2002-2026, The AROS Development Team.
    Copyright (C) 1999, David Le Corfec.
    All rights reserved.

*/

#include <stddef.h>
#include <exec/types.h>
#include <devices/timer.h>
#include <dos/dostags.h>
#include <dos/datetime.h>
#include <utility/date.h>
#include <prefs/prefhdr.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <clib/alib_protos.h>
#include <libraries/commodities.h>
#include <rexx/errors.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/commodities.h>
#include <proto/muimaster.h>
#include <proto/iffparse.h>
#include <proto/rexxsyslib.h>
#include <proto/workbench.h>
#include <proto/icon.h>
#include <proto/amigaguide.h>
#include <libraries/amigaguide.h>
#ifndef MUI_OBSOLETE
#define MUI_OBSOLETE
#endif //

//#define MYDEBUG 1
#include "debug.h"
#include "prefs.h"

#include "muimaster_intern.h"
#include "mui.h"
#include "support.h"
#include "application_private.h"
#include "modules.h"

#include <string.h>

extern struct Library *MUIMasterBase;



/* Make room for at least cap records (at most WINPOS_MAX), cleared. */
static BOOL winpos_grow(struct MUI_ApplicationData *data, ULONG cap)
{
    LONG *table;

    if (cap > WINPOS_MAX)
        cap = WINPOS_MAX;
    if (data->app_WinPos && cap <= data->app_WinPosCap)
        return TRUE;
    table = AllocVec(sizeof(LONG) + cap * sizeof(struct windowpos),
        MEMF_ANY | MEMF_CLEAR);
    if (!table)
        return FALSE;
    if (data->app_WinPos)
    {
        CopyMem(data->app_WinPos, table,
            sizeof(LONG) + data->app_WinPosCap * sizeof(struct windowpos));
        FreeVec(data->app_WinPos);
    }
    data->app_WinPos = table;
    data->app_WinPosCap = cap;
    return TRUE;
}


/****** Application.mui/MUIC_Application ************
*
*   NAME
*       MUIC_Application
*
*   SUPERCLASS
*       Notify.mui
*
*   DESCRIPTION
*       Own the application window family, optional menustrip, configuration
*       object, message ports, and event queues. Construction binds the
*       application to the creating task. Process input and perform ordinary
*       object mutations in that task; the pushed-method queue has a separate
*       semaphore but does not protect object lifetimes.
*       Window tags attach owned children. OM_ADDMEMBER appends through the
*       internal Family and connects the child to this application; OM_REMMEMBER
*       disconnects and unlinks without disposing it. Both member methods return
*       TRUE without checking child results. Supply live, correctly owned
*       Window-compatible children and remove them before separate disposal.
*       Application disposal removes and disposes remaining windows, the
*       menustrip, and configuration data, then releases its own resources.
*       Caller-supplied metadata strings, hooks, command tables, and DiskObject
*       remain borrowed unless an attribute explicitly documents copying. Stop
*       input processing and callbacks and remove input handlers before
*       disposal. Disposal discards queued methods without executing them.
*
*   METHODS
*       FindUData and GetUData examine the application first, then delegate to
*       the internal window Family. That Family itself participates in matching
*       before its children, including its default UserData value zero. GetUData
*       returns a match result, not the attribute getter's success.
*       SetUData sets the application on a match, visits the internal Family,
*       and returns TRUE regardless of matches. SetUDataOnce stops at the
*       application or first delegated match. The application menustrip is not
*       traversed by these overrides. Keep membership stable during callbacks.
*       MUIA_ShowMe is also accepted by the setter and propagated to windows.
*       The iteration rereads the window list after each setter and assumes that
*       a processed window does not remove itself. Notify answers MUIA_Version
*       and MUIA_Revision.
*
*   NOTES
*       Normal construction returns NULL on required allocation, port, timer,
*       configuration, legacy menu, or child validation failure. Optional
*       broker or Rexx-port creation failure does not fail construction. The
*       first input-processing call creates configuration notification for
*       ENV:OpenMUI/<base>.prefs when possible and reloads the configuration if
*       that file was written after construction; failure to watch that file
*       is not reported. NewInput processes changes and queues configuration
*       replacement.
*       Disposal can perform configuration-file I/O. Depending on the
*       configured window-position mode, it records the positions of open
*       windows and writes ENV preferences; mode 2 also reloads and saves
*       ENVARC preferences to retain persistent settings while updating
*       positions.
*
*   BUGS
*       The early disposal path dereferences mgi_Prefs whenever app_Base is
*       non-NULL. Failure after assigning the base but before preferences are
*       available can therefore access NULL instead of cleanly returning
*       construction failure.
*       Disposal does not enumerate and remove registered timer handlers.
*       Handler resources must not be assumed to be reclaimed by application
*       disposal. Cross-task queue insertion is not an application-lifetime
*       guarantee.
*
*/
/****** Application.mui/MUIA_Application_Author ************
*
*   NAME
*       MUIA_Application_Author
*
*   FUNCTION
*       Store the application author string. Default NULL.
*
*   INPUTS
*       STRPTR; initialize and get.
*
*   NOTES
*       The pointer is stored without copying and is not freed. Keep the string
*       alive while the application uses it; there is no runtime setter for this
*       attribute.
*
*/
/****** Application.mui/MUIA_Application_Copyright ************
*
*   NAME
*       MUIA_Application_Copyright
*
*   FUNCTION
*       Store the application copyright string. Default NULL.
*
*   INPUTS
*       STRPTR; initialize and get.
*
*   NOTES
*       The pointer is stored without copying and is not freed. Keep the string
*       alive while the application uses it; there is no runtime setter for this
*       attribute.
*
*/
/****** Application.mui/MUIA_Application_Description ************
*
*   NAME
*       MUIA_Application_Description
*
*   FUNCTION
*       Store the application description string. Default "?".
*
*   INPUTS
*       STRPTR; initialize and get.
*
*   NOTES
*       The pointer is stored without copying and is not freed. Keep the string
*       alive while the application uses it; there is no runtime setter for this
*       attribute.
*
*/
/****** Application.mui/MUIA_Application_Title ************
*
*   NAME
*       MUIA_Application_Title
*
*   FUNCTION
*       Store the application title string. Default "Unnamed".
*
*   INPUTS
*       STRPTR; initialize and get.
*
*   NOTES
*       The pointer is stored without copying and is not freed. Keep the string
*       alive while the application uses it; there is no runtime setter for this
*       attribute.
*
*/
/****** Application.mui/MUIA_Application_Version_Number ************
*
*   NAME
*       MUIA_Application_Version_Number
*
*   FUNCTION
*       Store the application version number string. Default NULL.
*
*   INPUTS
*       STRPTR; initialize and get.
*
*   NOTES
*       The pointer is stored without copying and is not freed. Keep the string
*       alive while the application uses it; there is no runtime setter for this
*       attribute. This value participates in version-string construction only
*       when Version is explicitly NULL; Date is not format-validated.
*
*/
/****** Application.mui/MUIA_Application_Version_Date ************
*
*   NAME
*       MUIA_Application_Version_Date
*
*   FUNCTION
*       Store the application version date string. Default NULL.
*
*   INPUTS
*       STRPTR; initialize and get.
*
*   NOTES
*       The pointer is stored without copying and is not freed. Keep the string
*       alive while the application uses it; there is no runtime setter for this
*       attribute. This value participates in version-string construction only
*       when Version is explicitly NULL; Date is not format-validated.
*
*/
/****** Application.mui/MUIA_Application_Version_Extra ************
*
*   NAME
*       MUIA_Application_Version_Extra
*
*   FUNCTION
*       Store the application version extra string. Default NULL.
*
*   INPUTS
*       STRPTR; initialize and get.
*
*   NOTES
*       The pointer is stored without copying and is not freed. Keep the string
*       alive while the application uses it; there is no runtime setter for this
*       attribute. This value participates in version-string construction only
*       when Version is explicitly NULL; Date is not format-validated.
*
*/
/****** Application.mui/MUIA_Application_Version ************
*
*   NAME
*       MUIA_Application_Version
*
*   FUNCTION
*       Store the supplied version pointer. If explicitly set to NULL, and both
*       Title and Version_Number are non-NULL, allocate "$VER: <title> <number>"
*       with optional " (<date>)" and " [<extra>]" suffixes.
*
*   INPUTS
*       STRPTR; initialize and get.
*
*   NOTES
*       A supplied string is borrowed. An automatically generated string is
*       owned by the application and freed at disposal. The getter returns a
*       borrowed pointer in either case. Failed generation leaves NULL without
*       failing construction.
*
*   BUGS
*       Omission does not request generation: the constructor initializes
*       Version to "Unnamed 0.0". This differs from the header comment that
*       promises generation when the attribute is unspecified.
*
*/
/****** Application.mui/MUIA_Application_Base ************
*
*   NAME
*       MUIA_Application_Base
*
*   FUNCTION
*       Choose the base name for application registration, Rexx port naming, and
*       configuration paths. Default "UNNAMED". NULL or any of : / ( ) # ? * ,
*       causes construction failure.
*
*   INPUTS
*       STRPTR; initialize and get.
*
*   NOTES
*       The stored result is an allocated copy, freed at application disposal.
*       With SingleTask FALSE, the constructor tries suffixes .1 through .999
*       against tracked application bases. SingleTask TRUE uses the supplied
*       name without a suffix. The getter returns the actual borrowed stored
*       name.
*
*   BUGS
*       Suffix construction uses a 255-byte buffer and can truncate. Exhaustion
*       of the suffix search is not rejected; this is not an unconditional
*       unique-name guarantee. The validation does not make every accepted name
*       safe as an unquoted shell argument; OpenConfigWindow inserts it without
*       quoting.
*
*/
/****** Application.mui/MUIA_Application_SingleTask ************
*
*   NAME
*       MUIA_Application_SingleTask
*
*   FUNCTION
*       Use a shared unsuffixed base when true; default FALSE. If a tracked
*       application already has the base, queue MUIM_Set of DoubleStart=TRUE on
*       that application and fail this construction.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       The notification is deferred through the other application's
*       pushed-method queue. This attribute does not make the application or its
*       objects safe for concurrent use.
*
*   BUGS
*       The target application's lifetime until the queued notification executes
*       is not retained. Registration and name lookup do not provide a general
*       cross-task lifetime guarantee.
*
*/
/****** Application.mui/MUIA_Application_Active ************
*
*   NAME
*       MUIA_Application_Active
*
*   FUNCTION
*       Store a normalized active flag, default TRUE. Activate or deactivate the
*       Commodities broker when one exists.
*
*   INPUTS
*       BOOL; initialize, set, get.
*
*   NOTES
*       This flag does not disable NewInput processing or put windows to sleep.
*       The ordinary setter forwards tags to Notify after updating broker state.
*
*/
/****** Application.mui/MUIA_Application_Broker ************
*
*   NAME
*       MUIA_Application_Broker
*
*   FUNCTION
*       Return the application-owned Commodities broker, or NULL if none was
*       created.
*
*   INPUTS
*       CxObj pointer; get only.
*
*   NOTES
*       Do not delete it. The application destroys it at disposal.
*       UseCommodities=TRUE does not guarantee a non-NULL result.
*
*/
/****** Application.mui/MUIA_Application_BrokerPort ************
*
*   NAME
*       MUIA_Application_BrokerPort
*
*   FUNCTION
*       Return the application-owned broker message port, or NULL.
*
*   INPUTS
*       MsgPort pointer; get only.
*
*   NOTES
*       Keep the application alive. NewInput consumes and replies to its
*       messages. Do not delete the port or compete with the event loop for its
*       messages. A port can exist even when broker creation failed.
*
*/
/****** Application.mui/MUIA_Application_BrokerPri ************
*
*   NAME
*       MUIA_Application_BrokerPri
*
*   FUNCTION
*       Convert the supplied value to BYTE and use it as the broker priority.
*       Default zero.
*
*   INPUTS
*       LONG in the header; initialize and get.
*
*   NOTES
*       The getter returns the stored BYTE value through IPTR. No setter or
*       range validation is provided.
*
*/
/****** Application.mui/MUIA_Application_BrokerHook ************
*
*   NAME
*       MUIA_Application_BrokerHook
*
*   FUNCTION
*       Store a borrowed hook, initially NULL, called for each broker message
*       after default command handling.
*
*   INPUTS
*       Hook pointer; initialize, set, get.
*
*   NOTES
*       CallHookPkt receives the application as object and the CxMsg pointer as
*       message. Its result is ignored. NewInput replies immediately afterward:
*       do not retain or reply to the message. Disable/Enable change Active;
*       Kill sets SIGBREAKF_CTRL_C. Appear/Disappear have no default action.
*
*/
/****** Application.mui/MUIA_Application_UseCommodities ************
*
*   NAME
*       MUIA_Application_UseCommodities
*
*   FUNCTION
*       Request a Commodities broker and message port when commodities.library
*       is available. Default TRUE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       Broker creation failure is tolerated. Inspect Broker if the application
*       requires it; this attribute has no getter or setter.
*
*/
/****** Application.mui/MUIA_Application_UseRexx ************
*
*   NAME
*       MUIA_Application_UseRexx
*
*   FUNCTION
*       Request a public Rexx message port named from the uppercased actual
*       Base, made by OM_NEW. Default TRUE, with or without a Commands table
*       or RexxHook (as MUI 3.8: tests/lean/probe/rexxport.c). The name is
*       not checked against other programs' ports, only against other
*       applications' bases, as on MUI 3.8.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       An explicit UseRexx tag overrides the default regardless of tag order.
*       Optional port creation failure does not fail the application. Setting
*       Commands later does not create a missing port.
*
*/
/****** Application.mui/MUIA_Application_Commands ************
*
*   NAME
*       MUIA_Application_Commands
*
*   FUNCTION
*       Store a borrowed command table terminated by an entry whose mc_Name is
*       NULL. Default NULL.
*
*   INPUTS
*       MUI_Command table pointer; initialize, set, get.
*
*   NOTES
*       Each entry supplies mc_Name, optional ReadArgs mc_Template,
*       mc_Parameters for the IPTR argument array, and a callable mc_Hook. With
*       mc_Template set to MC_TEMPLATE_ID, mc_Parameters is queued as a
*       ReturnID instead and no hook is called. Keep the table, strings, and
*       hooks alive during input processing. A matched hook receives the
*       application and the parsed array, or NULL without a template. Its
*       return value becomes rm_Result1. RexxMsg exposes the current request
*       only during the hook.
*       The command word must match a whole name case-insensitively. The
*       standard commands QUIT, HIDE, DEACTIVATE, SHOW, ACTIVATE, INFO, and
*       HELP take precedence over table entries of the same name.
*       mc_Parameters must cover every template item. Allocation failure
*       replies RC_ERROR and a ReadArgs failure replies -4, both without
*       calling the hook. Parsed argument pointers are valid only during the
*       hook.
*
*/
/****** Application.mui/MUIA_Application_RexxHook ************
*
*   NAME
*       MUIA_Application_RexxHook
*
*   FUNCTION
*       Store a borrowed fallback hook for Rexx commands matched neither by a
*       standard command nor by Commands. Default NULL.
*
*   INPUTS
*       Hook pointer; initialize, set, get.
*
*   NOTES
*       The hook receives the application and the live RexxMsg; its return value
*       becomes rm_Result1. Without it, unmatched requests receive RC_ERROR. The
*       message is replied to after the callback. Keep the hook alive and do not
*       retain or reply to the message yourself.
*
*/
/****** Application.mui/MUIA_Application_RexxMsg ************
*
*   NAME
*       MUIA_Application_RexxMsg
*
*   FUNCTION
*       Return the current Rexx request during a command or fallback hook; NULL
*       outside those callbacks.
*
*   INPUTS
*       RexxMsg pointer; get only.
*
*   NOTES
*       The pointer is borrowed and valid only during the callback. NewInput
*       clears the field before replying to the request.
*
*/
/****** Application.mui/MUIA_Application_RexxString ************
*
*   NAME
*       MUIA_Application_RexxString
*
*   FUNCTION
*       Replace the application-owned copy of the Rexx result string. When
*       RXFF_RESULT is requested and the command succeeds, the reply carries
*       an Argstring created from the copy.
*
*   INPUTS
*       STRPTR; set only.
*
*   NOTES
*       Each incoming command starts without a result string, so set it in
*       every callback that supplies a result. Input must remain valid
*       through the setter. This attribute does not execute an ARexx command.
*
*/
/****** Application.mui/MUIA_Application_DiskObject ************
*
*   NAME
*       MUIA_Application_DiskObject
*
*   FUNCTION
*       Store a borrowed icon used when iconifying. Default NULL; a missing icon
*       triggers fallback lookup when iconification is requested.
*
*   INPUTS
*       DiskObject pointer; initialize, set, get.
*
*   NOTES
*       The application does not free a supplied DiskObject. Keep it alive while
*       an AppIcon uses it. Iconification writes do_CurrentX and do_CurrentY to
*       NO_ICON_POSITION in the supplied object. Replacing the attribute does
*       not rebuild an already installed AppIcon.
*
*/
/****** Application.mui/MUIA_Application_Iconified ************
*
*   NAME
*       MUIA_Application_Iconified
*
*   FUNCTION
*       Treat exactly 1 as iconify and any other value as restore. On a state
*       change, store the flag, propagate inverse ShowMe to windows, and add or
*       remove a Workbench AppIcon.
*
*   INPUTS
*       BOOL; set and get.
*
*   NOTES
*       Initially FALSE. Fallback icons are application-owned; supplied
*       DiskObject remains borrowed. A no-argument AppIcon activation handled by
*       NewInput restores the application.
*
*   BUGS
*       The flag and window visibility are changed before icon creation
*       succeeds. Failed fallback lookup or AddAppIcon does not roll back the
*       iconified state; TRUE is not proof of a visible AppIcon.
*
*/
/****** Application.mui/MUIA_Application_DoubleStart ************
*
*   NAME
*       MUIA_Application_DoubleStart
*
*   FUNCTION
*       A duplicate SingleTask construction queues a setter tag with TRUE on the
*       existing application, which can trigger Notify registrations.
*
*   INPUTS
*       BOOL event; declared get only.
*
*   NOTES
*       No persistent DoubleStart field is stored by Application.
*
*   BUGS
*       The getter returns TRUE without writing the supplied storage. It cannot
*       be used to read a defined Boolean value; use the notification event.
*
*/
/****** Application.mui/MUIA_Application_ForceQuit ************
*
*   NAME
*       MUIA_Application_ForceQuit
*
*   FUNCTION
*       TRUE when the application should quit without asking. Initially
*       FALSE. The ARexx QUIT command sets it from its FORCE switch before
*       it queues MUIV_Application_ReturnID_Quit.
*
*   INPUTS
*       BOOL; initialize, set, get.
*
*   NOTES
*       Documented by MUI as get-only, but MUI 3.9 accepts setting it and
*       IBrowse sets it from its own ARexx QUIT FORCE.
*
*/
/****** Application.mui/MUIA_Application_HelpFile ************
*
*   NAME
*       MUIA_Application_HelpFile
*
*   FUNCTION
*       Store a borrowed help-file pointer, initially NULL.
*
*   INPUTS
*       STRPTR; initialize, set, get.
*
*   NOTES
*       MUIM_Application_ShowHelp opens it when called without a name.
*       Keep the string alive while stored; it is not copied or freed.
*
*/
/****** Application.mui/MUIA_Application_MenuAction ************
*
*   NAME
*       MUIA_Application_MenuAction
*
*   FUNCTION
*       Store and return the last application menu-action value, initially zero.
*       The window menu-pick path sets it from the selected item's UserData.
*
*   INPUTS
*       ULONG; declared get only; also accepted by OM_SET.
*
*   NOTES
*       The setter forwards the tag to Notify, so repeated values can trigger
*       notifications. The same window path also queues ReturnID. Values are
*       converted to the ULONG field; this is not pointer-sized storage on
*       64-bit targets.
*
*/
/****** Application.mui/MUIA_Application_Menustrip ************
*
*   NAME
*       MUIA_Application_Menustrip
*
*   FUNCTION
*       Supply the application-wide menu tree, initially NULL. The constructor
*       connects it to this application, and application disposal disposes it.
*
*   INPUTS
*       Object pointer; initialize; getter also implemented.
*
*   NOTES
*       Ownership transfers when the constructor stores the tag. Do not attach
*       the same owned object elsewhere. There is no setter for replacement. The
*       getter returns a borrowed pointer.
*
*/
/****** Application.mui/MUIA_Application_Sleep ************
*
*   NAME
*       MUIA_Application_Sleep
*
*   FUNCTION
*       Set Window_Sleep to TRUE for each current window when the value is
*       nonzero, or FALSE when it is zero.
*
*   INPUTS
*       BOOL; set only.
*
*   NOTES
*       No application-level sleep flag is stored and no getter is implemented.
*       Newly added windows do not inherit a remembered application sleep state.
*       Callback-driven membership changes during this traversal are not
*       protected.
*
*/
/****** Application.mui/MUIA_Application_Window ************
*
*   NAME
*       MUIA_Application_Window
*
*   FUNCTION
*       Append a non-NULL Window-compatible child through OM_ADDMEMBER and
*       connect it to the application.
*
*   INPUTS
*       Object pointer; initialize only; repeatable.
*
*   NOTES
*       The application owns attached windows. A NULL Window tag marks
*       construction failure; after processing tags, disposal removes and
*       disposes attached windows. Earlier construction failure can occur before
*       child tags are processed. Runtime changes use OM_ADDMEMBER and
*       OM_REMMEMBER. Neither operation validates existing membership or
*       propagates child failure results.
*
*/
/****** Application.mui/MUIA_Application_WindowList ************
*
*   NAME
*       MUIA_Application_WindowList
*
*   FUNCTION
*       Return the borrowed child list of the internal window Family.
*
*   INPUTS
*       Pointer to an Exec list; get only.
*
*   NOTES
*       Use NextObject for traversal. Do not free or modify the links directly.
*       Keep the application and membership stable during iteration; the list
*       becomes invalid at disposal.
*
*/
/****** Application.mui/MUIA_Application_UsedClasses ************
*
*   NAME
*       MUIA_Application_UsedClasses
*
*   FUNCTION
*       For each class name, call MUI_GetClass and, if it succeeds,
*       MUI_FreeClass. A NULL array is ignored.
*
*   INPUTS
*       NULL-terminated STRPTR array; initialize only.
*
*   NOTES
*       No class list is retained or returned. Individual failures do not abort
*       construction. Keep the array and strings valid during construction; this
*       is not a guarantee that every named class remains loaded.
*
*/
/****** Application.mui/MUIA_Application_Configdata ************
*
*   NAME
*       MUIA_Application_Configdata
*
*   FUNCTION
*       Queue MUIM_Application_SetConfigdata on this application with the
*       supplied pointer. This setter does not immediately install the object.
*
*   INPUTS
*       Object pointer; set only.
*
*   NOTES
*       Supply a live Configdata-compatible object and keep it alive until the
*       queued method adopts it. Application disposal destroys the installed
*       object. The old configuration remains active until execution.
*
*   BUGS
*       A replaced configuration object is never disposed (see
*       MUIM_Application_SetConfigdata).
*       Queue failure is ignored by OM_SET, and application disposal discards
*       a still-queued installation without disposing the supplied object. Do
*       not infer successful transfer of ownership from the setter return
*       value. Use the direct method when controlling installation
*       synchronously.
*
*/
/****** Application.mui/MUIA_Application_SearchWinId ************
*
*   NAME
*       MUIA_Application_SearchWinId
*
*   FUNCTION
*       Store the window identifier used by the next GetWinPos query. Initial
*       value zero.
*
*   INPUTS
*       ULONG; set only.
*
*   NOTES
*       This is mutable shared query state, not a parameter to GetWinPos.
*       Serialize the set/get pair. Identifier zero is never searched.
*
*/
/****** Application.mui/MUIA_Application_SetWinPos ************
*
*   NAME
*       MUIA_Application_SetWinPos
*
*   FUNCTION
*       Copy a position record into the first matching occupied slot or the
*       first empty slot. An occupied slot has nonzero w1. A record with zero
*       w1 instead removes the matching slot and moves later records down. A
*       zero id is ignored.
*
*   INPUTS
*       Pointer to struct windowpos; set only.
*
*   NOTES
*       Supply a non-NULL readable record. Up to 299 records are kept; the
*       table is allocated with the first record and grows by eight. A full
*       table, or failed growth, silently ignores a new ID. The record is
*       copied, not retained.
*
*/
/****** Application.mui/MUIA_Application_GetWinPos ************
*
*   NAME
*       MUIA_Application_GetWinPos
*
*   FUNCTION
*       Find SearchWinId in occupied slots until the first empty slot and return
*       a borrowed record pointer, or NULL when absent or the query ID is zero.
*
*   INPUTS
*       Pointer to struct windowpos; get only.
*
*   NOTES
*       The getter returns TRUE even for NULL. Records live inside the
*       application and can change through SetWinPos or CopyWinPosToApp; do not
*       free them.
*
*/
/****** Application.mui/MUIA_Application_GetWinPosSize ************
*
*   NAME
*       MUIA_Application_GetWinPosSize
*
*   FUNCTION
*       Count occupied records until the first zero w1 in the first 299 slots.
*       Return zero for none, otherwise sizeof(LONG) plus the record bytes, and
*       store that size in the serialized header.
*
*   INPUTS
*       LONG byte count; get only.
*
*   NOTES
*       This getter writes the serialized header. Call it before using GetWinPosAddr to
*       obtain a current serialized header; there is no lock around the pair.
*
*/
/****** Application.mui/MUIA_Application_GetWinPosAddr ************
*
*   NAME
*       MUIA_Application_GetWinPosAddr
*
*   FUNCTION
*       Return a borrowed pointer to the LONG size header immediately
*       preceding the internal position records (a shared empty header
*       while none are stored).
*
*   INPUTS
*       APTR; get only.
*
*   NOTES
*       The payload begins with a 32-bit LONG size even on 64-bit systems,
*       followed by native struct windowpos records. Query GetWinPosSize first.
*       No copy or byte-order conversion is made. Keep the application alive and
*       prevent position changes while consuming the bytes.
*
*/
/****** Application.mui/MUIA_Application_CopyWinPosToApp ************
*
*   NAME
*       MUIA_Application_CopyWinPosToApp
*
*   FUNCTION
*       If non-NULL, read the first LONG as a byte count and copy the header
*       and the whole records it covers (at most 299) into the application's
*       table, which grows to hold them. NULL does nothing.
*
*   INPUTS
*       CONST_APTR; set only.
*
*   NOTES
*       The caller retains the source. Only use a trusted native-format block
*       with a valid nonnegative size, readable source extent, and size no
*       greater than the LONG header plus the 300-record destination array.
*
*   BUGS
*       There is no size, sign, format, or record validation. A malformed block
*       can overwrite instance data; this is not a checked import interface.
*
*/
/****** Application.mui/MUIA_Application_DropObject ************
*
*   NAME
*       MUIA_Application_DropObject
*
*   FUNCTION
*       Store a borrowed object that receives MUIA_AppMessage when icons are
*       dropped on the application's AppIcon. Default NULL.
*
*   INPUTS
*       Object pointer; initialize and set.
*
*   NOTES
*       NewInput sets MUIA_AppMessage synchronously and replies to the
*       AppMessage afterwards; do not retain it. Keep the object alive while
*       stored. The getter has no matching case.
*
*/
/****** Application.mui/MUIA_Application_MenuHelp ************
*
*   NAME
*       MUIA_Application_MenuHelp
*
*   FUNCTION
*       This attribute is declared but has no Application implementation.
*
*   INPUTS
*       ULONG; declared get only.
*
*   NOTES
*       The constructor ignores it and the getter has no matching case. Setter
*       tags are forwarded to Notify without an Application state change.
*
*/
/****** Application.mui/MUIA_Application_Menu ************
*
*   NAME
*       MUIA_Application_Menu
*
*   FUNCTION
*       Build the application menustrip from a borrowed NewMenu array with
*       MUIO_MenustripNM when no Menustrip is supplied. Default NULL.
*
*   INPUTS
*       NewMenu pointer; declared initialize and get only when MUI_OBSOLETE is
*       enabled.
*
*   NOTES
*       A supplied Menustrip takes precedence and the array is then unused.
*       Failure to build the menustrip fails construction. The built menustrip
*       is owned like a supplied one. The getter returns the borrowed array.
*       Setter tags are forwarded to Notify without an Application state
*       change.
*
*/

#include "native_version.h"




/*
 * Allocates an MethodQueue Method
 */
/* Allocate one cleared queue node plus count IPTR words and initialize the
 * embedded message pointer. Return owned storage or NULL. count must be
 * nonnegative and fit the allocation arithmetic; this helper does not
 * validate it.
 */
struct MQNode *CreateMQNode(LONG count)
{
    struct MQNode *mq = zune_mem_alloc(sizeof(struct MQNode)
        + count * sizeof(IPTR));

    if (mq)
    {
        mq->mq_Count = count;
        mq->mq_Msg = (IPTR *)(mq + 1);
    }
    return mq;
}

/*
 * Free an IQ Method got from CreateIQMethod()
 */
void FreeWindowList(struct List *windows)
{
    struct Node *node;

    if (!windows)
        return;
    while ((node = RemHead(windows)))
        FreeVec(node);
    FreeVec(windows);
}

/* Free an owned queue-node allocation after it has been removed or
 * executed. Do not free its borrowed destination or referenced argument
 * storage. No result is returned.
 */
void DeleteMQNode(struct MQNode *mq)
{
    /* Only internal reopen requests transfer ownership of their arguments.
     * Ordinary PushMethod argument pointers remain the caller's property.
     */
    if ((ULONG)mq->mq_Count & MQF_OWNED_WINDOWS)
        FreeWindowList((struct List *)mq->mq_Msg[1]);
    zune_mem_free(mq, sizeof(struct MQNode)
        + (mq->mq_Count & 0xffff) * sizeof(IPTR));
}

void EnqueueMethod(struct MUI_ApplicationData *data, struct MQNode *mq)
{
    ObtainSemaphore(&data->app_MethodSemaphore);
    AddTail((struct List *)&data->app_MethodQueue, (struct Node *)mq);
    ReleaseSemaphore(&data->app_MethodSemaphore);
    Signal(data->app_Task,
        1L << data->app_GlobalInfo.mgi_WindowsPort->mp_SigBit);
}




/* Allocate a cleared return-ID node, store the ULONG value, and return
 * owned storage or NULL. data is unused; insertion is the caller's
 * responsibility.
 */
static struct RIDNode *CreateRIDNode(struct MUI_ApplicationData *data,
    ULONG retid)
{
    struct RIDNode *rid;

    if ((rid = zune_mem_alloc(sizeof(struct RIDNode))))
    {
        rid->rid_Value = retid;
    }
    return rid;
}


/* Free a removed owned return-ID node. data is unused. No result or
 * retained state is produced.
 */
static void DeleteRIDNode(struct MUI_ApplicationData *data,
    struct RIDNode *rid)
{
    zune_mem_free(rid, sizeof(struct RIDNode));
}


/**************************************************************************
 Process a pushed method.
**************************************************************************/
/* Remove one FIFO node while holding the method semaphore, release the
 * semaphore, execute the message on its borrowed destination, and free the
 * node. Return TRUE if work was executed, FALSE if the queue was empty. The
 * caller must ensure destination and application lifetime; method results
 * are discarded.
 */
static BOOL application_do_pushed_method(struct MUI_ApplicationData *data)
{
    struct MQNode *mq;

    ObtainSemaphore(&data->app_MethodSemaphore);

    if ((mq = (struct MQNode *)RemHead((struct List *)&data->app_MethodQueue)))
    {
        ReleaseSemaphore(&data->app_MethodSemaphore);

        /* OpenWindows consumes its list when called. A discarded node,
         * instead, retains ownership for DeleteMQNode to release it.
         */
        mq->mq_Count &= ~MQF_OWNED_WINDOWS;
        DoMethodA(mq->mq_Dest, (Msg) mq->mq_Msg);
        DeleteMQNode(mq);
        return TRUE;
    }
    ReleaseSemaphore(&data->app_MethodSemaphore);
    return FALSE;
}

/* Search tracked applications under ZuneSemaphore by exact strcmp of non-
 * NULL Base strings. Return a borrowed matching application or NULL. base
 * must be readable and NUL-terminated. Releasing the semaphore does not
 * retain the returned object.
 */
static Object *find_application_by_base(struct IClass *cl, Object *obj,
    STRPTR base);

/* The application's base name in portname (255 bytes), returned as a
 * copy (NULL without memory): MUI 3.8 gives it back in upper case, as the
 * ARexx port's name, with .1, .2 unless single task
 * (tests/lean/probe/appbase.c); AmTelnet titles its window with it. Not
 * inlined, the buffer the caller's: OM_NEW's frame stays as it was. */
static __attribute__((noinline)) STRPTR AppBaseName(struct IClass *cl,
    Object *obj, char *portname, CONST_STRPTR base, BOOL single)
{
    char *c;
    LONG i;

    for (i = 1; i < 1000; i++)
    {
        if (single)
            snprintf(portname, 255, "%s", base);
        else
            snprintf(portname, 255, "%s.%d", base, (int)i);
        for (c = portname; *c; c++)
            *c = toupper(*c);
        if (single || !find_application_by_base(cl, obj, portname))
            break;
    }
    return StrDup(portname);
}

static Object *find_application_by_base(struct IClass *cl, Object *obj,
    STRPTR base)
{
    struct MUIMasterBase_intern *intZuneBase = (struct MUIMasterBase_intern *)MUIMasterBase;
    struct TrackingNode *tn;
    Object *retval = NULL;

    ObtainSemaphore(&intZuneBase->ZuneSemaphore);
    ForeachNode(&intZuneBase->Applications, tn)
    {
        STRPTR tn_base = "";

        get(tn->tn_Application, MUIA_Application_Base, &tn_base);

        if (tn_base && (strcmp(base, tn_base)) == 0)
        {
            retval = tn->tn_Application;
            break;
        }
    }
    ReleaseSemaphore(&intZuneBase->ZuneSemaphore);

    return retval;
}

/**************************************************************************
 Config file notification

 ENV:OpenMUI/<appbase>.prefs is watched. Whenever it is written (e.g. by the
 MUI Settings window, sys:prefs/OpenMUI) a fresh Configdata is built from it
 and handed to the application, so the new settings are applied
 immediately without restarting the application.
**************************************************************************/
/* Construct a Configdata bound to this application and queue its
 * installation through the Configdata attribute if allocation succeeds.
 * Return no status. A queue failure is not checked and can leave the new
 * configuration unowned.
 */
/* The application's windows (the Family's list); NULL without memory */
struct MinList *app_windows(struct MUI_ApplicationData *data)
{
    struct MinList *children = NULL;

    get(data->app_WindowFamily, MUIA_Family_List, &children);
    return children;
}

/* One method with one argument on every window */
void app_each_window(struct MUI_ApplicationData *data, ULONG method,
    IPTR arg)
{
    struct MinList *children = app_windows(data);
    Object *cstate, *child;

    if (!children)
        return;
    cstate = (Object *)children->mlh_Head;
    while ((child = NextObject(&cstate)))
        DoMethod(child, method, arg);
}

static void Application_ReloadConfigdata(struct IClass *cl, Object *obj)
{
    Object *cfg;

    cfg = MUI_NewObject(MUIC_Configdata,
        MUIA_Configdata_Application, obj, MUIA_Configdata_SignalPort,
        ((struct MUI_ApplicationData *)INST_DATA(cl, obj))
            ->app_GlobalInfo.mgi_WindowsPort, TAG_DONE);
    if (cfg)
        set(obj, MUIA_Application_Configdata, cfg);
}

/* Create owned port/name resources and start DOS message notification for
 * the bounded ENV:OpenMUI/<base>.prefs path. On local allocation or
 * StartNotify failure, release those resources and leave notification
 * disabled. There is no return status.
 */
static __attribute__((noinline)) void Application_SetupConfigNotify(
    struct IClass *cl, Object *obj)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    char name[255];

    if (!data->app_Base)
        return;

    data->app_CfgNotifyName = NULL;
    data->app_CfgNotifyPort =
        ZuneCreateSharedPort(data->app_GlobalInfo.mgi_WindowsPort);
    if (!data->app_CfgNotifyPort)
        return;

    snprintf(name, sizeof(name), "ENV:OpenMUI/%s.prefs", data->app_Base);
    data->app_CfgNotifyName = StrDup(name);
    if (!data->app_CfgNotifyName)
    {
        ZuneDeleteSharedPort(data->app_CfgNotifyPort);
        data->app_CfgNotifyPort = NULL;
        return;
    }

    memset(&data->app_CfgNotifyRequest, 0, sizeof(struct NotifyRequest));
    data->app_CfgNotifyRequest.nr_Name = data->app_CfgNotifyName;
    data->app_CfgNotifyRequest.nr_Flags = NRF_SEND_MESSAGE;
    data->app_CfgNotifyRequest.nr_stuff.nr_Msg.nr_Port =
        data->app_CfgNotifyPort;

    if (!StartNotify(&data->app_CfgNotifyRequest))
    {
        FreeVec(data->app_CfgNotifyName);
        data->app_CfgNotifyName = NULL;
        ZuneDeleteSharedPort(data->app_CfgNotifyPort);
        data->app_CfgNotifyPort = NULL;
    }
}

/* The prefs file notification and the Configdata's public screen file
 * notification each cost a DOS handler round trip (about 4 ms on a
 * 68020). They matter only to an application that waits for input, so
 * they start at the first MUIM_Application_NewInput rather than at
 * construction; create/dispose cycles and MUI_Request-only uses then pay
 * nothing. MUI 3.8 starts no notification at all. */
static void Application_StartNotifications(struct IClass *cl, Object *obj)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    if (data->app_NotifyStarted)
        return;
    data->app_NotifyStarted = TRUE;
    Application_SetupConfigNotify(cl, obj);
    if (data->app_GlobalInfo.mgi_Configdata)
        DoMethod(data->app_GlobalInfo.mgi_Configdata,
            MUIM_Configdata_SetupNotify);
    /* Catch a prefs file written before the notification started */
    if (data->app_CfgNotifyName
        && zune_file_written_since(data->app_CfgNotifyName, &data->app_Created))
        Application_ReloadConfigdata(cl, obj);
}

/* The application's base name (AppBaseName: the given one, or a unique
 * one for a program that may run more than once) and, for a single-task
 * program, the other running one told of the double start. Leaves
 * app_Base NULL when the application must not be made. Out of line for
 * its port name buffer: OM_NEW's frame. */
static __attribute__((noinline)) void app_make_base(struct IClass *cl,
    Object *obj, struct MUI_ApplicationData *data)
{
    char portname[255];

    data->app_Base = AppBaseName(cl, obj, portname, data->app_Base,
        data->app_SingleTask);
    if (data->app_Base && data->app_SingleTask)
    {
        Object *other_app;

        ObtainSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
        if ((other_app = find_application_by_base(cl, obj, data->app_Base)))
        {
            // FIXME: Lifetime of other_app until the queued PushMethod
            // executes is not established here.
            DoMethod(other_app, MUIM_Application_PushMethod,
                (IPTR) other_app, 3, MUIM_Set, MUIA_Application_DoubleStart,
                TRUE);
            FreeVec(data->app_Base);
            data->app_Base = NULL;
        }
        ReleaseSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    }
}

/* The commodities broker (MUIA_Application_UseCommodities) with the
 * settings' iconification hotkey (System page), which iconifies and shows
 * the program again, as on MUI 3.8. Out of line for the NewBroker. */
static __attribute__((noinline)) void app_make_broker(
    struct MUI_ApplicationData *data)
{
    struct NewBroker nb;

    data->app_BrokerPort = CreateMsgPort();
    if (!data->app_BrokerPort)
        return;

    nb.nb_Version = NB_VERSION;
    nb.nb_Name = data->app_Title ? data->app_Title : (STRPTR) "Unnamed";
    nb.nb_Title = data->app_Version ? data->app_Version : (STRPTR) "Unnamed";
    nb.nb_Descr = data->app_Description ? data->app_Description : (STRPTR) "?";
    nb.nb_Unique = 0;
    nb.nb_Flags = COF_SHOW_HIDE;
    nb.nb_Pri = data->app_BrokerPri;
    nb.nb_Port = data->app_BrokerPort;
    nb.nb_ReservedChannel = 0;

    if (strncmp(nb.nb_Title, "$VER: ", 6) == 0)
        nb.nb_Title += 6;

    data->app_Broker = CxBroker(&nb, 0);

    if (data->app_Broker)
    {
        CONST_STRPTR hk = data->app_GlobalInfo.mgi_Prefs->iconification_hotkey;
        CxObj *filter;

        if (hk && hk[0] && (filter = CxFilter(hk)))
        {
            AttachCxObj(filter,
                CxSender(data->app_BrokerPort, ICONIFY_HOTKEY_ID));
            AttachCxObj(filter, CxTranslate(NULL));
            AttachCxObj(data->app_Broker, filter);
        }
        if (data->app_Active)
            ActivateCxObj(data->app_Broker, 1);
    }
}

/* At OM_DISPOSE: the final window positions into the application's own
 * file, ENV:OpenMUI/<base>.prefs (the settings' window position mode 1), and
 * ENVARC: too so they survive a reboot (mode 2). Only the positions go
 * there, as with MUI: writing the settings in use there made the
 * application ignore every later change of the global settings. Windows
 * closed by the application before disposing it are already snapshotted
 * in UndisplayWindow; apps that dispose the application with its windows
 * still open (iGame) would otherwise lose their position. */
static __attribute__((noinline)) void app_save_positions(
    struct MUI_ApplicationData *data)
{
    LONG positionmode = data->app_GlobalInfo.mgi_Prefs->window_position;
    char filename[255];
    struct MinList *children;
    Object *cstate, *child;

    if (positionmode < 1)
        return;
    if ((children = app_windows(data)))
    {
        cstate = (Object *) children->mlh_Head;
        while ((child = NextObject(&cstate)))
            ZuneRememberWindowPosition(child);
    }
    /* No position was ever stored or read: the file would be written as
     * it is (an empty one where there was none), a read and a write of
     * ENV: at every quit for nothing */
    if (!data->app_WinPos)
        return;
    snprintf(filename, 255, "ENV:OpenMUI/%s.prefs", data->app_Base);
    DoMethod(data->app_GlobalInfo.mgi_Configdata,
        MUIM_Configdata_SaveWindowPositions, (IPTR) filename);
    if (positionmode == 2)
    {
        snprintf(filename, 255, "ENVARC:OpenMUI/%s.prefs", data->app_Base);
        DoMethod(data->app_GlobalInfo.mgi_Configdata,
            MUIM_Configdata_SaveWindowPositions, (IPTR) filename);
    }
}

/**************************************************************************
 OM_NEW
**************************************************************************/
/* Construct Notify and application resources, parse initialization tags,
 * attach owned windows, and register the application. Return the object or
 * attempt cleanup and return zero on required failure. Optional broker,
 * Rexx port, watcher, and generated-version failures are tolerated. Early
 * cleanup assumes preferences exist whenever Base is non-NULL; see the
 * class BUGS contract.
 */
/* Application's plain attributes: the field each is kept in and how it may
 * be used (I: OM_NEW, S: OM_SET, G: OM_GET); one table for the three.
 * The others (windows, positions, iconify, ...) are handled where they
 * are used. */
#define AA_I 1
#define AA_S 2
#define AA_G 4
#define AA_BOOL 8       /* a BOOL, stored as TRUE or FALSE */
#define AA_BYTE 16      /* a BYTE, read back sign-extended */
struct app_attr
{
    ULONG id;
    UWORD offset;
    UWORD flags;
};
#define AA(id, field, flags) \
    { id, offsetof(struct MUI_ApplicationData, field), flags }
static const struct app_attr app_attrs[] = {
    AA(MUIA_Application_Author, app_Author, AA_I | AA_G),
    AA(MUIA_Application_Base, app_Base, AA_G),
    AA(MUIA_Application_Copyright, app_Copyright, AA_I | AA_G),
    AA(MUIA_Application_Description, app_Description, AA_I | AA_G),
    AA(MUIA_Application_HelpFile, app_HelpFile, AA_I | AA_S | AA_G),
    AA(MUIA_Application_Title, app_Title, AA_I | AA_G),
    AA(MUIA_Application_Version, app_Version, AA_I | AA_G),
    AA(MUIA_Application_Version_Number, app_Version_Number, AA_I | AA_G),
    AA(MUIA_Application_Version_Date, app_Version_Date, AA_I | AA_G),
    AA(MUIA_Application_Version_Extra, app_Version_Extra, AA_I | AA_G),
    AA(MUIA_Application_Menu, app_LegacyMenu, AA_I | AA_G),
    AA(MUIA_Application_Menustrip, app_Menustrip, AA_I | AA_G),
    AA(MUIA_Application_BrokerPri, app_BrokerPri, AA_I | AA_G | AA_BYTE),
    AA(MUIA_Application_BrokerHook, app_BrokerHook, AA_I | AA_S | AA_G),
    AA(MUIA_Application_Active, app_Active, AA_I | AA_G | AA_BOOL),
    AA(MUIA_Application_Commands, app_Commands, AA_I | AA_S | AA_G),
    AA(MUIA_Application_RexxHook, app_RexxHook, AA_I | AA_S | AA_G),
    /* Documented as get-only, but MUI 3.9 accepts it and IBrowse sets it
     * from its own ARexx QUIT FORCE before quitting. */
    AA(MUIA_Application_ForceQuit, app_ForceQuit,
        AA_I | AA_S | AA_G | AA_BOOL),
    AA(MUIA_Application_DropObject, app_DropObject, AA_I | AA_S),
    AA(MUIA_Application_DiskObject, app_DiskObject, AA_I | AA_S | AA_G),
    AA(MUIA_Application_Iconified, app_Iconified, AA_G | AA_BOOL),
    AA(MUIA_Application_MenuAction, app_MenuAction, AA_S | AA_G),
    AA(MUIA_Application_SearchWinId, searchwinid, AA_S),
    AA(MUIA_Application_Broker, app_Broker, AA_G),
    AA(MUIA_Application_BrokerPort, app_BrokerPort, AA_G),
    AA(MUIA_Application_RexxMsg, app_RexxMsg, AA_G),
};

/* The table's entry of id if it may be used as use says, else NULL */
static const struct app_attr *app_attr(ULONG id, UWORD use)
{
    const struct app_attr *a;

    for (a = app_attrs; a < app_attrs + sizeof(app_attrs) / sizeof(*a); a++)
        if (a->id == id)
            return a->flags & use ? a : NULL;
    return NULL;
}

static void app_attr_store(struct MUI_ApplicationData *data,
    const struct app_attr *a, IPTR value)
{
    UBYTE *field = (UBYTE *)data + a->offset;

    if (a->flags & AA_BOOL)
        *(BOOL *)field = value ? TRUE : FALSE;
    else if (a->flags & AA_BYTE)
        *(BYTE *)field = value;
    else
        *(IPTR *)field = value;
}

static IPTR Application__OM_SET(struct IClass *cl, Object *obj,
    struct opSet *msg);

/* The "iconify at startup" setting, applied by this class's own OM_SET:
 * MUI 3.8's subclass sees no OM_SET during OM_NEW with the setting on
 * (tests/lean/probe/newset.c), and a subclass's instance data is not set
 * up yet. Its own frame: OM_NEW's stays as it was. */
static __attribute__((noinline)) void app_iconify_at_start(
    register struct IClass *cl __asm("a0"), register Object *obj __asm("a1"))
{
    static const struct TagItem tags[] =
        { { MUIA_Application_Iconified, TRUE }, { TAG_DONE, 0 } };
    struct opSet m = { OM_SET, (struct TagItem *)tags, NULL };

    Application__OM_SET(cl, obj, &m);
}

static IPTR Application__OM_NEW(struct IClass *cl, Object *obj,
    struct opSet *msg)
{
    struct MUI_ApplicationData *data;
    struct TagItem *tags, *tag;
    BOOL bad_childs = FALSE;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
    {
        zune_dispose_children(msg->ops_AttrList);
        return FALSE;
    }

    /* Initial local instance data */
    data = INST_DATA(cl, obj);

    /* init input handler list */
    NewList((struct List *)&(data->app_IHList));
    NewList((struct List *)&data->app_Timers);

    /* init input queue */
    NewList((struct List *)&(data->app_MethodQueue));

    /* init return ids queue */
    NewList((struct List *)&(data->app_ReturnIDQueue));

    /* window list */
    data->app_WindowFamily = MUI_NewObjectA(MUIC_Family, NULL);
    if (!data->app_WindowFamily)
    {
        goto fail;
    }

    data->app_GlobalInfo.mgi_ApplicationObject = obj;
    /* the MUI Zune behaves as for this program (identity.c) */
    MGI_IDENTITY(&data->app_GlobalInfo) = zune_task_identity();
    if (!(data->app_GlobalInfo.mgi_WindowsPort = CreateMsgPort()))
    {
        goto fail;
    }

    data->app_Task = FindTask(NULL);

    /* Parse prefs */
    data->app_SingleTask =
        (BOOL) GetTagData(MUIA_Application_SingleTask, FALSE,
        msg->ops_AttrList);
    data->app_Base =
        (STRPTR) GetTagData(MUIA_Application_Base, (IPTR) "UNNAMED",
        msg->ops_AttrList);
    if (!data->app_Base || strpbrk(data->app_Base, ":/()#?*,"))
    {
        data->app_Base = NULL;  /* don't remove */
        goto fail;
    }

    app_make_base(cl, obj, data);
    if (!data->app_Base)
    {
        goto fail;
    }

    data->app_GlobalInfo.mgi_Configdata =
        MUI_NewObject(MUIC_Configdata, MUIA_Configdata_Application, obj,
        MUIA_Configdata_SignalPort, data->app_GlobalInfo.mgi_WindowsPort,
        TAG_DONE);
    if (!data->app_GlobalInfo.mgi_Configdata)
    {
        goto fail;
    }
    get(data->app_GlobalInfo.mgi_Configdata, MUIA_Configdata_ZunePrefs,
        &data->app_GlobalInfo.mgi_Prefs);

    /* The prefs file notification starts when the application first
     * waits for input (Application_StartNotifications); see there. */
    DateStamp(&data->app_Created);

//    D(bug("muimaster.library/application.c: Message Port created at 0x%p\n",
//    data->app_GlobalInfo.mgi_WindowPort));

    /* Setup timer stuff; MUI 3.9 uses one signal for all internal ports */
    if (!(data->app_TimerPort =
            ZuneCreateSharedPort(data->app_GlobalInfo.mgi_WindowsPort)))
    {
        goto fail;
    }

    if (!(data->app_TimerReq =
            (struct timerequest *)CreateIORequest(data->app_TimerPort,
                sizeof(struct timerequest))))
    {
        goto fail;
    }

    if (OpenDevice(TIMERNAME, UNIT_VBLANK,
            (struct IORequest *)data->app_TimerReq, 0))
    {
        goto fail;
    }

    InitSemaphore(&data->app_MethodSemaphore);

    muiNotifyData(obj)->mnd_GlobalInfo = &data->app_GlobalInfo;

    /* parse initial taglist */

    data->app_Active = 1;
    data->app_Title = "Unnamed";
    data->app_Version = "Unnamed 0.0";
    data->app_Description = "?";

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        const struct app_attr *a = app_attr(tag->ti_Tag, AA_I);

        if (a)
            app_attr_store(data, a, tag->ti_Data);
        else if (tag->ti_Tag == MUIA_Application_Window)
        {
            if (tag->ti_Data)
                DoMethod(obj, OM_ADDMEMBER, tag->ti_Data);
            else
                bad_childs = TRUE;
        }
        else if (tag->ti_Tag == MUIA_Application_UsedClasses
            && tag->ti_Data)
        {
            /* MUI 4's list of the classes a program uses (YAM and IBrowse
             * give it on MUI 3 too: in the library): each loaded, given
             * back */
            STRPTR *list;

            for (list = (STRPTR *)tag->ti_Data; *list; list++)
            {
                struct IClass *icl = MUI_GetClass(*list);
                if (icl)
                    MUI_FreeClass(icl);
            }
        }
    }

    /* MUI 3.8 makes the port at once, also without Commands or a RexxHook
     * (tests/lean/probe/rexxport.c) */
    data->app_UseRexx =
        GetTagData(MUIA_Application_UseRexx, TRUE, msg->ops_AttrList) ? TRUE : FALSE;

    /* MUIA_Application_Version made of Version_Number and the others
     * (AROS's attributes, the module) */
    if (data->app_Version == NULL
        && data->app_Title != NULL && data->app_Version_Number != NULL)
        zune_app_version(data);

    if (bad_childs)
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

    if (GetTagData(MUIA_Application_UseCommodities, TRUE,
            msg->ops_AttrList) && ZUNE_CX())
        app_make_broker(data);

    /* rexxsyslib.library is opened when the first message arrives. No
     * port when the settings switch ARexx off (System page). */
    if (data->app_UseRexx
        && data->app_GlobalInfo.mgi_Prefs->interfaces_enable_arexx)
    {
        data->app_RexxPort = CreateMsgPort();
        if (data->app_RexxPort)
        {
            data->app_RexxPort->mp_Node.ln_Name = StrDup(data->app_Base);
            if (data->app_RexxPort->mp_Node.ln_Name != NULL)
            {
                D(bug("[MUI] %s is using REXX!\n", data->app_RexxPort->mp_Node.ln_Name));
                char *i;
                for (i = data->app_RexxPort->mp_Node.ln_Name; *i != '\0';
                    i++)
                {
                    *i = toupper(*i);
                }
                AddPort(data->app_RexxPort);
            }
            else
            {
                DeleteMsgPort(data->app_RexxPort);
                data->app_RexxPort = NULL;
            }
        }
    }

    if (data->app_LegacyMenu && !data->app_Menustrip)
    {
        IPTR params[2];
        params[0] = (IPTR)data->app_LegacyMenu;
        params[1] = 0;
        data->app_Menustrip = MUI_MakeObjectA(MUIO_MenustripNM, params);
        if (!data->app_Menustrip)
        {
            CoerceMethod(cl, obj, OM_DISPOSE);
            return 0;
        }
    }
    if (data->app_Menustrip)
        DoMethod(data->app_Menustrip, MUIM_ConnectParent, (IPTR) obj);

    data->app_AppPort =
        ZuneCreateSharedPort(data->app_GlobalInfo.mgi_WindowsPort);
    data->app_GlobalInfo.mgi_AppPort = data->app_AppPort;
    if (data->app_AppPort == NULL)
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

    ObtainSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    data->app_TrackingNode.tn_Application = obj;
    AddTail((struct List *)&MUIMB(MUIMasterBase)->Applications,
        (struct Node *)&data->app_TrackingNode);
    data->app_is_TNode_in_list = TRUE;
    ReleaseSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);

    /* The settings may start every program iconified (System page) */
    if (data->app_GlobalInfo.mgi_Prefs->iconification_on_startup)
        app_iconify_at_start(cl, obj);

    return (IPTR) obj;

fail:
    /* A failure before the tag list is read: the windows and the menustrip
     * it carries are not the application's yet, and no one else would
     * dispose them (MUI's rule: a failed call leaves nothing behind). */
    zune_dispose_children(msg->ops_AttrList);
    CoerceMethod(cl, obj, OM_DISPOSE);
    return 0;
}



/**************************************************************************
 OM_DISPOSE
**************************************************************************/
/* After callbacks and external users have stopped, snapshot and optionally
 * save positions, remove and dispose child windows, and release owned
 * application resources before calling Notify disposal. Return the
 * superclass result. Saving positions can block on I/O; queued methods
 * are discarded.
 */
/* Every message of a port replied; TRUE if there was one */
static BOOL app_drain(struct MsgPort *port)
{
    struct Message *m;
    BOOL any = FALSE;

    while (port && (m = GetMsg(port)))
    {
        ReplyMsg(m);
        any = TRUE;
    }
    return any;
}

static IPTR Application__OM_DISPOSE(struct IClass *cl, Object *obj,
    Msg msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    struct MinNode *n;
    Object *cstate, *child;

    if (data->app_Base)
        app_save_positions(data);

    if (data->app_is_TNode_in_list)
    {
        ObtainSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
        Remove((struct Node *)&data->app_TrackingNode);
        ReleaseSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    }

    if (data->app_WindowFamily)
    {
        /* the first window each time: disposing one may have removed or
         * disposed others */
        for (;;)
        {
            struct MinList *children = app_windows(data);

            if (!children)
                break;
            cstate = (Object *)children->mlh_Head;
            if (!(child = NextObject(&cstate)))
                break;
            DoMethod(obj, OM_REMMEMBER, (IPTR) child);
            MUI_DisposeObject(child);
        }
        MUI_DisposeObject(data->app_WindowFamily);
    }

    if (data->app_Menustrip)
        MUI_DisposeObject(data->app_Menustrip);
    if (data->app_VersionAllocated)
        FreeVec(data->app_Version);
    if (data->app_GlobalInfo.mgi_Configdata)
        MUI_DisposeObject(data->app_GlobalInfo.mgi_Configdata);
    data->app_GlobalInfo.mgi_Configdata = NULL;
    data->app_GlobalInfo.mgi_Prefs = NULL;

    /* commodities */
    if (data->app_Broker)
        DeleteCxObjAll(data->app_Broker);
    if (data->app_BrokerPort)
    {
        app_drain(data->app_BrokerPort);
        DeleteMsgPort(data->app_BrokerPort);
    }

    /* the timer; timers a class did not remove would otherwise stay with
     * timer.device and later be replied to the deleted port */
    if (data->app_TimerReq)
    {
        if (data->app_TimerReq->tr_node.io_Device)
        {
            while ((n = (struct MinNode *)RemHead(
                    (struct List *)&data->app_Timers)))
            {
                struct timerequest_ext *time_ext = (struct timerequest_ext *)
                    ((UBYTE *)n - offsetof(struct timerequest_ext, link));

                if (!CheckIO((struct IORequest *)time_ext))
                    AbortIO((struct IORequest *)time_ext);
                WaitIO((struct IORequest *)time_ext);
                time_ext->ihn->ihn_Node.mln_Pred = NULL;
                FreeVec(time_ext);
            }
            CloseDevice((struct IORequest *)data->app_TimerReq);
        }
        DeleteIORequest((struct IORequest *)data->app_TimerReq);
    }
    ZuneDeleteSharedPort(data->app_TimerPort);

    if (data->app_RexxPort)
    {
        app_drain(data->app_RexxPort);
        RemPort(data->app_RexxPort);
        FreeVec(data->app_RexxPort->mp_Node.ln_Name);
        DeleteMsgPort(data->app_RexxPort);
    }
    FreeVec(data->app_RexxString);

    /* an iconified application's icon and menu item (the module made
     * them, so it is in memory) */
    if (data->app_AppMenu || data->app_AppIcon || data->app_DefaultDiskObject)
        zune_app_iconify(data, obj, FALSE);

    FreeVec(data->app_WinPos);

    if (data->app_AppPort)
    {
        app_drain(data->app_AppPort);
        ZuneDeleteSharedPort(data->app_AppPort);
    }

    if (data->app_CfgNotifyPort)
    {
        EndNotify(&data->app_CfgNotifyRequest);
        ZuneDeleteSharedPort(data->app_CfgNotifyPort);
    }
    FreeVec(data->app_CfgNotifyName);

    DeleteMsgPort(data->app_GlobalInfo.mgi_WindowsPort);

    FreeVec(data->app_Base);

    /* Discard calls to objects being destroyed, including calls queued by
     * their cleanup methods. The list is initialized before any failing
     * constructor step; its semaphore need not exist on those paths. */
    while ((n = (struct MinNode *)RemHead(
                (struct List *)&data->app_MethodQueue)))
        DeleteMQNode((struct MQNode *)n);
    while ((n = (struct MinNode *)RemHead(
                (struct List *)&data->app_ReturnIDQueue)))
        DeleteRIDNode(data, (struct RIDNode *)n);

    /* Never while running on it (a dispose from inside NewInput) */
    if (data->app_SpareStack && !data->app_SpareBusy)
        FreeMem(data->app_SpareStack, ZUNE_STACK_SPARE);

    return DoSuperMethodA(cl, obj, msg);
}


/**************************************************************************
 OM_SET
**************************************************************************/
/* Apply supported attribute operations in tag order, including copying
 * trusted position data, deferred configuration installation, iconification
 * and window broadcasts. Then forward the same tags to Notify and return
 * its result. No general failure status is collected; pointer ownership
 * follows each attribute contract.
 */
/* MUIA_Application_SetWinPos: the record with this id, or the first free
 * one (the table grows by eight records at a time); a record of width 0
 * removes this id's */
static void app_set_winpos(struct MUI_ApplicationData *data,
    struct windowpos *winp)
{
    struct windowpos *wp;
    ULONG i, cap;

    if (!winp->id)
        return;
    if (!winp->w1)
    {
        cap = data->app_WinPos ? data->app_WinPosCap : 0;
        wp = cap ? WINPOS(data) : NULL;
        for (i = 0; i < cap && wp[i].w1; i++)
            if (wp[i].id == winp->id)
            {
                memmove(&wp[i], &wp[i + 1], (cap - i - 1) * sizeof(*winp));
                memset(&wp[cap - 1], 0, sizeof(*winp));
                break;
            }
        return;
    }
    for (i = 0; i < WINPOS_MAX; i++)
    {
        if ((!data->app_WinPos || i >= data->app_WinPosCap)
            && !winpos_grow(data, i + 8))
            break;
        wp = &WINPOS(data)[i];
        if (!wp->w1 || wp->id == winp->id)
        {
            *wp = *winp;
            break;
        }
    }
}

/* MUIA_Application_CopyWinPosToApp: the first LONG is the byte count of
 * the records that follow; the table grows to hold them (at most
 * WINPOS_MAX), the copy never runs past it */
static void app_copy_winpos(struct MUI_ApplicationData *data, LONG *addr)
{
    LONG n;

    if (!addr || *addr <= (LONG)sizeof(LONG))
        return;
    n = (*addr - (LONG)sizeof(LONG)) / (LONG)sizeof(struct windowpos);
    if (n > WINPOS_MAX)
        n = WINPOS_MAX;
    if (n > 0 && winpos_grow(data, n))
        CopyMem(addr, data->app_WinPos,
            sizeof(LONG) + n * sizeof(struct windowpos));
}

/* MUIA_ShowMe to every window. Showing or hiding a window can add or
 * remove other windows (MUIM_Setup/Cleanup), so the list is read again
 * after each and the walk goes on after the last one done (assuming that
 * window stays in the list). */
static void app_show_windows(struct MUI_ApplicationData *data, IPTR show)
{
    Object *curwin, *lastwin = NULL;
    APTR wstate;

    for (;;)
    {
        wstate = (Object *)app_windows(data)->mlh_Head;
        while ((curwin = NextObject(&wstate)) && lastwin)
            if (curwin == lastwin)
            {
                curwin = NextObject(&wstate);
                break;
            }
        if (!curwin)
            break;
        set(curwin, MUIA_ShowMe, show);
        lastwin = curwin;
    }
}

/* One attribute set on every window */
static void app_set_windows(struct MUI_ApplicationData *data, ULONG attr,
    IPTR value)
{
    Object *cstate = (Object *)app_windows(data)->mlh_Head, *child;

    while ((child = NextObject(&cstate)))
        set(child, attr, value);
}

/* OM_SET's attributes that are not a plain field (app_attrs) */
static __attribute__((noinline)) void app_set_other(Object *obj,
    struct MUI_ApplicationData *data, struct TagItem *tag)
{
    IPTR value = tag->ti_Data;

    switch (tag->ti_Tag)
    {
    case MUIA_Application_CopyWinPosToApp:
        app_copy_winpos(data, (LONG *)value);
        break;

    case MUIA_Application_SetWinPos:
        app_set_winpos(data, (struct windowpos *)value);
        break;

    case MUIA_Application_Configdata:
        DoMethod(obj, MUIM_Application_PushMethod, (IPTR) obj, 2,
            MUIM_Application_SetConfigdata, value);
        break;

    case MUIA_Application_Iconified:
        if (data->app_Iconified != (value == 1))
        {
            data->app_Iconified = value == 1;
            nnset(obj, MUIA_ShowMe, !data->app_Iconified);
            /* Workbench's icon and Tools menu item: the module */
            zune_app_iconify(data, obj, data->app_Iconified);
        }
        break;

    case MUIA_ShowMe:
        /* an attribute of an interface Application implements with Area
         * and a few others */
        app_show_windows(data, value);
        break;

    case MUIA_Application_Sleep:
        app_set_windows(data, MUIA_Window_Sleep, value ? TRUE : FALSE);
        break;

    case MUIA_Application_Active:
        data->app_Active = value ? TRUE : FALSE;
        if (data->app_Broker)
            ActivateCxObj(data->app_Broker, data->app_Active);
        break;

    case MUIA_Application_RexxString:
        {
            STRPTR replacement = StrDup((CONST_STRPTR)value);
            if (replacement || !value)
            {
                FreeVec(data->app_RexxString);
                data->app_RexxString = replacement;
            }
        }
        break;
    }
}

static IPTR Application__OM_SET(struct IClass *cl, Object *obj,
    struct opSet *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList;
    struct TagItem *tag;
    const struct app_attr *a;

    while ((tag = NextTagItem(&tags)) != NULL)
    {
        if ((a = app_attr(tag->ti_Tag, AA_S)))
            app_attr_store(data, a, tag->ti_Data);
        else
            app_set_other(obj, data, tag);
    }
    return DoSuperMethodA(cl, obj, (Msg) msg);
}


/*
 * OM_GET
 */
/* Write recognized application fields through writable opg_Storage, or
 * delegate WindowList to the internal Family and unknown IDs to Notify.
 * Pointer results are borrowed. DoubleStart returns TRUE without writing
 * storage; GetWinPosSize updates the serialized size header.
 */
/* OM_GET's attributes into *store; FALSE: not one of Application's (the
 * superclass's, MUIA_Version and MUIA_Revision too: Notify reports the
 * version of the object's class library) */
static __attribute__((noinline)) BOOL app_get(
    register struct MUI_ApplicationData *data __asm("a0"),
    register ULONG id __asm("d0"), register IPTR *store __asm("a1"))
{
    /* No positions stored: an empty serialized block. */
    static const LONG no_winpos = 0;
    const struct app_attr *a = app_attr(id, AA_G);
    ULONG i;

    if (a)
    {
        UBYTE *field = (UBYTE *)data + a->offset;

        *store = a->flags & AA_BOOL ? (IPTR)*(BOOL *)field
            : a->flags & AA_BYTE ? (IPTR)(LONG)*(BYTE *)field
            : *(IPTR *)field;
        return TRUE;
    }
    switch (id)
    {
    case MUIA_Application_DoubleStart:
        return TRUE;

    case MUIA_Application_WindowList:
        return GetAttr(MUIA_Family_List, data->app_WindowFamily, store);

    case MUIA_Application_GetWinPosAddr:
        *store = data->app_WinPos ? (IPTR) data->app_WinPos
            : (IPTR) &no_winpos;
        return TRUE;

    case MUIA_Application_GetWinPosSize:
        /* the records up to the first free one, and the LONG header; the
         * header written too */
        for (i = 0; data->app_WinPos && i < data->app_WinPosCap
            && WINPOS(data)[i].w1; i++)
            ;
        *store = i ? sizeof(LONG) + i * sizeof(struct windowpos) : 0;
        if (data->app_WinPos)
            data->app_WinPos[0] = *store;
        return TRUE;

    case MUIA_Application_GetWinPos:
        /* the record of MUIA_Application_SearchWinId, or NULL */
        *store = 0;
        for (i = 0; data->searchwinid && data->app_WinPos
            && i < data->app_WinPosCap && WINPOS(data)[i].w1; i++)
            if (WINPOS(data)[i].id == data->searchwinid)
            {
                *store = (IPTR)&WINPOS(data)[i];
                break;
            }
        return TRUE;
    }
    return FALSE;
}

static IPTR Application__OM_GET(struct IClass *cl, Object *obj,
    struct opGet *msg)
{
    if (app_get(INST_DATA(cl, obj), msg->opg_AttrID, msg->opg_Storage))
        return TRUE;
    return DoSuperMethodA(cl, obj, (Msg) msg);
}


/**************************************************************************
 OM_ADDMEMBER
**************************************************************************/
/* Forward membership insertion to the internal Family, connect the supplied
 * live Window-compatible child to this application, and return TRUE
 * regardless of child results. The window becomes owned when attached;
 * membership and NULL input are not validated here.
 */
static IPTR Application__OM_ADDMEMBER(struct IClass *cl, Object *obj,
    struct opMember *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    D(bug("Application_AddMember: Adding 0x%p to window member list\n",
            msg->opam_Object));

    DoMethodA(data->app_WindowFamily, (Msg) msg);
    /* Application knows its GlobalInfo, so we can inform window */
    DoMethod(msg->opam_Object, MUIM_ConnectParent, (IPTR) obj);
    return TRUE;
}


/**************************************************************************
 OM_REMMEMBER
**************************************************************************/
/* Disconnect the live attached window, forward removal to the internal
 * Family, and return TRUE regardless of child results. The child is not
 * disposed; the caller must manage its later lifetime.
 */
static IPTR Application__OM_REMMEMBER(struct IClass *cl, Object *obj,
    struct opMember *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    D(bug("Application_RemMember: Removing 0x%p from window member list\n",
            msg->opam_Object));

    DoMethod(msg->opam_Object, MUIM_DisconnectParent);
    DoMethodA(data->app_WindowFamily, (Msg) msg);

    return TRUE;
}



/****** Application.mui/MUIM_Application_AddInputHandler ************
*
*   NAME
*       MUIM_Application_AddInputHandler
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_AddInputHandler,
*       (IPTR)ihnode);
*
*   FUNCTION
*       Register a borrowed MUI_InputHandlerNode for signal callbacks or start a
*       repeating timer when MUIIHNF_TIMER is set.
*
*   INPUTS
*       ihnode - Non-NULL, stable, writable node not already registered. Set
*       ihn_Object to a live target and ihn_Method to the method to invoke.
*       Signal handlers receive no arguments; timer handlers receive zero and
*       the node. For signals set ihn_Signals; for timers set ihn_Millis
*       (UWORD milliseconds).
*
*   RESULT
*       Always TRUE, including timer-request allocation failure.
*
*   NOTES
*       Signal nodes are linked directly into the application list. Timer
*       registration stores an allocated timer-request pointer in
*       ihn_Node.mln_Pred; do not overwrite it. NewInput restarts a completed
*       timer before calling its target. Keep node and target alive until
*       removal has completed.
*
*   BUGS
*       Timer allocation failure leaves no valid registered timer request but
*       still reports TRUE. A later RemInputHandler cannot safely assume success
*       from this return value. No duplicate-registration or interval validation
*       is performed.
*
*/
static IPTR Application__MUIM_AddInputHandler(struct IClass *cl,
    Object *obj, struct MUIP_Application_AddInputHandler *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    if (msg->ihnode->ihn_Flags & MUIIHNF_TIMER)
    {
        struct timerequest_ext *time_ext =
            (struct timerequest_ext *)AllocVec(sizeof(struct
                timerequest_ext), MEMF_PUBLIC);
        if (time_ext)
        {
            /* Store the request inside the input handler, so that we can
             ** remove the inputhandler without problems */
            msg->ihnode->ihn_Node.mln_Pred = (struct MinNode *)time_ext;

            time_ext->treq = *data->app_TimerReq;
            time_ext->treq.tr_node.io_Command = TR_ADDREQUEST;
            time_ext->treq.tr_time.tv_secs = msg->ihnode->ihn_Millis / 1000;
            time_ext->treq.tr_time.tv_micro =
                (msg->ihnode->ihn_Millis % 1000) * 1000;
            time_ext->ihn = msg->ihnode;
            AddTail((struct List *)&data->app_Timers,
                (struct Node *)&time_ext->link);
            SendIO((struct IORequest *)time_ext);
        }
        else
        {
            /* No request: RemInputHandler must not take the stale link
             * for one */
            msg->ihnode->ihn_Node.mln_Pred = NULL;
            return FALSE;
        }
    }
    else
        AddTail((struct List *)&data->app_IHList,
            (struct Node *)msg->ihnode);
    return TRUE;
}


/****** Application.mui/MUIM_Application_RemInputHandler ************
*
*   NAME
*       MUIM_Application_RemInputHandler
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_RemInputHandler,
*       (IPTR)ihnode);
*
*   FUNCTION
*       Unlink a registered signal node, or abort if necessary, wait for, and
*       free its timer request.
*
*   INPUTS
*       ihnode - The same node successfully registered with AddInputHandler. Its
*       flags and internal timer pointer must remain intact.
*
*   RESULT
*       Always TRUE after removal.
*
*   NOTES
*       Timer removal can wait for I/O completion. The caller owns the node and
*       target. A timer callback can remove itself because NewInput resubmits
*       before invoking it.
*
*   BUGS
*       An unregistered node, repeated removal, or a failed timer registration
*       is not checked. Signal-list iteration does not protect a callback that
*       removes its current node and frees it before NewInput advances.
*
*/
static IPTR Application__MUIM_RemInputHandler(struct IClass *cl,
    Object *obj, struct MUIP_Application_RemInputHandler *msg)
{
    if (msg->ihnode->ihn_Flags & MUIIHNF_TIMER)
    {
        struct timerequest_ext *time_ext =
            (struct timerequest_ext *)msg->ihnode->ihn_Node.mln_Pred;
        /* NULL: AddInputHandler had no memory, or already removed */
        if (!time_ext)
            return TRUE;
        if (!CheckIO((struct IORequest *)time_ext))
            AbortIO((struct IORequest *)time_ext);
        WaitIO((struct IORequest *)time_ext);
        Remove((struct Node *)&time_ext->link);
        FreeVec(time_ext);
        msg->ihnode->ihn_Node.mln_Pred = NULL;
    }
    else
        Remove((struct Node *)msg->ihnode);

    return TRUE;
}


/* Window event consumer defined in window.c. Pass a dequeued live
 * IntuiMessage; reply behavior depends on the Window path. GADGETUP and an
 * eaten IDCMPUPDATE currently lack a reply. Do not reuse a possibly replied
 * message or issue a speculative second reply.
 */
void _zune_window_message(struct IntuiMessage *imsg);   /* from window.c */

/* Queue an id for the next MUIM_Application_NewInput. */
BOOL application_return_id(struct MUI_ApplicationData *data,
    ULONG id)
{
    struct RIDNode *rid = CreateRIDNode(data, id);

    if (!rid)
        return FALSE;
    AddTail((struct List *)&data->app_ReturnIDQueue, (struct Node *)rid);
    return TRUE;
}


static IPTR application_input(struct IClass *, Object *, ULONG *, BOOL);
/****** Application.mui/MUIM_Application_InputBuffered ************
*
*   NAME
*       MUIM_Application_InputBuffered
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_InputBuffered);
*
*   FUNCTION
*       Process all pending input as NewInput does, treating every event
*       source as signalled, without removing a ReturnID. Applications call
*       it during long operations to keep their windows responsive.
*
*   INPUTS
*       No arguments beyond MethodID.
*
*   RESULT
*       Always TRUE.
*
*/
static IPTR Application__MUIM_InputBuffered(struct IClass *cl, Object *obj,
    struct MUIP_Application_InputBuffered *msg)
{
    ULONG signals = SetSignal(0, 0);
    return application_input(cl, obj, &signals, TRUE);
}

/****** Application.mui/MUIM_Application_NewInput ************
*
*   NAME
*       MUIM_Application_NewInput
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_NewInput, (IPTR)signals);
*
*   FUNCTION
*       Process pending pushed methods, handle the signalled event sources,
*       drain newly pushed methods, and remove at most one ReturnID from its
*       FIFO.
*
*   INPUTS
*       signals - Readable and writable ULONG pointer. On entry supply the
*       signals returned by Wait, or zero for polling all current sources.
*       On return it contains the wait mask, or zero while further ReturnIDs
*       are queued. A NULL pointer does nothing and returns zero.
*
*   RESULT
*       The next queued ULONG ReturnID, or zero when none is queued. A queued
*       zero is indistinguishable from no ID.
*
*   NOTES
*       This method itself does not call Wait. Callbacks and invoked operations
*       can still block. A zero input mask treats all registered signal sources
*       as signalled, including user handlers.
*       The wait mask is computed before event callbacks and the final queue
*       drain. Callback changes to registrations are not reflected until a later
*       call. A pushed method can enqueue more work, extending the drain
*       indefinitely. Do not dispose of this application while NewInput is
*       active.
*
*/
static IPTR Application__MUIM_NewInput(struct IClass *cl, Object *obj,
    struct MUIP_Application_NewInput *msg)
{
    return msg ? application_input(cl, obj, msg->signal, FALSE) : 0;
}

/**************************************************************************
 MUIM_Application_NewInput : application main loop
**************************************************************************/
/* The signals the application waits for: its ports' and the input
 * handlers' (each handler's ULONG at offset field: ihn_Signals, or
 * MUIM_Application_Input's ihn_Flags, as before) */
static ULONG app_signal_mask(struct MUI_ApplicationData *data, UWORD field)
{
    struct MinNode *mn;
    ULONG mask = (1L << data->app_GlobalInfo.mgi_WindowsPort->mp_SigBit)
        | (1L << data->app_TimerPort->mp_SigBit);

    for (mn = data->app_IHList.mlh_Head; mn->mln_Succ; mn = mn->mln_Succ)
        mask |= *(ULONG *)((UBYTE *)mn + field);
    if (data->app_Broker)
        mask |= 1L << data->app_BrokerPort->mp_SigBit;
    if (data->app_RexxPort)
        mask |= 1L << data->app_RexxPort->mp_SigBit;
    if (data->app_AppPort)
        mask |= 1L << data->app_AppPort->mp_SigBit;
    if (data->app_CfgNotifyPort)
        mask |= 1L << data->app_CfgNotifyPort->mp_SigBit;
    return mask;
}

/* Whether port has a message for signal */
#define APP_PORT_SIGNALLED(port, signal) \
    ((port) && ((signal) & (1L << (port)->mp_SigBit)))

/* The timers that came back: sent again, then their handler called (the
 * messages taken off the port first: a handler may remove its timer) */
static void app_timers(struct MUI_ApplicationData *data)
{
    struct timerequest_ext *time_ext;
    struct List list;

    NewList(&list);
    while ((time_ext = (struct timerequest_ext *)GetMsg(data->app_TimerPort)))
        AddTail(&list, (struct Node *)time_ext);
    while ((time_ext = (struct timerequest_ext *)RemHead(&list)))
    {
        struct MUI_InputHandlerNode *ihn = time_ext->ihn;

        time_ext->treq.tr_time.tv_secs = ihn->ihn_Millis / 1000;
        time_ext->treq.tr_time.tv_micro = (ihn->ihn_Millis % 1000) * 1000;
        SendIO((struct IORequest *)&time_ext->treq);
        /* Timers carry no signal bits, then the originating node. */
        DoMethod(ihn->ihn_Object, ihn->ihn_Method, 0, ihn);
    }
}

/* The ports other than the windows' and the timer's: commodities, ARexx,
 * the settings file's notification, Workbench's AppMessages (the rarely
 * used handling of their messages is in the module) */
static __attribute__((noinline)) void app_other_ports(struct IClass *cl,
    Object *obj, struct MUI_ApplicationData *data, ULONG signal)
{
    struct Message *m;

    if (APP_PORT_SIGNALLED(data->app_BrokerPort, signal))
        while ((m = GetMsg(data->app_BrokerPort)))
        {
            zune_app_cxmsg(data, obj, m);
            ReplyMsg(m);
        }

    if (APP_PORT_SIGNALLED(data->app_RexxPort, signal))
        while ((m = GetMsg(data->app_RexxPort)))
        {
            struct RexxMsg *rmsg = (struct RexxMsg *)m;

            /* Not IsRexxMsg(): that is TRUE only for messages ARexx itself
             * sent; MUI 3.8 also takes a host's own CreateRexxMsg (wasabi
             * arexx). Without the module the command fails. */
            if (ZUNE_REXX() && (rmsg->rm_Action & RXCODEMASK) == RXCOMM)
            {
                rmsg->rm_Result1 = RETURN_FAIL;
                rmsg->rm_Result2 = 0;
                zune_app_rexx(data, obj, rmsg);
            }
            ReplyMsg(m);
        }

    /* the settings file was written (the settings program): a new
     * Configdata from it */
    if (APP_PORT_SIGNALLED(data->app_CfgNotifyPort, signal)
        && app_drain(data->app_CfgNotifyPort))
        Application_ReloadConfigdata(cl, obj);

    if (APP_PORT_SIGNALLED(data->app_AppPort, signal))
        while ((m = GetMsg(data->app_AppPort)))
            if (!zune_app_appmsg(data, obj, (struct AppMessage *)m))
                ReplyMsg(m);
}

static IPTR application_input(struct IClass *cl, Object *obj,
    ULONG *signals, BOOL buffered)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    struct RIDNode *rid;
    ULONG retval;
    ULONG signal, signalmask;
    struct MinNode *mn;
    struct Message *m;

    if (!signals) return 0;
    signal = *signals;

    /* process all pushed methods */
    while (application_do_pushed_method(data))
        /* nothing */ ;

    Application_StartNotifications(cl, obj);

    signalmask = app_signal_mask(data,
        offsetof(struct MUI_InputHandlerNode, ihn_stuff));

    /* A program that passes 0 (always) has called Wait() already, which
     * cleared the task's signals: every source is taken as signalled, the
     * input handlers' too, as MUI does */
    if (buffered || signal == 0)
        signal |= signalmask;

    if (signal & signalmask)
    {
        if (signal & (1L << data->app_GlobalInfo.mgi_WindowsPort->mp_SigBit))
            while ((m = GetMsg(data->app_GlobalInfo.mgi_WindowsPort)))
                /* the window replies (window.c) */
                _zune_window_message((struct IntuiMessage *)m);

        if (signal & (1L << data->app_TimerPort->mp_SigBit))
            app_timers(data);

        app_other_ports(cl, obj, data, signal);

        for (mn = data->app_IHList.mlh_Head; mn->mln_Succ; mn = mn->mln_Succ)
        {
            struct MUI_InputHandlerNode *ihn =
                (struct MUI_InputHandlerNode *)mn;

            if (signal & ihn->ihn_Signals)
                DoMethod(ihn->ihn_Object, ihn->ihn_Method);
        }
    }

    /* process all pushed methods - again */
    while (application_do_pushed_method(data))
        /* nothing */ ;

    *signals = signalmask;
    if (buffered)
        return TRUE;

    /* the next return id; 0 to wait for while more are queued */
    if (!(rid = (struct RIDNode *)RemHead(
            (struct List *)&data->app_ReturnIDQueue)))
        return 0;
    retval = rid->rid_Value;
    DeleteRIDNode(data, rid);
    if (!IsListEmpty((struct List *)&data->app_ReturnIDQueue))
        *signals = 0;
    return retval;
}

/****** Application.mui/MUIM_Application_Input ************
*
*   NAME
*       MUIM_Application_Input
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_Input, (IPTR)signals);
*
*   FUNCTION
*       Construct a polling mask and invoke NewInput. This is the legacy input
*       method.
*
*   INPUTS
*       signals - Writable ULONG pointer; the incoming value is replaced.
*
*   RESULT
*       The ReturnID result from NewInput; the location receives its wait mask.
*
*   NOTES
*       Use NewInput for signal-driven processing.
*
*   BUGS
*       This implementation combines handler flags rather than ihn_Signals and
*       omits the broker signal from its constructed mask. It does not
*       faithfully poll every registered source.
*
*/
static IPTR Application__MUIM_Input(struct IClass *cl, Object *obj,
    struct MUIP_Application_Input *msg)
{
    /* every source taken as signalled; the handlers' ihn_Flags, not
     * their signals, as before (BUGS above) */
    *msg->signal = app_signal_mask(INST_DATA(cl, obj),
        offsetof(struct MUI_InputHandlerNode, ihn_Flags));
    return Application__MUIM_NewInput(cl, obj, (APTR) msg);
}

/****** Application.mui/MUIM_Application_PushMethod ************
*
*   NAME
*       MUIM_Application_PushMethod
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_PushMethod, (IPTR)dest,
*       count, ...);
*
*   FUNCTION
*       Copy message words into a FIFO entry, enqueue it under the method
*       semaphore, and signal the creating task using the window-port signal.
*
*   INPUTS
*       dest - Live destination object to keep alive until execution or
*       cancellation.
*       count - LONG whose low four bits specify the number of following IPTR
*       words, including MethodID. Use 1 through 15.
*       ... - Destination MethodID followed by its message arguments.
*
*   RESULT
*       An opaque IPTR queue-entry token, or zero on allocation failure. The
*       token is valid only while that entry remains queued.
*
*   NOTES
*       Only words are copied; referenced storage and objects remain borrowed.
*       NewInput and InputBuffered execute queued messages outside the semaphore
*       and discard their results. There is no completion acknowledgement.
*       Cancel pending work before destroying its target; cancellation cannot
*       stop a method already dequeued. Application disposal discards entries
*       still queued.
*
*   BUGS
*       Count is masked with 0xf, not range-checked. Zero creates an entry
*       without a MethodID; 16 becomes zero. The source explicitly notes that
*       extracting words after count is broken on 64-bit big-endian systems.
*
*/
static IPTR Application__MUIM_PushMethod(struct IClass *cl, Object *obj,
    struct MUIP_Application_PushMethod *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    struct MQNode *mq;
    LONG i;
    IPTR *m = (IPTR *) &msg->count; /* FIXME: breaks on 64-bit BigEndian systems */
    LONG count;

    count = msg->count & 0xf; /* MUI4 uses count to pass additional info */

    mq = CreateMQNode(count);
    if (!mq)
        return 0;
    mq->mq_Dest = msg->dest;

    /* fill msg */
    for (i = 0; i < count; i++)
        mq->mq_Msg[i] = *(m + 1 + i);

    EnqueueMethod(data, mq);

    return (IPTR)mq;
}

/****** Application.mui/MUIM_Application_UnpushMethod ************
*
*   NAME
*       MUIM_Application_UnpushMethod
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_UnpushMethod, (IPTR)dest,
*       methodid, method);
*
*   FUNCTION
*       Remove and free every queued method matching all supplied filters.
*
*   INPUTS
*       dest - Destination pointer or NULL for any destination.
*       methodid - Queue-entry token from PushMethod, or zero for any token.
*       method - Destination MethodID, or zero for any method.
*
*   RESULT
*       Number of queued entries removed.
*
*   NOTES
*       The queue is locked while filtering. This neither waits for nor cancels
*       an entry already dequeued for execution. Freed tokens may be reused by
*       allocation; do not retain them as permanent identifiers.
*
*/


/****** Application.mui/MUIM_Application_ReturnID ************
*
*   NAME
*       MUIM_Application_ReturnID
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_ReturnID, retid);
*
*   FUNCTION
*       Append a ULONG return value to the application ReturnID FIFO.
*
*   INPUTS
*       retid - ULONG value; MUIV_Application_ReturnID_Quit is the conventional
*       quit value.
*
*   RESULT
*       TRUE if allocation and insertion succeed; FALSE on allocation failure.
*
*   NOTES
*       NewInput removes one ID per call. This method neither signals the
*       application task nor locks the ReturnID queue. Use it in the application
*       task; external tasks can request it through PushMethod while separately
*       ensuring object lifetime.
*
*/
static IPTR Application__MUIM_ReturnID(struct IClass *cl, Object *obj,
    struct MUIP_Application_ReturnID *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

/*
    if (!data->app_RIDMemChunk)
    {
        data->app_RIDMemChunk =
            g_mem_chunk_create(struct RIDNode, 10, G_ALLOC_AND_FREE);
    }
*/
    return application_return_id(data, msg->retid);
}


/* Match the application UserData first, otherwise delegate to the internal
 * Family including its own UserData. Return a borrowed match pointer or
 * zero. The menustrip is not traversed by this override.
 */
static IPTR Application__MUIM_FindUData(struct IClass *cl, Object *obj,
    struct MUIP_FindUData *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    if (ZUNE_FIND_MATCH(obj, msg))
        return (IPTR) obj;

    return DoMethodA(data->app_WindowFamily, (Msg) msg);
}


/* On an application UserData match, get attr into writable storage and
 * return TRUE without checking the getter result; otherwise delegate to the
 * internal Family. The comparison field is ULONG and the menustrip is not
 * traversed.
 */
static IPTR Application__MUIM_GetUData(struct IClass *cl, Object *obj,
    struct MUIP_GetUData *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
    {
        get(obj, msg->attr, msg->storage);
        return TRUE;
    }
    return DoMethodA(data->app_WindowFamily, (Msg) msg);
}


/* Set the application on a UserData match and forward to the internal
 * Family, then return TRUE regardless of setter results or matches. Keep
 * traversal stable through synchronous notifications.
 */
static IPTR Application__MUIM_SetUData(struct IClass *cl, Object *obj,
    struct MUIP_SetUData *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
        set(obj, msg->attr, msg->val);

    DoMethodA(data->app_WindowFamily, (Msg) msg);
    return TRUE;
}


/* Set the application and return TRUE on a UserData match, ignoring setter
 * success; otherwise delegate to the internal Family and return its first-
 * match result. Keep traversal stable through callbacks.
 */
static IPTR Application__MUIM_SetUDataOnce(struct IClass *cl, Object *obj,
    struct MUIP_SetUDataOnce *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
    {
        set(obj, msg->attr, msg->val);
        return TRUE;
    }
    return DoMethodA(data->app_WindowFamily, (Msg) msg);
}



/****** Application.mui/MUIM_Application_AboutMUI ************
*
*   NAME
*       MUIM_Application_AboutMUI
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_AboutMUI, (IPTR)refwindow);
*
*   FUNCTION
*       Create the Aboutmui window on first use, cache it, and request that it
*       open. Later calls reuse the same window.
*
*   INPUTS
*       refwindow - Optional live Window object used as the reference during
*       first construction; NULL omits the reference tag.
*
*   RESULT
*       Always zero, including construction or opening failure.
*
*   NOTES
*       The Aboutmui constructor attaches the window to this application, which
*       owns it. Later refwindow arguments do not reposition the cached window.
*       Keep a supplied reference window alive while the cached window uses it.
*       Do not dispose of the cached window separately and leave this
*       application alive.
*
*/


/* Close each currently open window and try to collect its borrowed pointer
 * in ln_Name of an owned AllocVec node/list. Return that list or NULL.
 * List/node allocation failure does not stop closing and can lose reopening
 * information. The eventual OpenWindows call consumes the allocations; keep
 * all referenced windows alive.
 */


/****** Application.mui/MUIM_Application_SetConfigdata ************
*
*   NAME
*       MUIM_Application_SetConfigdata
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_SetConfigdata,
*       (IPTR)configdata);
*
*   FUNCTION
*       Close currently open windows, install the new object, read its
*       ZunePrefs pointer, and queue reopening.
*
*   INPUTS
*       configdata - Live Configdata-compatible object. Ownership transfers to
*       the application when installed.
*
*   RESULT
*       Always zero, without an installation or reopening success indication.
*
*   NOTES
*       Configuration/prefs pointers obtained before the call describe the
*       replaced configuration, not the installed one. The method closes windows immediately but
*       reopens them through the pushed-method queue. Once input processing
*       has started, it also starts the new object's notification. The new
*       configuration must provide valid ZunePrefs.
*
*   BUGS
*       The replaced configuration object is never disposed, and its public
*       screen file notification and input handler stay active.
*       No NULL or getter-success check is performed before adopting the
*       replacement. If saving the open-window list fails, NULL later means
*       reopen all windows; failed node allocation can omit individual
*       windows. Queue failure leaves the windows closed.
*
*/

/****** Application.mui/MUIM_Application_SetConfigItem ************
*
*   NAME
*       MUIM_Application_SetConfigItem
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_SetConfigItem, item,
*       (IPTR)data);
*
*   FUNCTION
*       Set a supported string-valued configuration item with
*       MUIM_Configdata_SetString. Other item IDs do nothing.
*
*   INPUTS
*       item - One of the supported identifiers listed below.
*       data - Non-NULL readable NUL-terminated string.
*       Supported IDs: MUICFG_Keyboard_Press, MUICFG_Keyboard_Toggle,
*       MUICFG_Keyboard_Up, MUICFG_Keyboard_Down, MUICFG_Keyboard_PageUp,
*       MUICFG_Keyboard_PageDown, MUICFG_Keyboard_Top, MUICFG_Keyboard_Bottom,
*       MUICFG_Keyboard_Left, MUICFG_Keyboard_Right, MUICFG_Keyboard_WordLeft,
*       MUICFG_Keyboard_WordRight, MUICFG_Keyboard_LineStart,
*       MUICFG_Keyboard_LineEnd, MUICFG_Keyboard_NextGadget,
*       MUICFG_Keyboard_PrevGadget, MUICFG_Keyboard_GadgetOff,
*       MUICFG_Keyboard_CloseWindow, MUICFG_Keyboard_NextWindow,
*       MUICFG_Keyboard_PrevWindow, MUICFG_Keyboard_Help, MUICFG_Keyboard_Popup,
*       MUICFG_Drag_LMBModifier, MUICFG_Drag_MMBModifier,
*       MUICFG_Iconification_Hotkey, MUICFG_Font_Normal, MUICFG_Font_List,
*       MUICFG_Font_Tiny, MUICFG_Font_Fixed, MUICFG_Font_Title, MUICFG_Font_Big,
*       MUICFG_Font_Button, MUICFG_Font_Knob, MUICFG_String_Background,
*       MUICFG_String_Text, MUICFG_String_ActiveBackground,
*       MUICFG_String_ActiveText, MUICFG_String_Cursor,
*       MUICFG_String_MarkedBackground, MUICFG_String_MarkedText,
*       MUICFG_ActiveObject_Color, MUICFG_PublicScreen.
*
*   RESULT
*       Always zero, including unsupported IDs or ignored underlying failures.
*
*   NOTES
*       Keyboard bindings, drag modifiers, and the iconification hotkey are set
*       directly. Font IDs, String color/pen IDs, ActiveObject_Color, and
*       PublicScreen close currently open windows and queue reopening after the
*       change. Configdata copies the string or removes a value equal to its
*       default. This does not save preferences to disk.
*
*   BUGS
*       The close/reopen path shares SetConfigdata's incomplete-allocation and
*       queue-failure limitations; no rollback or success status is provided.
*
*/


/****** Application.mui/MUIM_Application_OpenWindows ************
*
*   NAME
*       MUIM_Application_OpenWindows
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_OpenWindows,
*       (IPTR)windows);
*
*   FUNCTION
*       Request opening of the listed windows that are still application
*       members, or all current application windows when windows is NULL.
*
*   INPUTS
*       windows - NULL, or an owned Exec List allocated with AllocVec whose
*       nodes were individually allocated with AllocVec and hold window
*       pointers in ln_Name.
*
*   RESULT
*       Always zero; window setter results are ignored.
*
*   NOTES
*       A non-NULL list and every removed node are consumed and freed with
*       FreeVec. Do not pass the borrowed Application_WindowList or stack
*       storage. The referenced windows are not freed. For NULL, traverse the
*       internal Family without taking ownership of its list.
*
*/


/****** Application.mui/MUIM_Application_OpenConfigWindow ************
*
*   NAME
*       MUIM_Application_OpenConfigWindow
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_OpenConfigWindow, flags);
*
*   FUNCTION
*       Launch sys:prefs/OpenMUI (sys:prefs/Zune if that is missing, as
*       an earlier installation had it) asynchronously with the
*       application base and object address on its command line.
*
*   INPUTS
*       flags - ULONG declared in the message; ignored.
*
*   RESULT
*       0 if SystemTagList returns -1; otherwise 1. This does not report whether
*       the preferences program completed successfully.
*
*   NOTES
*       The command is built in a 255-byte buffer. Child standard input and
*       output are disabled; the stack size is AROS_STACKSIZE.
*
*   BUGS
*       The base is inserted without shell quoting and may be truncated along
*       with the command. Accepted Base characters are not a shell-escaping
*       guarantee. The method does not establish that the externally used
*       application object remains alive.
*
*/

/****** Application.mui/MUIM_Application_Execute ************
*
*   NAME
*       MUIM_Application_Execute
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_Execute);
*
*   FUNCTION
*       Run NewInput and Wait repeatedly until the quit ReturnID or
*       SIGBREAKF_CTRL_C is received.
*
*   INPUTS
*       No arguments beyond MethodID. Call in the application's creating task.
*
*   RESULT
*       Always zero on leaving the loop.
*
*   NOTES
*       The loop waits on the NewInput mask plus Ctrl-C when the mask is
*       nonzero. It ignores other ReturnIDs, does not dispose of the
*       application, and has no timeout.
*
*   BUGS
*       It uses an IPTR local for an interface that writes ULONG signals. The
*       mismatch is not portable to all 64-bit byte orders. Callback changes
*       to masks inherit NewInput's limitations.
*
*/


/****** Application.mui/MUIM_Application_UpdateMenus ************
*
*   NAME
*       MUIM_Application_UpdateMenus
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_UpdateMenus);
*
*   FUNCTION
*       Send MUIM_Window_UpdateMenu synchronously to every current application
*       window.
*
*   INPUTS
*       No arguments beyond MethodID. Keep membership stable during traversal.
*
*   RESULT
*       Always zero; window results are ignored.
*
*   NOTES
*       Window menu updates can free native Menu and MenuItem structures before
*       rebuilding them. Do not retain their pointers across this method. This
*       is not a success report for menu allocation or attachment.
*
*/

/****** Application.mui/MUIM_Application_Load ************
*
*   NAME
*       MUIM_Application_Load
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_Load, (IPTR)name);
*
*   FUNCTION
*       Read a PREF/MUIO chunk into a temporary Dataspace, then send MUIM_Import
*       to each application window.
*
*   INPUTS
*       name - Filename, or MUIV_Application_Load_ENV (NULL) for
*       ENV:MUI/<base>.cfg, or MUIV_Application_Load_ENVARC for
*       ENVARC:MUI/<base>.cfg. If that file cannot be opened, the same name
*       in ENV:Zune or ENVARC:Zune is tried. Explicit filenames must be
*       NUL-terminated and shorter than 1023 bytes.
*
*   RESULT
*       Always zero, including allocation, file, IFF, and child-method failures.
*
*   NOTES
*       Import is attempted even when the file could not be opened or parsed,
*       using an empty or partially populated dataspace. There is no rollback of
*       child state.
*       The temporary dataspace is disposed before return; child methods must
*       not retain it or borrowed payloads. I/O can block. These .cfg state
*       files are separate from the .prefs configuration file.
*
*   BUGS
*       Underlying results are ignored. The explicit-name strncpy path does not
*       append a terminator for an overlong name. The return value cannot
*       establish successful loading.
*
*/
/* MUIM_Application_Load and MUIM_Application_Save are one function: the
 * windows' state through a Dataspace (MUIM_Export before writing,
 * MUIM_Import after reading), the file msg->name, or ENV(ARC):MUI/<base>.cfg
 * for the MUIV_ values (both messages are {MethodID, name}). MUI keeps the
 * objects' state in ENV(ARC):MUI/<base>.cfg; earlier Zune68 versions used
 * ENV(ARC):Zune, which is still read. Like MUI, Save creates the MUI drawer
 * when it is missing. */

/****** Application.mui/MUIM_Application_Save ************
*
*   NAME
*       MUIM_Application_Save
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_Save, (IPTR)name);
*
*   FUNCTION
*       Export each window into a temporary Dataspace, then write an IFF PREF
*       file containing PRHD and MUIO chunks.
*
*   INPUTS
*       name - Filename, or MUIV_Application_Save_ENV (NULL) for
*       ENV:MUI/<base>.cfg, or MUIV_Application_Save_ENVARC for
*       ENVARC:MUI/<base>.cfg. A missing ENV:MUI or ENVARC:MUI drawer is
*       created. Explicit filenames must be NUL-terminated and shorter than
*       1023 bytes.
*
*   RESULT
*       Always zero, including allocation, file, IFF, and child-method failures.
*
*   NOTES
*       Opening with MODE_NEWFILE can replace existing content. Output is not
*       atomic and failures can leave a partial file. Exported state depends on
*       each child's implementation.
*       The temporary dataspace is disposed before return; child methods must
*       not retain it or borrowed payloads. I/O can block. These .cfg state
*       files are separate from the .prefs configuration file.
*
*   BUGS
*       Underlying results are ignored. The explicit-name strncpy path does not
*       append a terminator for an overlong name. The return value cannot
*       establish successful saving.
*
*/
/****** Application.mui/MUIM_Application_CheckRefresh ************
*
*   NAME
*       MUIM_Application_CheckRefresh
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_CheckRefresh);
*
*   FUNCTION
*       Send MUIM_Window_Refresh to every window in the internal family.
*
*   INPUTS
*       No arguments beyond MethodID. Keep the application and window list alive
*       throughout the call.
*
*   RESULT
*       Always zero; child results are ignored.
*
*   NOTES
*       This is the refresh entry point used from modal-requester IntuiMessage
*       hooks. It does not itself dequeue or reply to the hook message. Supply
*       any native-window requester tags only while that window is open; they
*       are not necessarily valid during construction.
*
*/
static IPTR Application__MUIM_CheckRefresh(struct IClass *cl, Object *obj,
    struct MUIP_Application_CheckRefresh *message)
{
    app_each_window(INST_DATA(cl, obj), MUIM_Window_Refresh, 0);
    return 0;
}

/* Older menu entry points share the modern menu objects and their state. */


/*
 * The class dispatcher
 */
/****** Application.mui/MUIM_Application_GetMenuCheck ************
*
*   NAME
*       MUIM_Application_GetMenuCheck
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_GetMenuCheck, MenuID);
*
*   FUNCTION
*       Return MUIA_Menuitem_Checked of the first menu item whose UserData
*       equals MenuID. MUIM_FindUData searches each application window's
*       menustrip, or the application menustrip for a window without one,
*       in window-list order.
*
*   INPUTS
*       MenuID - ULONG menu identifier.
*
*   RESULT
*       The item's checked state, or zero when no item matches.
*
*   NOTES
*       Only menustrips reachable through application windows are searched;
*       without windows the result is zero.
*
*/

/****** Application.mui/MUIM_Application_GetMenuState ************
*
*   NAME
*       MUIM_Application_GetMenuState
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_GetMenuState, MenuID);
*
*   FUNCTION
*       Return MUIA_Menuitem_Enabled of the first menu item whose UserData
*       equals MenuID. MUIM_FindUData searches each application window's
*       menustrip, or the application menustrip for a window without one,
*       in window-list order.
*
*   INPUTS
*       MenuID - ULONG menu identifier.
*
*   RESULT
*       The item's enabled state, or zero when no item matches.
*
*   NOTES
*       Only menustrips reachable through application windows are searched;
*       without windows the result is zero.
*
*/

/****** Application.mui/MUIM_Application_SetMenuCheck ************
*
*   NAME
*       MUIM_Application_SetMenuCheck
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_SetMenuCheck, MenuID,
*       stat);
*
*   FUNCTION
*       Set MUIA_Menuitem_Checked to stat on the menu item whose UserData
*       equals MenuID. MUIM_FindUData searches each application window's
*       menustrip, or the application menustrip for a window without one.
*
*   INPUTS
*       MenuID - ULONG menu identifier.
*       stat - LONG state.
*
*   RESULT
*       Always zero.
*
*   NOTES
*       Only menustrips reachable through application windows are searched;
*       without windows nothing changes.
*
*/

/****** Application.mui/MUIM_Application_SetMenuState ************
*
*   NAME
*       MUIM_Application_SetMenuState
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_SetMenuState, MenuID,
*       stat);
*
*   FUNCTION
*       Set MUIA_Menuitem_Enabled to stat on the menu item whose UserData
*       equals MenuID. MUIM_FindUData searches each application window's
*       menustrip, or the application menustrip for a window without one.
*
*   INPUTS
*       MenuID - ULONG menu identifier.
*       stat - LONG state.
*
*   RESULT
*       Always zero.
*
*   NOTES
*       Only menustrips reachable through application windows are searched;
*       without windows nothing changes.
*
*/

/****** Application.mui/MUIM_Application_ShowHelp ************
*
*   NAME
*       MUIM_Application_ShowHelp
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Application_ShowHelp, (IPTR)window,
*       (IPTR)name, (IPTR)node, line);
*
*   FUNCTION
*       Open an AmigaGuide file synchronously with OpenAmigaGuideA from
*       amigaguide.library V39, then close it.
*
*   INPUTS
*       window - Window whose screen hosts the viewer, -1 for the first open
*       application window, or NULL to leave the screen to amigaguide.library.
*       name - Guide file name, or NULL for MUIA_Application_HelpFile.
*       node - Node name, or NULL.
*       line - Starting line passed as nag_Line.
*
*   RESULT
*       TRUE if the guide was opened; FALSE without a name, without
*       amigaguide.library, or when opening fails.
*
*   NOTES
*       MUIA_Application_Sleep is TRUE during the call.
*
*/

/****** Application.mui/MUIM_Application_Iconify ************
*
*   NAME
*       MUIM_Application_Iconify
*
*   SYNOPSIS
*       DoMethodA(obj, message);
*
*   FUNCTION
*       Declared in application.h, but not handled by the Application
*       dispatcher. The message is forwarded through Notify to rootclass without
*       implementing this operation.
*
*   INPUTS
*       The header defines the MethodID but no corresponding method structure or
*       implemented argument contract.
*
*   RESULT
*       The inherited unhandled-method result is zero. It is not a completion or
*       menu-state result.
*
*   NOTES
*       Use the implemented attributes and object interfaces only where their
*       documented behavior meets the need. A method name alone does not
*       establish compatibility.
*
*/

/* A program started with a small stack (4 KB, the Workbench and Shell
 * default) can run out of it while Zune draws a window full of nested
 * groups: the drawing passes through the ROM's gadget and layer code too.
 * MUI 3.8 needs about 3.6 KB there, Zune about 4.9 KB. When little stack
 * is left, the work runs on a spare stack the application keeps until it
 * is disposed; programs with enough stack never get one. */

#if defined(ZUNE68_GCC_NATIVE) && defined(__mc68000__)
typedef IPTR (*ZuneDeepFunc)(struct IClass *, Object *, Msg);

/* zune_stack_run(sss, fn, cl, obj, msg): StackSwap to sss, call
 * fn(cl, obj, msg) there, swap back, return fn's result. */
IPTR zune_stack_run(struct StackSwapStruct *sss, ZuneDeepFunc fn,
    struct IClass *cl, Object *obj, Msg msg);
__asm__(
"   .text\n"
"   .even\n"
"   .globl _zune_stack_run\n"
"_zune_stack_run:\n"
"   movem.l d2-d4/a2-a3/a6,-(sp)\n"
"   move.l  28(sp),a2\n"
"   move.l  32(sp),a3\n"
"   move.l  36(sp),d2\n"
"   move.l  40(sp),d3\n"
"   move.l  44(sp),d4\n"
"   move.l  4.w,a6\n"
"   move.l  a2,a0\n"
"   jsr     -732(a6)\n"
"   move.l  d4,-(sp)\n"
"   move.l  d3,-(sp)\n"
"   move.l  d2,-(sp)\n"
"   jsr     (a3)\n"
"   lea     12(sp),sp\n"
"   move.l  d0,d2\n"
"   move.l  4.w,a6\n"
"   move.l  a2,a0\n"
"   jsr     -732(a6)\n"
"   move.l  d2,d0\n"
"   movem.l (sp)+,d2-d4/a2-a3/a6\n"
"   rts\n");

/* Run fn(cl, obj, msg) with enough stack; data is the application's. */
static IPTR deep_call(struct MUI_ApplicationData *data, ZuneDeepFunc fn,
    struct IClass *cl, Object *obj, Msg msg)
{
    struct Task *me = SysBase->ThisTask;
    struct StackSwapStruct sss;
    IPTR result;

    if (!me || !data || data->app_SpareBusy
        || (ULONG)&sss - (ULONG)me->tc_SPLower >= ZUNE_STACK_LOW
        || (ULONG)&sss > (ULONG)me->tc_SPUpper)
        return fn(cl, obj, msg);
    if (!data->app_SpareStack &&
        !(data->app_SpareStack = AllocMem(ZUNE_STACK_SPARE, MEMF_ANY)))
        return fn(cl, obj, msg);
    sss.stk_Lower = data->app_SpareStack;
    sss.stk_Upper = (ULONG)data->app_SpareStack + ZUNE_STACK_SPARE;
    sss.stk_Pointer = (APTR)sss.stk_Upper;
    data->app_SpareBusy = TRUE;
    result = zune_stack_run(&sss, fn, cl, obj, msg);
    data->app_SpareBusy = FALSE;
    return result;
}

/* The same for code outside this class (window opening): the application
 * is obj's (a Notify object's), none before it has one. */
IPTR zune_deep_call(ZuneDeepFunc fn, struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_GlobalInfo *mgi = muiNotifyData(obj)->mnd_GlobalInfo;
    Object *app = mgi ? mgi->mgi_ApplicationObject : NULL;
    struct IClass *c = app ? OCLASS(app) : NULL;

    while (c && !(c->cl_ID && !strcmp((char *)c->cl_ID, MUIC_Application)))
        c = c->cl_Super;
    return deep_call(c ? INST_DATA(c, app) : NULL, fn, cl, obj, msg);
}
#define DEEP(fn, cl, obj, msg) \
    deep_call(INST_DATA(cl, obj), (ZuneDeepFunc)(fn), cl, obj, (Msg)(msg))
#else
#define DEEP(fn, cl, obj, msg) fn(cl, obj, (APTR)(msg))
#endif

/**************************************************************************
 MUI's application list object (private LVO -264, muimaster_lib.c)

 MUI 3.8 and MUI 5 return from their private library function at LVO -264
 one global object: a subclass of Semaphore.mui (MUI 5 names it
 Applist.mui), whose private method 0x8042e50f takes an attribute and a
 string and returns the first application whose attribute reads as that
 string (strcmp, case counts), else 0. Measured by calling them (not read
 from their code): Base, Title and Author of two applications found, an
 unknown value or other case not, a disposed application not. AmIRC 3.6
 asks it for "AMIRC.1", "AMIRC.2"... to find a free ARexx base name.
**************************************************************************/
#define MUIM_Applist_Find 0x8042e50f   /* private in MUI */
struct MUIP_Applist_Find { STACKED ULONG MethodID; STACKED ULONG attr; STACKED IPTR value; };

/* Search tracked applications under ZuneSemaphore for one whose attr gets
 * as the string value. Return a borrowed application or NULL. */
static Object *find_application_by_string(ULONG attr, CONST_STRPTR value)
{
    struct MUIMasterBase_intern *intZuneBase = (struct MUIMasterBase_intern *)MUIMasterBase;
    struct TrackingNode *tn;
    Object *retval = NULL;

    if (!value)
        return NULL;
    ObtainSemaphore(&intZuneBase->ZuneSemaphore);
    ForeachNode(&intZuneBase->Applications, tn)
    {
        IPTR v = 0;

        if (GetAttr(attr, tn->tn_Application, &v) && v
            && !strcmp((CONST_STRPTR)v, value))
        {
            retval = tn->tn_Application;
            break;
        }
    }
    ReleaseSemaphore(&intZuneBase->ZuneSemaphore);
    return retval;
}

BOOPSI_DISPATCHER(IPTR, Applist_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_Applist_Find)
    {
        struct MUIP_Applist_Find *m = (struct MUIP_Applist_Find *)msg;
        return (IPTR)find_application_by_string(m->attr,
            (CONST_STRPTR)m->value);
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/* The object, made the first time it is asked for; NULL without memory.
 * It lives until the library is expunged (zune_applist_free). */
Object *zune_applist_get(void)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);

    ObtainSemaphore(&base->ZuneSemaphore);
    if (!base->ApplistClass)
        base->ApplistClass = MUI_CreateCustomClass(NULL, MUIC_Semaphore,
            NULL, 0, (APTR)Applist_Dispatcher);
    if (base->ApplistClass && !base->Applist)
        base->Applist = NewObject(base->ApplistClass->mcc_Class, NULL,
            TAG_DONE);
    ReleaseSemaphore(&base->ZuneSemaphore);
    return base->Applist;
}

void zune_applist_free(void)
{
    struct MUIMasterBase_intern *base = MUIMB(MUIMasterBase);

    if (base->Applist)
        DisposeObject(base->Applist);
    base->Applist = NULL;
    if (base->ApplistClass)
        MUI_DeleteCustomClass(base->ApplistClass);
    base->ApplistClass = NULL;
}

/* Dispatch supported Application operations and inherited user-data/member
 * overrides, returning handler results. Forward unknown methods to Notify.
 * Header-only method IDs are not silently implemented here; messages must
 * contain the fields required by the selected handler.
 */
/*
 * FilePrefHeader
 */
struct FilePrefHeader
{
    UBYTE ph_Version;
    UBYTE ph_Type;
    UBYTE ph_Flags[4];
};

#define ID_MUIO MAKE_ID('M','U','I','O')

/* MUIM_Application_Load/Save (most programs restore their windows' state
 * at start and save it at quit), SetConfigItem (some set settings at
 * start) and the settings' reopening of the windows (SetConfigdata,
 * OpenWindows): in the library, not the module (the suite's LoadSeg trace:
 * YAM, IBrowse, AmIRC, MultiRen and Archiver loaded it at start) */
static struct List *Application__CloseWindows(struct IClass *cl, Object *obj)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    struct MinList *children = NULL;
    struct List *windows = NULL;
    Object *cstate;
    Object *child;

    children = app_windows(data);
    if (!children)
        return NULL;

    /* remember which windows were open, so they can be re-opened after a
     * configdata change. Windows that were already closed are left alone. */
    windows = AllocVec(sizeof(struct List), MEMF_CLEAR);
    if (windows)
        NewList(windows);

    cstate = (Object *) children->mlh_Head;
    while ((child = NextObject(&cstate)))
    {
        if (XGET(child, MUIA_Window_Open))
        {
            if (windows)
            {
                struct Node *n = AllocVec(sizeof(struct Node), MEMF_CLEAR);

                if (n)
                {
                    n->ln_Name = (STRPTR)child;
                    AddTail(windows, n);
                }
            }
            D(bug("[MUI:App] %s: closing window %p\n", __func__, child));
            set(child, MUIA_Window_Open, FALSE);
        }
    }
    return windows;
}

static void QueueOpenWindows(Object *obj, struct MUI_ApplicationData *data,
    struct List *windows)
{
    struct MQNode *mq = CreateMQNode(2);

    if (!mq)
    {
        FreeWindowList(windows);
        return;
    }
    mq->mq_Dest = obj;
    mq->mq_Msg[0] = MUIM_Application_OpenWindows;
    mq->mq_Msg[1] = (IPTR)windows;
    mq->mq_Count |= MQF_OWNED_WINDOWS;
    EnqueueMethod(data, mq);
}

static IPTR Application__MUIM_SetConfigdata(struct IClass *cl, Object *obj,
    struct MUIP_Application_SetConfigdata *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    struct List *windows;

    /* close all currently open windows and remember them, so they can be
     * re-opened with the new settings afterwards */
    windows = Application__CloseWindows(cl, obj);

    data->app_GlobalInfo.mgi_Configdata = msg->configdata;
    get(data->app_GlobalInfo.mgi_Configdata, MUIA_Configdata_ZunePrefs,
        &data->app_GlobalInfo.mgi_Prefs);
    if (data->app_NotifyStarted)
        DoMethod(data->app_GlobalInfo.mgi_Configdata,
            MUIM_Configdata_SetupNotify);

    QueueOpenWindows(obj, data, windows);
    return 0;
}

static IPTR Application__MUIM_SetConfigItem(struct IClass *cl, Object *obj,
    struct MUIP_Application_SetConfigItem *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    if (data->app_GlobalInfo.mgi_Configdata) {
        switch (msg->item) {
            /*
            case MUICFG_Window_Spacing_Left:
            case MUICFG_Window_Spacing_Right:
            case MUICFG_Window_Spacing_Top:
            case MUICFG_Window_Buttons:
            case MUICFG_Window_Spacing_Bottom:
            case MUICFG_Window_Positions:
            case MUICFG_Window_Redraw:
            case MUICFG_Window_Refresh:
            case MUICFG_Radio_HSpacing:
            case MUICFG_Radio_VSpacing:
            case MUICFG_Group_HSpacing:
            case MUICFG_Group_VSpacing:
            case MUICFG_Cycle_MenuCtrl_Position:
            case MUICFG_Cycle_MenuCtrl_Level:
            case MUICFG_Cycle_MenuCtrl_Speed:
            case MUICFG_Cycle_Menu_Recessed:
            case MUICFG_Listview_Font_Leading:
            case MUICFG_Listview_Smoothed:
            case MUICFG_Listview_SmoothVal:
            case MUICFG_Listview_Refresh:
            case MUICFG_Listview_Multi:
            case MUICFG_GroupTitle_Position:
            case MUICFG_GroupTitle_Color:
            case MUICFG_Scrollbar_Type:
            case MUICFG_Scrollbar_Arrangement:
            case MUICFG_Balance_Look:
            case MUICFG_Dragndrop_Look:
            case MUICFG_Drag_Autostart:
            case MUICFG_Drag_Autostart_Length:
            case MUICFG_Drag_LeftButton:
            case MUICFG_Drag_MiddleButton:
            case MUICFG_Register_TruncateTitles:
            case MUICFG_Screen_Mode:
            case MUICFG_Screen_Mode_ID:
            case MUICFG_Screen_Width:
            case MUICFG_Screen_Height:
            case MUICFG_PublicScreen_PopToFront:
            case MUICFG_Iconification_ShowIcon:
            case MUICFG_Iconification_ShowMenu:
            case MUICFG_Iconification_OnStartup:
            case MUICFG_Interfaces_EnableARexx:
            case MUICFG_BubbleHelp_FirstDelay:
            case MUICFG_BubbleHelp_NextDelay: */
            case MUICFG_Keyboard_Press:
            case MUICFG_Keyboard_Toggle:
            case MUICFG_Keyboard_Up:
            case MUICFG_Keyboard_Down:
            case MUICFG_Keyboard_PageUp:
            case MUICFG_Keyboard_PageDown:
            case MUICFG_Keyboard_Top:
            case MUICFG_Keyboard_Bottom:
            case MUICFG_Keyboard_Left:
            case MUICFG_Keyboard_Right:
            case MUICFG_Keyboard_WordLeft:
            case MUICFG_Keyboard_WordRight:
            case MUICFG_Keyboard_LineStart:
            case MUICFG_Keyboard_LineEnd:
            case MUICFG_Keyboard_NextGadget:
            case MUICFG_Keyboard_PrevGadget:
            case MUICFG_Keyboard_GadgetOff:
            case MUICFG_Keyboard_CloseWindow:
            case MUICFG_Keyboard_NextWindow:
            case MUICFG_Keyboard_PrevWindow:
            case MUICFG_Keyboard_Help:
            case MUICFG_Keyboard_Popup:
            case MUICFG_Drag_LMBModifier:
            case MUICFG_Drag_MMBModifier:
            case MUICFG_Iconification_Hotkey:
                DoMethod(data->app_GlobalInfo.mgi_Configdata, MUIM_Configdata_SetString, msg->item, msg->data);
                break;
            case MUICFG_Font_Normal:
            case MUICFG_Font_List:
            case MUICFG_Font_Tiny:
            case MUICFG_Font_Fixed:
            case MUICFG_Font_Title:
            case MUICFG_Font_Big:
            case MUICFG_Font_Button:
            case MUICFG_Font_Knob:
            case MUICFG_String_Background:
            case MUICFG_String_Text:
            case MUICFG_String_ActiveBackground:
            case MUICFG_String_ActiveText:
            case MUICFG_String_Cursor:
            case MUICFG_String_MarkedBackground:
            case MUICFG_String_MarkedText:
            case MUICFG_ActiveObject_Color:
            case MUICFG_PublicScreen:
                {
                    struct List *windows;

                    windows = Application__CloseWindows(cl, obj);
                    DoMethod(data->app_GlobalInfo.mgi_Configdata, MUIM_Configdata_SetString, msg->item, msg->data);
                    QueueOpenWindows(obj, data, windows);
                }
                break;
        }
    }
    return 0;
}

static IPTR Application__MUIM_OpenWindows(struct IClass *cl, Object *obj,
    struct MUIP_Application_OpenWindows *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    /* a list of windows to open was passed (e.g. the ones closed by
     * Application__CloseWindows before a configdata change) - open exactly
     * those and free the list */
    if (msg->windows)
    {
        struct Node *n;

        while ((n = (struct Node *)RemHead(msg->windows)))
        {
            struct MinList *children = NULL;
            Object *cstate, *child;

            /* A window may have been removed and disposed since queuing.
             * Compare pointers in the live family before using the target.
             * Re-read after each open, which can itself change membership.
             */
            children = app_windows(data);
            if (children)
            {
                cstate = (Object *)children->mlh_Head;
                while ((child = NextObject(&cstate)))
                    if (child == (Object *)n->ln_Name)
                    {
                        set(child, MUIA_Window_Open, TRUE);
                        break;
                    }
            }
            FreeVec(n);
        }
        FreeVec(msg->windows);
        return 0;
    }

    /* otherwise open all windows of the application */
    {
        struct MinList *children = NULL;
        Object *cstate;
        Object *child;

        children = app_windows(data);
        if (!children)
            return 0;

        cstate = (Object *) children->mlh_Head;
        while ((child = NextObject(&cstate)))
        {
            set(child, MUIA_Window_Open, TRUE);
        }
    }
    return 0;
}

static IPTR Application__MUIM_LoadSave(struct IClass *cl, Object *obj,
    struct MUIP_Application_Load *message)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    BOOL save = message->MethodID == MUIM_Application_Save;
    struct IFFHandle *iff;
    char buf[280];
    CONST_STRPTR name = message->name;
    CONST_STRPTR env = NULL;
    BPTR fh;
    Object *dataspace;

    if (!data->app_Base)
        return 0;

    dataspace = MUI_NewObject(MUIC_Dataspace, TAG_DONE);
    if (!dataspace)
        return 0;

    if (save)
        app_each_window(data, MUIM_Export, (IPTR)dataspace);

    if (message->name == MUIV_Application_Load_ENV)
        env = "ENV";
    else if (message->name == MUIV_Application_Load_ENVARC)
        env = "ENVARC";
    if (env)
    {
        snprintf(buf, sizeof(buf), "%s:MUI/%s.cfg", env, data->app_Base);
        name = buf;
    }

    fh = Open(name, save ? MODE_NEWFILE : MODE_OLDFILE);
    if (!fh && env)
    {
        if (save)
        {
            BPTR dir;

            snprintf(buf, sizeof(buf), "%s:MUI", env);
            if ((dir = CreateDir(buf)))
            {
                UnLock(dir);
                snprintf(buf, sizeof(buf), "%s:MUI/%s.cfg", env,
                    data->app_Base);
                fh = Open(buf, MODE_NEWFILE);
            }
        }
        else
        {
            snprintf(buf, sizeof(buf), "%s:Zune/%s.cfg", env,
                data->app_Base);
            fh = Open(buf, MODE_OLDFILE);
        }
    }
    if (fh)
    {
        if (ZUNE_IFFPARSE() && (iff = AllocIFF()))
        {
            iff->iff_Stream = (IPTR) fh;

            InitIFFasDOS(iff);

            if (!OpenIFF(iff, save ? IFFF_WRITE : IFFF_READ))
            {
                if (!save)
                {
                    if (!StopChunk(iff, ID_PREF, ID_MUIO)
                        && !ParseIFF(iff, IFFPARSE_SCAN))
                        DoMethod(dataspace, MUIM_Dataspace_ReadIFF, iff,
                            ID_PREF, ID_MUIO);
                }
                else if (!PushChunk(iff, ID_PREF, ID_FORM, IFFSIZE_UNKNOWN))
                {
                    if (!PushChunk(iff, ID_PREF, ID_PRHD,
                            sizeof(struct FilePrefHeader)))
                    {
                        struct FilePrefHeader head;
                        BOOL written;

                        memset(&head, 0, sizeof(head));
                        written = WriteChunkBytes(iff, &head,
                            sizeof(head)) == sizeof(head);
                        PopChunk(iff);
                        if (written)
                            DoMethod(dataspace, MUIM_Dataspace_WriteIFF,
                                iff, ID_PREF, ID_MUIO);
                    }
                    PopChunk(iff);
                }
                CloseIFF(iff);
            }
            FreeIFF(iff);
        }
        Close(fh);
    }

    if (!save)
        app_each_window(data, MUIM_Import, (IPTR)dataspace);

    MUI_DisposeObject(dataspace);

    return 0;
}

/* MUIM_Application_Execute/Run (MUI 4): the MUI 5 SDK's demos run their
 * loop with it; 82 bytes, in the library (as lean) rather than a module
 * each of them would load */
static IPTR Application__MUIM_Execute(Class *CLASS, Object *self,
    Msg message)
{
    IPTR signals = 0L;

    while
        (DoMethod(self, MUIM_Application_NewInput, (IPTR) & signals)
        != MUIV_Application_ReturnID_Quit)
    {
        if (signals)
        {
            signals = Wait(signals | SIGBREAKF_CTRL_C);
            if (signals & SIGBREAKF_CTRL_C)
                break;
        }
    }

    return 0;
}

/* UnpushMethod (Popasl sends it when disposed with its requester's
 * process) and UpdateMenus (every menu item that changes sends it) stay
 * in the library, not the module */
static IPTR Application__MUIM_UnpushMethod(struct IClass *cl, Object *obj,
    struct MUIP_Application_UnpushMethod *msg)
{
    D(bug("[Application__MUIM_UnpushMethod] dest %p id %p method %u\n",
        msg->dest, msg->methodid, msg->method));

    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    struct MQNode *current, *next;
    ULONG removed = 0;

    ObtainSemaphore(&data->app_MethodSemaphore);
    ForeachNodeSafe(&data->app_MethodQueue, current, next)
    {
        D(bug("[Application__MUIM_UnpushMethod] examine dest %p id %p "
            "method %u\n",
            current->mq_Dest, current, current->mq_Msg[0]));
        if (((msg->dest == NULL) || (msg->dest == current->mq_Dest))
            && ((msg->methodid == 0) || (msg->methodid == (IPTR)current))
            && ((msg->method == 0) || (msg->method == current->mq_Msg[0]))
        )
        {
            Remove((struct Node*)current);
            DeleteMQNode(current);
            removed++;
            D(bug("[Application__MUIM_UnpushMethod] current %p removed\n",
                current));
        }
    }
    ReleaseSemaphore(&data->app_MethodSemaphore);

    return removed;
}

static IPTR Application__MUIM_UpdateMenus(struct IClass *cl, Object *obj,
    Msg message)
{
    struct List *wlist = NULL;
    APTR wstate;
    Object *curwin;

    get(obj, MUIA_Application_WindowList, &wlist);

    if (wlist)
    {
        wstate = wlist->lh_Head;
        while ((curwin = NextObject(&wstate)))
        {
            DoMethod(curwin, MUIM_Window_UpdateMenu);
        }
    }

    return 0;
}

/* MUIM_Application_Get/SetMenuCheck/State (MUI V4, which old programs
 * with MUIA_Application_Menu use: in the library, not the module): the
 * first menu item with the MenuID in each window's menustrip, or the
 * application's for a window without one */
static __attribute__((noinline)) IPTR app_menu(
    struct MUI_ApplicationData *data, ULONG id, IPTR value, BOOL setting,
    ULONG attr)
{
    struct MinList *children;
    Object *state, *child, *strip, *item;

    if (!(children = app_windows(data)))
        return 0;
    state = (Object *)children->mlh_Head;
    while ((child = NextObject(&state)))
    {
        strip = NULL;
        get(child, MUIA_Window_Menustrip, &strip);
        if (!strip) strip = data->app_Menustrip;
        item = strip ? (Object *)DoMethod(strip, MUIM_FindUData, id) : NULL;
        if (!item) continue;
        if (setting)
        {
            set(item, attr, value);
            continue;
        }
        value = 0;
        get(item, attr, &value);
        return value;
    }
    return 0;
}

static IPTR Application__Menu(struct IClass *cl, Object *obj, ULONG *msg)
{
    return app_menu(INST_DATA(cl, obj), msg[1], msg[2],
        msg[0] == MUIM_Application_SetMenuCheck
        || msg[0] == MUIM_Application_SetMenuState,
        msg[0] == MUIM_Application_GetMenuCheck
        || msg[0] == MUIM_Application_SetMenuCheck
        ? MUIA_Menuitem_Checked : MUIA_Menuitem_Enabled);
}

/* The methods Application answers, by MethodID (unsigned, ascending: the
 * dispatcher halves the table to find one; the compiler checks the order).
 * The rarely used ones go to the application module (zune_app_method).
 * NewInput, the event loop's, and the other input methods are tested
 * before: they run with enough stack (zune_deep_call). */
static const struct { ULONG id; APTR method; } app_methods[]
    __attribute__((used)) =
{
    { OM_NEW, (APTR)Application__OM_NEW },
    { OM_DISPOSE, (APTR)Application__OM_DISPOSE },
    { OM_SET, (APTR)Application__OM_SET },
    { OM_GET, (APTR)Application__OM_GET },
    { OM_ADDMEMBER, (APTR)Application__OM_ADDMEMBER },
    { OM_REMMEMBER, (APTR)Application__OM_REMMEMBER },
    { MUIM_FindObject, (APTR)Application__MUIM_FindUData },
    { MUIM_Application_UnpushMethod, (APTR)Application__MUIM_UnpushMethod },
    { MUIM_Application_Save, (APTR)Application__MUIM_LoadSave },
    { MUIM_Application_SetConfigItem, (APTR)Application__MUIM_SetConfigItem },
    { MUIM_Application_CheckRefresh, (APTR)Application__MUIM_CheckRefresh },
    { MUIM_Application_Execute, (APTR)Application__MUIM_Execute },
    { MUIM_Application_ShowHelp, (APTR)zune_app_method },
    { MUIM_Application_ReturnID, (APTR)Application__MUIM_ReturnID },
    { MUIM_Application_SetMenuState, (APTR)Application__Menu },
    { MUIM_Application_OpenConfigWindow, (APTR)zune_app_method },
    { MUIM_Application_PushMethod, (APTR)Application__MUIM_PushMethod },
    { MUIM_Application_GetMenuState, (APTR)Application__Menu },
    { MUIM_Application_SetMenuCheck, (APTR)Application__Menu },
    { MUIM_Application_GetMenuCheck, (APTR)Application__Menu },
    { MUIM_FindUData, (APTR)Application__MUIM_FindUData },
    { MUIM_SetUData, (APTR)Application__MUIM_SetUData },
    { MUIM_SetUDataOnce, (APTR)Application__MUIM_SetUDataOnce },
    { MUIM_Application_AboutMUI, (APTR)zune_app_method },
    { MUIM_Application_RemInputHandler, (APTR)Application__MUIM_RemInputHandler },
    { MUIM_GetUData, (APTR)Application__MUIM_GetUData },
    { MUIM_Application_AddInputHandler, (APTR)Application__MUIM_AddInputHandler },
    { MUIM_Application_Load, (APTR)Application__MUIM_LoadSave },
    { MUIM_Application_SetConfigdata, (APTR)Application__MUIM_SetConfigdata },
    { MUIM_Application_OpenWindows, (APTR)Application__MUIM_OpenWindows },
    { MUIM_Application_Run, (APTR)Application__MUIM_Execute },
    { MUIM_Application_UpdateMenus, (APTR)Application__MUIM_UpdateMenus },
};
_Static_assert((ULONG)OM_NEW < (ULONG)OM_DISPOSE, "app_methods order");
_Static_assert((ULONG)OM_DISPOSE < (ULONG)OM_SET, "app_methods order");
_Static_assert((ULONG)OM_SET < (ULONG)OM_GET, "app_methods order");
_Static_assert((ULONG)OM_GET < (ULONG)OM_ADDMEMBER, "app_methods order");
_Static_assert((ULONG)OM_ADDMEMBER < (ULONG)OM_REMMEMBER, "app_methods order");
_Static_assert((ULONG)OM_REMMEMBER < (ULONG)MUIM_FindObject, "app_methods order");
_Static_assert((ULONG)MUIM_FindObject < (ULONG)MUIM_Application_UnpushMethod, "app_methods order");
_Static_assert((ULONG)MUIM_Application_UnpushMethod < (ULONG)MUIM_Application_Save, "app_methods order");
_Static_assert((ULONG)MUIM_Application_Save < (ULONG)MUIM_Application_SetConfigItem, "app_methods order");
_Static_assert((ULONG)MUIM_Application_SetConfigItem < (ULONG)MUIM_Application_CheckRefresh, "app_methods order");
_Static_assert((ULONG)MUIM_Application_CheckRefresh < (ULONG)MUIM_Application_Execute, "app_methods order");
_Static_assert((ULONG)MUIM_Application_Execute < (ULONG)MUIM_Application_ShowHelp, "app_methods order");
_Static_assert((ULONG)MUIM_Application_ShowHelp < (ULONG)MUIM_Application_ReturnID, "app_methods order");
_Static_assert((ULONG)MUIM_Application_ReturnID < (ULONG)MUIM_Application_SetMenuState, "app_methods order");
_Static_assert((ULONG)MUIM_Application_SetMenuState < (ULONG)MUIM_Application_OpenConfigWindow, "app_methods order");
_Static_assert((ULONG)MUIM_Application_OpenConfigWindow < (ULONG)MUIM_Application_PushMethod, "app_methods order");
_Static_assert((ULONG)MUIM_Application_PushMethod < (ULONG)MUIM_Application_GetMenuState, "app_methods order");
_Static_assert((ULONG)MUIM_Application_GetMenuState < (ULONG)MUIM_Application_SetMenuCheck, "app_methods order");
_Static_assert((ULONG)MUIM_Application_SetMenuCheck < (ULONG)MUIM_Application_GetMenuCheck, "app_methods order");
_Static_assert((ULONG)MUIM_Application_GetMenuCheck < (ULONG)MUIM_FindUData, "app_methods order");
_Static_assert((ULONG)MUIM_FindUData < (ULONG)MUIM_SetUData, "app_methods order");
_Static_assert((ULONG)MUIM_SetUData < (ULONG)MUIM_SetUDataOnce, "app_methods order");
_Static_assert((ULONG)MUIM_SetUDataOnce < (ULONG)MUIM_Application_AboutMUI, "app_methods order");
_Static_assert((ULONG)MUIM_Application_AboutMUI < (ULONG)MUIM_Application_RemInputHandler, "app_methods order");
_Static_assert((ULONG)MUIM_Application_RemInputHandler < (ULONG)MUIM_GetUData, "app_methods order");
_Static_assert((ULONG)MUIM_GetUData < (ULONG)MUIM_Application_AddInputHandler, "app_methods order");
_Static_assert((ULONG)MUIM_Application_AddInputHandler < (ULONG)MUIM_Application_Load, "app_methods order");
_Static_assert((ULONG)MUIM_Application_Load < (ULONG)MUIM_Application_SetConfigdata, "app_methods order");
_Static_assert((ULONG)MUIM_Application_SetConfigdata < (ULONG)MUIM_Application_OpenWindows, "app_methods order");
_Static_assert((ULONG)MUIM_Application_OpenWindows < (ULONG)MUIM_Application_Run, "app_methods order");
_Static_assert((ULONG)MUIM_Application_Run < (ULONG)MUIM_Application_UpdateMenus, "app_methods order");

#define APP_METHODS_LAST 248   /* (entries - 1) * 8 */
_Static_assert(sizeof(app_methods) == (APP_METHODS_LAST + 8), "APP_METHODS_LAST");
#define ZMC_STR2(x) #x
#define ZMC_STR(x) ZMC_STR2(x)

/* The function of a method in app_methods, NULL if Application has none:
 * the table halved, in assembly that keeps every register but d0 (the id
 * in, the function out) and d1, so the dispatcher keeps its arguments in
 * a0-a2 and its frame as small as a switch's */
asm(
    "\t.text\n\t.even\n"
    "_app_method_find:\n"
    "\tmovem.l %a0-%a1,-(%sp)\n"
    "\tlea _app_methods,%a0\n"                     /* lo */
    "\tlea _app_methods+" ZMC_STR(APP_METHODS_LAST) ",%a1\n" /* hi */
    "1:\tcmp.l %a0,%a1\n"
    "\tbcs.s 8f\n"                                 /* hi < lo: none */
    "\tmove.l %a1,%d1\n"
    "\tsub.l %a0,%d1\n"
    "\tlsr.l #4,%d1\n"
    "\tlsl.l #3,%d1\n"                             /* the middle entry */
    "\tcmp.l 0(%a0,%d1.l),%d0\n"
    "\tbeq.s 7f\n"
    "\tbcs.s 2f\n"
    "\tlea 8(%a0,%d1.l),%a0\n"                     /* id above: lo */
    "\tbra.s 1b\n"
    "2:\tlea -8(%a0,%d1.l),%a1\n"                  /* id below: hi */
    "\tbra.s 1b\n"
    "7:\tmove.l 4(%a0,%d1.l),%d0\n"
    "\tmovem.l (%sp)+,%a0-%a1\n"
    "\trts\n"
    "8:\tmoveq #0,%d0\n"
    "\tmovem.l (%sp)+,%a0-%a1\n"
    "\trts\n");

static inline APTR AppMethod(ULONG id)
{
    register ULONG d0 __asm("d0") = id;

    __asm volatile ("jsr _app_method_find" : "+d"(d0) : : "d1", "cc", "memory");
    return (APTR)d0;
}

BOOPSI_DISPATCHER(IPTR, Application_Dispatcher, cl, obj, msg)
{
    IPTR (*method)(struct IClass *, Object *, Msg);

    if (msg->MethodID == MUIM_Application_NewInput)
        method = (APTR)Application__MUIM_NewInput;
    else if (msg->MethodID == MUIM_Application_InputBuffered)
        method = (APTR)Application__MUIM_InputBuffered;
    else if (msg->MethodID == MUIM_Application_Input)
        method = (APTR)Application__MUIM_Input;
    else if ((method = AppMethod(msg->MethodID)))
        return method(cl, obj, msg);
    else
        return DoSuperMethodA(cl, obj, msg);
    return DEEP(method, cl, obj, msg);
}
BOOPSI_DISPATCHER_END
/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUI_Application_desc =
{
    MUIC_Application,
    MUIC_Notify,
    sizeof(struct MUI_ApplicationData),
    (void *) Application_Dispatcher
};
