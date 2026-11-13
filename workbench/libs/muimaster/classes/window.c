/*
    Copyright (C) 2002-2026, The AROS Development Team.
    Copyright (C) 1999, David Le Corfec.
    All rights reserved.

*/
#include <exec/types.h>
#include <exec/memory.h>
#include <aros/atomic.h>

#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include <intuition/imageclass.h>
#include <intuition/icclass.h>
#include <intuition/gadgetclass.h>
#ifdef __AROS__
#include <intuition/extensions.h>
#endif
#include <clib/alib_protos.h>
#include <graphics/gfxmacros.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/commodities.h>
#include <proto/layers.h>
#include <proto/gadtools.h>
#include <proto/muimaster.h>
#include <proto/workbench.h>

#ifndef MUI_OBSOLETE
#define MUI_OBSOLETE
#endif //            /* for the obsolete menu stuff */

#include "mui.h"
#include "support.h"
#include "pointer.h"
#include "classes/window.h"
#include "classes/area.h"
#include "imspec.h"
#include "datatypescache.h"
#include "prefs.h"
#include "dragndrop.h"
#include "dragwin.h"
#define wd_DragObject wd_Drag.object
#define wd_DropWindow wd_Drag.dropwin
#define wd_DropObject wd_Drag.drop
#define wd_dnd wd_Drag.dnd
#define wd_DragImage wd_Drag.image
#include "modules.h"
#ifdef ZUNE68_GCC_NATIVE
#define TITLEGLYPHS_NAMES_ONLY
#include "titleglyphs.h"   /* TG_*: the title gadgets */
#endif

#include "muimaster_intern.h"
#include "font.h"
#ifdef ZUNE68_GCC_NATIVE
#include <proto/cybergraphics.h>
#endif

#include <proto/muiscreen.h>

//#define MYDEBUG 1
#include "debug.h"

#define DPS(x)

//#define ZUNE_WINDOWCLASS_FORCEFANCYPENS

extern struct Library *MUIMasterBase;

/****** Window.mui/MUIC_Window ************
*
*   NAME
*       MUIC_Window
*
*   SUPERCLASS
*       Notify.mui
*
*   DESCRIPTION
*       Manage an Intuition window, one owned Area-compatible root object, an
*       optional owned local menustrip, and display resources. Attach the Window
*       to a live Application before opening it. Opening needs the application's
*       preferences, ports, and global information. Construction itself does not
*       open an Intuition window.
*       Application owns an attached Window. Window owns its current root and
*       local menustrip; it borrows the application's fallback menustrip.
*       Titles, screen selectors, reference/default/active objects, and
*       registered event-handler nodes are borrowed. Keep borrowed objects and
*       storage alive while used. Perform window operations and input processing
*       in the application's task.
*       Remove a window from its application before disposing it separately.
*       OM_DISPOSE closes an open window, disposes the current root and local
*       menustrip and deletes the private pool, but does not remove application
*       membership. Normal application removal disconnects and closes an open
*       child before disposal.
*
*   METHODS
*       FindUData and GetUData match the window first, then search its root
*       subtree. GetUData reports a match without checking getter success.
*       SetUData visits the window and root and returns TRUE; SetUDataOnce stops
*       at the first match. Neither traversal searches the local menustrip.
*       Export and Import forward to the root without checking for NULL and
*       return zero regardless of the child's result. Provide a root before
*       application state loading or saving.
*       The private ShowMe handling supports application iconification by
*       closing and remembering open windows. It is not an Area visibility
*       implementation. Seven Window methods marked PRIV in window.h remain
*       internal implementation contracts, not public interfaces.
*
*   NOTES
*       Construction returns NULL on superclass or pool failure, or an explicit
*       NULL RootObject tag. Omitting RootObject is allowed, but such an object
*       cannot open. Setter tags reach Notify after local processing;
*       notification delivery is not proof that an operation succeeded. Query
*       Open to determine the actual open state.
*       Handlers may be removed, and the window disposed, during its synchronous
*       callback traversal. Event data is borrowed only for the call.
*       RenderInfo screen, draw-info and font resources belong to the
*       setup/cleanup interval; the native window and drawing port belong to
*       the show/hide interval.
*
*   BUGS
*       Opening has incomplete checks for optional images, scrollbars, menus
*       and AppWindow registration. Setup success does not guarantee that every
*       optional resource exists. The AppWindow handle is not removed by this
*       file's close or disposal paths. See AppWindow, Setup and AllocGadgetID
*       for specific limitations.
*
*/
/****** Window.mui/MUIA_Window_Activate ************
*
*   NAME
*       MUIA_Window_Activate
*
*   FUNCTION
*       Choose activation on opening; default TRUE. Setting TRUE on an open
*       window calls ActivateWindow, except while the window reports its own
*       activation event. Getting returns TRUE only when the window is open and
*       its active flag is set.
*
*   INPUTS
*       BOOL; initialize, set, get.
*
*   NOTES
*       Setting FALSE while open does not deactivate the Intuition window. While
*       closed it changes the next-open activation request. Activation events
*       update the local flag and pass notification tags to Notify.
*
*/
/****** Window.mui/MUIA_Window_ActiveObject ************
*
*   NAME
*       MUIA_Window_ActiveObject
*
*   FUNCTION
*       Select keyboard focus. None clears focus; Next and Prev traverse the
*       cycle chain. While closed, a setter stores the request for the next
*       opening. The getter returns the active object only if it is found in the
*       current root subtree, otherwise NULL.
*
*   INPUTS
*       Object pointer or MUIV_Window_ActiveObject_None, Next, Prev; set and
*       get.
*
*   NOTES
*       Objects remain borrowed. An explicit target is stored without ownership
*       or membership validation; use a live descendant. GoInactive and GoActive
*       depend on setup/draw state. Raw-key processing tries active, parent,
*       default and other handlers.
*
*   BUGS
*       Next/Prev filter disabled or hidden objects during traversal, but
*       wraparound falls back to the first/last chain entry without that
*       filtering. The raw-key parent loop starts at the active object again, so
*       an unconsumed key can reach it twice. Initialization tags are not
*       handled.
*
*/
/****** Window.mui/MUIA_Window_DefaultObject ************
*
*   NAME
*       MUIA_Window_DefaultObject
*
*   FUNCTION
*       Store the fallback raw-key target, initially NULL. It is tried after the
*       active-object/parent path if distinct from the active object and not
*       disabled.
*
*   INPUTS
*       Object pointer; initialize, set, get.
*
*   NOTES
*       The pointer is borrowed, not added to the root or cycle chain. Keep it
*       alive and compatible with Area while registered.
*
*/
/****** Window.mui/MUIA_Window_Backdrop ************
*
*   NAME
*       MUIA_Window_Backdrop
*
*   FUNCTION
*       Set or clear WFLG_BACKDROP for native window creation. Default FALSE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*        There is no Window setter or getter for this attribute. Final
*       appearance also depends on Intuition and other creation flags.
*
*/
/****** Window.mui/MUIA_Window_Borderless ************
*
*   NAME
*       MUIA_Window_Borderless
*
*   FUNCTION
*       Set or clear WFLG_BORDERLESS for native window creation. Default FALSE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       Root spacing defaults to zero for a borderless window. There is no
*       Window setter or getter for this attribute. Final appearance also
*       depends on Intuition and other creation flags.
*
*/
/****** Window.mui/MUIA_Window_CloseGadget ************
*
*   NAME
*       MUIA_Window_CloseGadget
*
*   FUNCTION
*       Set or clear WFLG_CLOSEGADGET for native window creation. Default TRUE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       A close click raises CloseRequest; it does not close the window. There
*       is no Window setter or getter for this attribute. Final appearance also
*       depends on Intuition and other creation flags.
*
*/
/****** Window.mui/MUIA_Window_DepthGadget ************
*
*   NAME
*       MUIA_Window_DepthGadget
*
*   FUNCTION
*       Set or clear WFLG_DEPTHGADGET for native window creation. Default TRUE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*        There is no Window setter or getter for this attribute. Final
*       appearance also depends on Intuition and other creation flags.
*
*/
/****** Window.mui/MUIA_Window_DragBar ************
*
*   NAME
*       MUIA_Window_DragBar
*
*   FUNCTION
*       Set or clear WFLG_DRAGBAR for native window creation. Default TRUE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       This requests an Intuition drag bar, not object drag-and-drop. There is
*       no Window setter or getter for this attribute. Final appearance also
*       depends on Intuition and other creation flags.
*
*/
/****** Window.mui/MUIA_Window_SizeGadget ************
*
*   NAME
*       MUIA_Window_SizeGadget
*
*   FUNCTION
*       Set or clear WFLG_SIZEGADGET for native window creation. Default TRUE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       Opening removes the size-gadget flag when both minimum and maximum
*       dimensions are equal. There is no Window setter or getter for this
*       attribute. Final appearance also depends on Intuition and other creation
*       flags.
*
*/
/****** Window.mui/MUIA_Window_SizeRight ************
*
*   NAME
*       MUIA_Window_SizeRight
*
*   FUNCTION
*       Set or clear WFLG_SIZEBRIGHT for native window creation. Default FALSE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       Without this flag, opening selects a bottom-border size gadget. There is
*       no Window setter or getter for this attribute. Final appearance also
*       depends on Intuition and other creation flags.
*
*/
/****** Window.mui/MUIA_Window_ZoomGadget ************
*
*   NAME
*       MUIA_Window_ZoomGadget
*
*   FUNCTION
*       Enable WA_Zoom with the calculated alternate rectangle when opening a
*       window with a size gadget. Default TRUE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       There is no setter or getter. Alternate dimensions are initialization
*       values; see AltWidth and AltHeight for calculation limits.
*
*/
/****** Window.mui/MUIA_Window_ToolBox ************
*
*   NAME
*       MUIA_Window_ToolBox
*
*   FUNCTION
*       Request toolbox behavior. Default FALSE. On AROS, opening adds
*       WFLG_TOOLBOX; WA_ToolBox is also passed when that tag is available at
*       compile time.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       This class does not implement a setter or getter and does not
*       independently implement Intuition toolbox policy.
*
*/
/****** Window.mui/MUIA_Window_EraseArea ************
*
*   NAME
*       MUIA_Window_EraseArea
*
*   FUNCTION
*       Set the flag used when redrawing the window background. Setup also
*       forces the flag on when the preferences select redraw with clearing or
*       smart refresh.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       The zero-initialized flag is not explicitly set TRUE by the constructor;
*       the header's unconditional TRUE default does not describe this
*       implementation. There is no getter or setter. Not every background
*       drawing path tests this flag.
*
*/
/****** Window.mui/MUIA_Window_AppWindow ************
*
*   NAME
*       MUIA_Window_AppWindow
*
*   FUNCTION
*       When TRUE, attempt AddAppWindowA after opening the native window, using
*       the application AppPort and this object as user data. Default FALSE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       Delivered AppMessage tags are forwarded synchronously to the root and
*       children whose bounds contain the drop coordinates. Message ownership
*       remains with the application input path; recipients must not retain or
*       reply to it.
*
*   BUGS
*       Registration failure is ignored. No RemoveAppWindow call appears in this
*       file's close or disposal paths, so registration lifetime is not
*       correctly paired with window lifetime here.
*
*/
/****** Window.mui/MUIA_Window_CloseRequest ************
*
*   NAME
*       MUIA_Window_CloseRequest
*
*   FUNCTION
*       A close IDCMP event and the configured close key set this tag to TRUE
*       for Notify delivery. The Window getter always returns FALSE.
*
*   INPUTS
*       BOOL notification event; getter implemented.
*
*   NOTES
*       Register a notification and decide whether to close or quit in the
*       application. No pending-close state is stored and this event does not
*       itself close or dispose the window.
*
*/
/****** Window.mui/MUIA_Window_DisableKeys ************
*
*   NAME
*       MUIA_Window_DisableKeys
*
*   FUNCTION
*       Store a bit mask, initially zero, tested as 1 shifted by the matched
*       MUIKEY value before Window performs built-in keyboard actions.
*
*   INPUTS
*       ULONG mask; initialize, set, get.
*
*   NOTES
*       Disabling a built-in action does not suppress raw-key delivery to
*       handlers or erase the matched muikey argument. This is not a general
*       input-disable switch; Sleep has separate behavior.
*
*/
/****** Window.mui/MUIA_Window_ID ************
*
*   NAME
*       MUIA_Window_ID
*
*   FUNCTION
*       Store the identifier used to find application window-position records.
*       Default zero.
*
*   INPUTS
*       ULONG; initialize, set, get.
*
*   NOTES
*       Choose distinct nonzero identifiers for windows whose positions should
*       be distinguished. The class neither generates nor validates uniqueness.
*       Zero is not searched by Application_GetWinPos and disables
*       preference-driven close snapshots.
*
*/
/****** Window.mui/MUIA_Window_IsSubWindow ************
*
*   NAME
*       MUIA_Window_IsSubWindow
*
*   FUNCTION
*       Store a normalized subwindow flag, initially FALSE.
*
*   INPUTS
*       BOOL; initialize, set, get.
*
*   NOTES
*       The flag is exposed to callers but does not itself change this class's
*       opening, activation, ownership or event processing.
*
*/
/****** Window.mui/MUIA_Window_InputEvent ************
*
*   NAME
*       MUIA_Window_InputEvent
*
*   FUNCTION
*       Before raw-key dispatch, set this tag to a temporary InputEvent
*       containing the raw key, qualifiers, timestamp and event-address
*       information.
*
*   INPUTS
*       InputEvent pointer carried by a notification tag; no implemented getter.
*
*   NOTES
*       Use MUIV_TriggerValue in a synchronous notification to inspect the
*       event. The structure is on the handler's stack and must not be retained.
*       It does not provide an input queue or a persistent last-event value. The
*       header's get flag does not correspond to an OM_GET case.
*
*/
/****** Window.mui/MUIA_Window_MenuAction ************
*
*   NAME
*       MUIA_Window_MenuAction
*
*   FUNCTION
*       Menu selection sets this tag to the selected menu item's UserData, also
*       sets Application_MenuAction, and queues that value through
*       Application_ReturnID.
*
*   INPUTS
*       ULONG notification value; no implemented Window storage or getter.
*
*   NOTES
*       Use notification trigger data. Window does not retain the last action. A
*       setter tag can trigger Notify even though this attribute has no Window
*       field. The native menu path handles one selected item, not a complete
*       NextSelect chain.
*
*/
/****** Window.mui/MUIA_Window_Menustrip ************
*
*   NAME
*       MUIA_Window_Menustrip
*
*   FUNCTION
*       Supply an owned local Menustrip, initially NULL. Opening uses this
*       object if present, otherwise borrows Application_Menustrip.
*
*   INPUTS
*       Object pointer; initialize and get.
*
*   NOTES
*       The getter returns the local object, otherwise the application fallback
*       in use while open. Window connects/disconnects the local menu with its
*       parent and disposes it at disposal. There is no replacement setter. Do
*       not share one owned menu tree between owners. Native menus are created
*       for opening and freed on closing or rebuilding.
*
*/
/****** Window.mui/MUIA_Window_NoMenus ************
*
*   NAME
*       MUIA_Window_NoMenus
*
*   FUNCTION
*       Store a right-mouse-button trap request, initially FALSE. Opening passes
*       WA_RMBTrap when true or when the window gets no menu; a setter updates
*       WFLG_RMBTRAP on the native window immediately by the same rule.
*
*   INPUTS
*       BOOL; initialize and set.
*
*   NOTES
*       No getter is implemented. Mouse-move context-menu detection also adjusts
*       the native trap flag. This does not dispose or remove the menustrip.
*
*/
/****** Window.mui/MUIA_Window_Open ************
*
*   NAME
*       MUIA_Window_Open
*
*   FUNCTION
*       Request opening or closing. Opening requires a root and a connected
*       application, then sets up display resources and the root, computes
*       layout and opens Intuition. Closing hides and cleans up the root and
*       releases display resources. The getter returns the actual open flag.
*
*   INPUTS
*       BOOL; set and get.
*
*   NOTES
*       Initially FALSE; initialization tags are ignored. A TRUE setter on an
*       already open window brings it forward and activates it. While hidden by
*       iconification, TRUE records a pending reopen and FALSE cancels it.
*       Failed opening leaves Open FALSE, but the setter forwards the original
*       tag to Notify and does not return the open result. Query Open after
*       requesting it.
*
*   BUGS
*       Optional resource failures are not all detected. Close does not reset
*       Sleep's nesting counter or remove an AppWindow registration.
*
*/
/****** Window.mui/MUIA_Window_RootObject ************
*
*   NAME
*       MUIA_Window_RootObject
*
*   FUNCTION
*       Install the owned Area-compatible root. An explicit NULL construction
*       tag fails construction; omission leaves no root. A setter while closed
*       disconnects the old root, installs the new pointer and connects it if
*       application global information exists.
*
*   INPUTS
*       Object pointer; initialize, set while closed, get.
*
*   NOTES
*       A setter while open is ignored. Replacement does not dispose the old
*       root: retain its pointer and manage its lifetime after detaching it. The
*       current root is disposed with the window. NULL is accepted by the setter
*       but prevents opening and is unsafe for Import/Export.
*
*   BUGS
*       Replacement clears active focus only when it equals the old root, not
*       when it points to a descendant. Ensure borrowed focus/default targets do
*       not outlive their objects.
*
*/
/****** Window.mui/MUIA_Window_Title ************
*
*   NAME
*       MUIA_Window_Title
*
*   FUNCTION
*       Store the window title pointer, initially NULL. When open, a setter
*       calls SetWindowTitles for that title and leaves the other unchanged.
*
*   INPUTS
*       STRPTR; initialize, set, get.
*
*   NOTES
*       The string is borrowed, not copied or freed. Keep it readable and
*       NUL-terminated while used. The getter returns the stored pointer. When
*       opening, a NULL pointer omits the corresponding Intuition title tag.
*
*/
/****** Window.mui/MUIA_Window_ScreenTitle ************
*
*   NAME
*       MUIA_Window_ScreenTitle
*
*   FUNCTION
*       Store the screen title associated with the window pointer, initially
*       NULL. When open, a setter calls SetWindowTitles for that title and
*       leaves the other unchanged.
*
*   INPUTS
*       STRPTR; initialize, set, get.
*
*   NOTES
*       The string is borrowed, not copied or freed. Keep it readable and
*       NUL-terminated while used. The getter returns the stored pointer. When
*       opening, a NULL pointer omits the corresponding Intuition title tag.
*
*/
/****** Window.mui/MUIA_Window_Screen ************
*
*   NAME
*       MUIA_Window_Screen
*
*   FUNCTION
*       Select a borrowed screen to use during the next setup. The getter
*       returns the actual render-info screen, not the requested pointer.
*
*   INPUTS
*       Screen pointer; initialize, set, get.
*
*   NOTES
*       A non-NULL supplied screen takes precedence over public-screen selection
*       and is not locked or closed as a caller-owned screen. Keep it open
*       throughout use. Setting it while open does not migrate the window.
*       Application custom screens have separate ownership.
*
*/
/****** Window.mui/MUIA_Window_PublicScreen ************
*
*   NAME
*       MUIA_Window_PublicScreen
*
*   FUNCTION
*       Store a borrowed public-screen name, initially NULL. With no user
*       screen, setup may first select an application custom screen from
*       preferences; otherwise it tries the requested public name, or the
*       preference-selected name when no explicit name was supplied. If that
*       attempt fails, it tries the default public screen.
*
*   INPUTS
*       STRPTR; initialize, set, get.
*
*   NOTES
*       The getter returns the requested string, not the actual selected screen
*       name. Keep it readable while stored. Public-screen locks are owned by
*       setup and released by cleanup. Setting a new name while open does not
*       move the window. Failure to lock the requested name can fall back to
*       another screen.
*
*/
/****** Window.mui/MUIA_Window_Window ************
*
*   NAME
*       MUIA_Window_Window
*
*   FUNCTION
*       Return the native window while open, otherwise NULL.
*
*   INPUTS
*       Intuition Window pointer; get only.
*
*   NOTES
*       The pointer is borrowed. Do not CloseWindow it, replace its shared
*       UserPort, consume its messages independently, or retain it across
*       closing. The Zune Window object and the native Window are distinct
*       objects.
*
*/
/****** Window.mui/MUIA_Window_Sleep ************
*
*   NAME
*       MUIA_Window_Sleep
*
*   FUNCTION
*       A nonzero setter on an open window increments a nesting counter. The
*       first increment installs a busy pointer and freezes the native size
*       limits. Zero decrements a nonzero counter; reaching zero restores
*       pointer and saved limits. The getter reports whether the counter is
*       nonzero.
*
*   INPUTS
*       BOOL; set and get.
*
*   NOTES
*       Initially zero. Balance each accepted sleep request with a wake request.
*       Sleeping input is replied to without ordinary dispatch; refresh events
*       are still processed. A sleep request while closed is ignored, but a wake
*       request can decrement a retained counter.
*
*   BUGS
*       Closing does not reset the counter. Reopening while it is nonzero does
*       not repeat the first-sleep setup. The WORD counter has no overflow
*       check; do not treat repeated TRUE as an idempotent assignment.
*
*/
/****** Window.mui/MUIA_Window_Width ************
*
*   NAME
*       MUIA_Window_Width
*
*   FUNCTION
*       Request the window width. Default MUIV_Window_Width_Default selects the
*       root's default dimension. Positive values request pixels. MinMax(p),
*       Screen(p), Visible(p) for p from 0 through 100, and Scaled are declared
*       encodings. Pixel, Screen and Visible values give the outer width
*       including borders; the others give the inner width. Results are clamped
*       to the root's computed minimum and maximum. The getter returns native
*       GZZWidth while open, otherwise zero.
*
*   INPUTS
*       LONG; initialize and get; setter also implemented.
*
*   NOTES
*       Screen and Visible both use screen dimensions rather than measuring the
*       unobscured viewport. Scaled derives this dimension from the other using
*       the root minimum aspect ratio; it does not guard division by zero. Do
*       not request both dimensions Scaled. A setter stores the request and
*       clears this cached dimension; it does not immediately resize the native
*       window.
*
*   BUGS
*       Both requests are evaluated only when cached width is zero. Setting
*       Height alone can leave its request unevaluated. Boundary encodings
*       overlap; the first matching MinMax, Screen or Visible branch wins.
*
*/
/****** Window.mui/MUIA_Window_Height ************
*
*   NAME
*       MUIA_Window_Height
*
*   FUNCTION
*       Request the window height. Default MUIV_Window_Height_Default selects
*       the root's default dimension. Positive values request pixels.
*       MinMax(p), Screen(p), Visible(p) for p from 0 through 100, and Scaled
*       are declared encodings. Pixel, Screen and Visible values give the outer
*       height including borders; the others give the inner height. Results
*       are clamped to the root's computed minimum and maximum. The getter
*       returns native GZZHeight while open, otherwise zero.
*
*   INPUTS
*       LONG; initialize and get; setter also implemented.
*
*   NOTES
*       Screen and Visible both use screen dimensions rather than measuring the
*       unobscured viewport. Scaled derives this dimension from the other using
*       the root minimum aspect ratio; it does not guard division by zero. Do
*       not request both dimensions Scaled. A setter stores the request and
*       clears this cached dimension; it does not immediately resize the native
*       window.
*
*   BUGS
*       Both requests are evaluated only when cached width is zero. Setting
*       Height alone can leave its request unevaluated.
*
*/
/****** Window.mui/MUIA_Window_RefWindow ************
*
*   NAME
*       MUIA_Window_RefWindow
*
*   FUNCTION
*       Store a borrowed Window object used when calculating the next opening
*       position. Default NULL.
*
*   INPUTS
*       Object pointer; initialize and set.
*
*   NOTES
*       Keep it alive through position calculation. An open reference supplies
*       its native window's outer box; a closed reference supplies zero. There
*       is no getter, no immediate repositioning, and no ownership transfer.
*       Centering uses the outer sizes of both windows.
*
*/
/****** Window.mui/MUIA_Window_LeftEdge ************
*
*   NAME
*       MUIA_Window_LeftEdge
*
*   FUNCTION
*       Request the next opening position. Default
*       MUIV_Window_LeftEdge_Centered. Moused centers about the screen pointer.
*       Ordinary values are screen coordinates without RefWindow, or offsets
*       from that reference's corresponding edge. The getter returns the actual
*       native edge while open, otherwise zero.
*
*   INPUTS
*       LONG; initialize and get; setter also implemented.
*
*   NOTES
*       Setting the request does not move an open window. Saved application
*       position records and Intuition auto-adjustment can override calculated
*       geometry.
*
*/
/****** Window.mui/MUIA_Window_TopEdge ************
*
*   NAME
*       MUIA_Window_TopEdge
*
*   FUNCTION
*       Request the next opening position. Default MUIV_Window_TopEdge_Centered.
*       Moused centers about the screen pointer. Ordinary values are screen
*       coordinates without RefWindow, or offsets from that reference's
*       corresponding edge. The getter returns the actual native edge while
*       open, otherwise zero.
*
*   INPUTS
*       LONG; initialize and get; setter also implemented.
*
*   NOTES
*       Setting the request does not move an open window. TopEdge_Delta(p) is
*       recognized only for p from 0 through 6, adding the offset below the
*       screen bar or reference top plus the estimated top border. Other Delta
*       encodings fall through as ordinary values. Saved application position
*       records and Intuition auto-adjustment can override calculated geometry.
*
*/
/****** Window.mui/MUIA_Window_AltWidth ************
*
*   NAME
*       MUIA_Window_AltWidth
*
*   FUNCTION
*       Store the alternate zoom width, default MUIV_Window_AltWidth_MinMax(0).
*       The getter returns the stored encoding, not the computed zoom rectangle.
*       Opening computes the rectangle and passes it to WA_Zoom when ZoomGadget
*       is true and the window has a size gadget.
*
*   INPUTS
*       LONG converted to the IBox WORD field; initialize and get.
*
*   NOTES
*       Positive pixels, MinMax(p), Screen(p) and Visible(p) are processed, then
*       clamped to content limits. Screen and Visible both use the full screen
*       dimension. Opening adds estimated borders after calculation. There is no
*       setter.
*
*   BUGS
*       MinMax(p) omits division by 100 for both alternate dimensions. The
*       declared Scaled encoding has no scaling branch and is merely clamped.
*       The WORD conversion can truncate large values.
*
*/
/****** Window.mui/MUIA_Window_AltHeight ************
*
*   NAME
*       MUIA_Window_AltHeight
*
*   FUNCTION
*       Store the alternate zoom height, default
*       MUIV_Window_AltHeight_MinMax(0). The getter returns the stored encoding,
*       not the computed zoom rectangle. Opening computes the rectangle and
*       passes it to WA_Zoom when ZoomGadget is true and the window has a size
*       gadget.
*
*   INPUTS
*       LONG converted to the IBox WORD field; initialize and get.
*
*   NOTES
*       Positive pixels, MinMax(p), Screen(p) and Visible(p) are processed, then
*       clamped to content limits. Screen and Visible both use the full screen
*       dimension. Opening adds estimated borders after calculation. There is no
*       setter.
*
*   BUGS
*       MinMax(p) omits division by 100 for both alternate dimensions. The
*       declared Scaled encoding has no scaling branch and is merely clamped.
*       The WORD conversion can truncate large values.
*
*/
/****** Window.mui/MUIA_Window_AltLeftEdge ************
*
*   NAME
*       MUIA_Window_AltLeftEdge
*
*   FUNCTION
*       Store an alternate zoom position, default
*       MUIV_Window_AltLeftEdge_NoChange. NoChange becomes Intuition's unchanged
*       coordinate. Centered uses the screen dimension and current normal inner
*       dimension. The getter returns the stored request.
*
*   INPUTS
*       LONG converted to the IBox WORD field; initialize and get.
*
*   NOTES
*       There is no setter. Moused is treated as unchanged rather than
*       mouse-centered. RefWindow is not applied to the alternate rectangle.
*
*/
/****** Window.mui/MUIA_Window_AltTopEdge ************
*
*   NAME
*       MUIA_Window_AltTopEdge
*
*   FUNCTION
*       Store an alternate zoom position, default
*       MUIV_Window_AltTopEdge_NoChange. NoChange becomes Intuition's unchanged
*       coordinate. Centered uses the screen dimension and current normal inner
*       dimension. The getter returns the stored request.
*
*   INPUTS
*       LONG converted to the IBox WORD field; initialize and get.
*
*   NOTES
*       There is no setter. Moused is treated as unchanged rather than
*       mouse-centered. The declared AltTopEdge_Delta(p) has no special
*       calculation and remains a negative coordinate. RefWindow is not applied
*       to the alternate rectangle.
*
*/
/****** Window.mui/MUIA_Window_UseBottomBorderScroller ************
*
*   NAME
*       MUIA_Window_UseBottomBorderScroller
*
*   FUNCTION
*       Request native bottom-border scrollbar gadgets during opening. Default
*       FALSE. Setup creates their system images and display creates their
*       gadget chain.
*
*   INPUTS
*       BOOL; initialize and set; no implemented getter.
*
*   NOTES
*       Setting the flag while open does not create or remove existing gadgets.
*       Use it before opening. Prop objects can associate with the native
*       scrollbars through RenderInfo; button updates then call Prop_Increase or
*       Prop_Decrease.
*
*   BUGS
*       System-image, gadget and gadget-ID allocation failures are not fully
*       checked. The header advertises a getter, but Window has no matching
*       getter case.
*
*/
/****** Window.mui/MUIA_Window_UseRightBorderScroller ************
*
*   NAME
*       MUIA_Window_UseRightBorderScroller
*
*   FUNCTION
*       Request native right-border scrollbar gadgets during opening. Default
*       FALSE. Setup creates their system images and display creates their
*       gadget chain.
*
*   INPUTS
*       BOOL; initialize and set; no implemented getter.
*
*   NOTES
*       Setting the flag while open does not create or remove existing gadgets.
*       Use it before opening. Prop objects can associate with the native
*       scrollbars through RenderInfo; button updates then call Prop_Increase or
*       Prop_Decrease.
*
*   BUGS
*       System-image, gadget and gadget-ID allocation failures are not fully
*       checked. The header advertises a getter, but Window has no matching
*       getter case.
*
*/
/****** Window.mui/MUIA_Window_FancyDrawing ************
*
*   NAME
*       MUIA_Window_FancyDrawing
*
*   FUNCTION
*       Declared in window.h, but not implemented by Window.
*
*   INPUTS
*       BOOL; declared initialize, set, get.
*
*   NOTES
*       There is no matching constructor, setter or getter behavior. Setter tags
*       can still reach Notify. The declaration alone does not provide the named
*       capability.
*
*/
/****** Window.mui/MUIA_Window_MouseObject ************
*
*   NAME
*       MUIA_Window_MouseObject
*
*   FUNCTION
*       Declared in window.h, but not implemented by Window.
*
*   INPUTS
*       Object pointer; declared get only.
*
*   NOTES
*       There is no matching constructor, setter or getter behavior. Setter tags
*       can still reach Notify. The declaration alone does not provide the named
*       capability.
*
*/
/****** Window.mui/MUIA_Window_NeedsMouseObject ************
*
*   NAME
*       MUIA_Window_NeedsMouseObject
*
*   FUNCTION
*       Declared in window.h, but not implemented by Window.
*
*   INPUTS
*       BOOL; declared initialize only.
*
*   NOTES
*       There is no matching constructor, setter or getter behavior. Setter tags
*       can still reach Notify. The declaration alone does not provide the named
*       capability.
*
*/
/****** Window.mui/MUIA_Window_UseLeftBorderScroller ************
*
*   NAME
*       MUIA_Window_UseLeftBorderScroller
*
*   FUNCTION
*       Declared in window.h, but not implemented by Window.
*
*   INPUTS
*       BOOL; declared initialize, set, get.
*
*   NOTES
*       There is no matching constructor, setter or getter behavior. Setter tags
*       can still reach Notify. The declaration alone does not provide the named
*       capability.
*
*/
/****** Window.mui/MUIA_Window_Menu ************
*
*   NAME
*       MUIA_Window_Menu
*
*   FUNCTION
*       Declared in window.h, but not implemented by Window.
*
*   INPUTS
*       NewMenu pointer; declared initialize only under MUI_OBSOLETE.
*
*   NOTES
*       There is no matching constructor, setter or getter behavior. Setter tags
*       can still reach Notify. Use Menustrip for the implemented menu-object
*       interface; this legacy tag is not an alias.
*
*/

#include "native_version.h"

#define IM(x) ((struct Image*)(x))
#define G(x) ((struct Gadget*)(x))
#define GADGETID(x) (((struct Gadget*)(x))->GadgetID)



struct MUI_ImageSpec_intern;

/* Client-owned handler nodes may be removed and freed during dispatch. */
struct WindowEventCursor
{
    struct WindowEventCursor *previous;
    /* A node or list tail while alive; NULL if the Window was disposed. */
    struct MinNode *next;
};

/* What the area macros read on a Window object. The SDK's _window(),
 * _screen(), _rp(), ... are for area objects: on a Window object they
 * read the long right after the notify data as a MUI_RenderInfo pointer.
 * MUI 3.8 has a pointer there to a block whose public fields are all 0
 * (measured: _window(win) and _screen(win) are NULL, open or closed), so
 * a program that passes _window(win) as ASLFR_Window gets asl.library's
 * default screen. Zune had its own render info there, whose first field
 * (the window object) was then read as the render info: _window(win) was
 * a pointer into the notify data, which asl.library took for a window
 * (FALSE at once on an RTG screen, a reset on the A1200). The fields up
 * to mri_Flags, all 0. */
static const IPTR window_area_renderinfo[8];

struct MUI_WindowData
{
    const IPTR *wd_AreaRenderInfo;  /* window_area_renderinfo, see above */
    struct MUI_RenderInfo wd_RenderInfo;
    struct MUI_MinMax wd_MinMax;
    struct IBox wd_AltDim;      /* zoomed dimensions */
    BOOL wd_ZoomGadget;         /* enable/disable zoomgadget (altdim stuff) */
    /* The cycle chain (the objects Tab activates), in order, in one
     * block of wd_ChainSize pointers of which the first wd_ChainCount are
     * used; a removed object leaves NULL (skipped) until the block is
     * full, so that a window's cleanup, which removes its objects from the
     * front, moves nothing. The gadget IDs: one byte each in wd_IDMap
     * (wd_IDBytes; ID n is byte n - 1, not 0 while in use). Before 2026-11-13 both
     * were nodes in a memory pool of the window's own: a 1 KB puddle per
     * window. */
    Object **wd_Chain;
    UWORD wd_ChainCount, wd_ChainSize;
    UBYTE *wd_IDMap;
    UWORD wd_IDBytes;
    struct MinList wd_EHList;   /* event handlers */
    struct MinList wd_CCList;   /* control chars */
    ULONG wd_Events;            /* events received */
    ULONG wd_CrtFlags;          /* window creation flags, see below */
    Object *wd_ActiveObject;    /* the active object */
    Object *wd_OldActive;       /* active object before window was closed */
    APTR wd_DefaultObject;
    ULONG wd_ID;
    STRPTR wd_Title;
    STRPTR wd_ScreenTitle;
    LONG wd_Height;             /* Current dimensions */
    LONG wd_Width;
    LONG wd_X;
    LONG wd_Y;
    LONG wd_ReqHeight;          /* given by programmer */
    LONG wd_ReqWidth;
    LONG wd_ReqX;
    LONG wd_ReqY;
    APTR wd_RootObject;         /* unique child */
    ULONG wd_Flags;             /* various status flags */
    struct MUI_ImageSpec_intern *wd_Background;
    ULONG wd_DisabledKeys;
    BOOL wd_NoMenus;            /* MUIA_Window_NoMenus */

    struct ZuneDrag wd_Drag;    /* drag and drop (dragwin.h) */
    struct AppWindow *wd_AppWindow;

    Object *wd_Menustrip;       /* The menustrip object which is actually
                                 * used (either app's or window's or NULL) */
    Object *wd_ChildMenustrip;  /* If window has its own Menustrip */
    struct Menu *wd_Menu;       /* the intuition menustrip */

    Object *wd_VertProp;        /* these six in this order (UndisplayWindow) */
    Object *wd_UpButton;
    Object *wd_DownButton;
    Object *wd_HorizProp;
    Object *wd_LeftButton;
    Object *wd_RightButton;
    Object *wd_RefWindow;

    Object *wd_MUIGadget;

    Object *wd_HelpObject;
    APTR wd_HelpBubble;
    WORD wd_HelpTicker;

    struct Screen *wd_UserScreen;
    STRPTR wd_UserPublicScreen;

    WORD wd_SleepCount;         /* MUIA_Window_Sleep nests */
    LONG wd_SleepMaxHeight;     /* Remember Min/Max values for wakeup */
    LONG wd_SleepMaxWidth;
    LONG wd_SleepMinHeight;
    LONG wd_SleepMinWidth;

    struct IClass *wd_Class;
    Object **wd_CycleOrder; /* allocated only for the legacy explicit chain */
    /* Public mri_Pens contains only UWORD pen numbers. Keep ownership bits. */
    LONG wd_PenHandles[MPEN_COUNT];
#ifdef ZUNE68_GCC_NATIVE
    struct Gadget *wd_TitleGadgets[4]; /* extra title gadgets, TG_* */
    ULONG wd_SnapshotSeconds, wd_SnapshotMicros;
    BOOL wd_SnapshotClick;
#endif
    struct WindowEventCursor *wd_EventCursors;
    struct ZuneWindowGuard *wd_Guards;
    struct ZunePointer *wd_Pointer;
    IPTR wd_PointerType;
    LONG wd_Opacity;
};

#ifndef WFLG_SIZEGADGET

#define WFLG_CLOSEGADGET (1<<0) /* has close gadget */
#define WFLG_SIZEGADGET  (1<<1) /* has size gadget */
#define WFLG_BACKDROP    (1<<2) /* is backdrop window */
#define WFLG_BORDERLESS  (1<<3) /* has no borders */
#define WFLG_DEPTHGADGET (1<<4) /* has depth gadget */
#define WFLG_DRAGBAR     (1<<5) /* is draggable */
#define WFLG_SIZEBRIGHT  (1<<6) /* size gadget is in right border */

#endif

/* wd_Flags */
#define MUIWF_OPENED            (1<<0)  /* window currently opened */
#define MUIWF_HIDDEN            (1<<1)  /* window currently iconified */
#define MUIWF_ACTIVE            (1<<2)  /* window currently active */
#define MUIWF_RESIZING          (1<<4)  /* window currently resizing */
#define MUIWF_DONTACTIVATE      (1<<7)  /* do not activate the window when
                                         * opening */
#define MUIWF_USERIGHTSCROLLER  (1<<8)  /* should have right scroller */
#define MUIWF_USEBOTTOMSCROLLER (1<<9)  /* should have bottom scroller */
#define MUIWF_ERASEAREA         (1<<10) /* Erase area after a window resize */
#define MUIWF_ISAPPWINDOW       (1<<11) /* Is an AppWindow */
#define MUIWF_ISSUBWINDOW       (1<<12) /* Don't get automatically disposed
                                         * with app */
#define MUIWF_BUBBLEMODE        (1<<13) /* Quick bubble mode. Bubbles appear
                                         * quick when moving */
#define MUIWF_OPENONUNHIDE      (1<<14) /* Open the window when unhiding */
#define MUIWF_SCREENLOCKED      (1<<15) /* A pub screen was locked in
                                         * SetupRenderInfo. Unlock it in
                                         * CleanupRenderInfo! */
#define MUIWF_OBJECTGOACTIVESENT (1<<16) /* A MUIM_GoActive msg was sent to
                                          * window's active object */
#define MUIWF_TOOLBOX           (1<<17) /* Window should be opened as
                                         * ToolBox */
#define MUIWF_ACTIVEEVENT       (1<<18) /* Reporting IDCMP_ACTIVEWINDOW */
#define MUIWF_REFITPENDING      (1<<19) /* MUIM_Window_Refit pushed: the
                                         * contents outgrew the screen */
#define MUIWF_SCROLLWRAP        (1<<20) /* the root is MUI 5's Scrollgroup
                                         * around the program's root */

/* Bubble help delays in IntuiTicks (1/10 s), from the settings as on
 * MUI 3.8 (2.0 s and 0.3 s by default); Zune used fixed 1.0 and 0.3 s */
#define BUBBLEHELP_TICKER_DEFAULT 20
#define BUBBLEHELP_PREFS(data) \
    (muiGlobalInfo((data)->wd_RenderInfo.mri_WindowObject)->mgi_Prefs)
#define BUBBLEHELP_TICKER_FIRST \
    MAX(BUBBLEHELP_PREFS(data)->bubblehelp_first_delay, 1)
#define BUBBLEHELP_TICKER_LATER \
    MAX(BUBBLEHELP_PREFS(data)->bubblehelp_next_delay, 1)

struct __dummyXFC3__
{
    struct MUI_NotifyData mnd;
    struct MUI_WindowData mwd CLASS_INSTANCE_ALIGN;
};

#define muiWindowData(obj)   (&(((struct __dummyXFC3__ *)(obj))->mwd))




/* If the stored active object is in the cycle chain, send GoActive once
 * according to the sent flag; otherwise clear the active pointer. Requires
 * stable live chain members and callback-safe window lifetime. Return no
 * status; this does not choose a replacement focus object.
 */
static void ActivateObject(struct MUI_WindowData *data);
/* Dispatch a borrowed event to matching non-disabled handler objects. For
 * events other than IDCMPUPDATE, copy the message, clear IAddress and reply
 * before callbacks. IDCMPUPDATE retains the original until final ReplyMsg;
 * an Eat return currently bypasses that reply. Update the native context-
 * menu trap on mouse move. Return no status and require stable live handler
 * nodes.
 */
static void HandleInputEvent(Object *win, struct MUI_WindowData *data,
    struct IntuiMessage *event);

/* Compute a replicated 32-bit color component from the high bytes of a and
 * b using (a + 3*b)/4. Return the value without allocating or changing
 * state; inputs are color components, not pointers.
 */
static ULONG DoHalfshineGun(ULONG a, ULONG b)
{
    ULONG val = ((((a) >> 24) + 3 * ((b) >> 24)) / 4);
    val = val + (val << 8) + (val << 16) + (val << 24);
    return val;
}

/* Measured against MUI 3.9 with several shadow/background colours. */
/* Compute a replicated 32-bit color component from the high bytes of a and
 * b using (a + 5*b)/6. Return the value without allocating or changing
 * state.
 */
static ULONG DoHalfshadowGun(ULONG a, ULONG b)
{
    ULONG val = (46 * ((a) >> 24) + 212 * ((b) >> 24)) >> 8;
    val = val + (val << 8) + (val << 16) + (val << 24);
    return val;
}

/* Create a sysiclass image for which using the live borrowed screen
 * DrawInfo. Return an owned BOOPSI image or NULL; the caller must dispose a
 * successful image before releasing its DrawInfo.
 */
static Object *CreateSysimage(struct DrawInfo *dri, ULONG which)
{
    return NewObject(NULL, "sysiclass",
        SYSIA_DrawInfo, (IPTR) dri, SYSIA_Which, which, TAG_DONE);
}

/* Insert an unlinked live node in decreasing signed priority and then
 * decreasing address order. Borrow list and node, mutate their links, and
 * return no status. Membership is not validated; keep traversal and
 * insertion serialized.
 */
static void EnqueueByPriAndAddress(struct List *list, struct Node *node)
{
    struct Node *scannode;

    /* Sort by priority and by node address, so that a
       "remove - modify - enqueue" sequence will re-add
       the node at the same place in the list it was
       initially */
    ForeachNode(list, scannode)
    {
        if (((struct Node *)node)->ln_Pri > scannode->ln_Pri)
            break;
        if (((struct Node *)node)->ln_Pri == scannode->ln_Pri)
        {
            if ((IPTR) node > (IPTR) scannode)
                break;
        }
    }

    Insert(list, (struct Node *)node, scannode->ln_Pred);
}

/* The 16 custom frames of the settings (customframe_config_1 to _16, in
 * this order in the settings) through the frame cache; a slot whose frame
 * does not load is NULL. Requires live application preferences. */
static __attribute__((noinline)) void InitCustomFrames(Object *obj,
    struct MUI_RenderInfo *mri)
{
    CONST_STRPTR *spec =
        &muiGlobalInfo(obj)->mgi_Prefs->customframe_config_1;
    int i;

    for (i = 0; i < 16; i++)
        mri->mri_Tail->FrameImage[i] =
            load_custom_frame(spec[i], mri->mri_Screen);
}

/* Release each non-NULL custom-frame cache reference and clear its slot.
 * Requires initialized writable render-info state; return no status and do
 * not free the render-info structure.
 */
/* not inlined: CleanupRenderInfo's frame stays as small as before the
 * tail's NULL test */
static void __attribute__((noinline)) DisposeCustomFrames(struct MUI_RenderInfo *mri)
{
    int i;

    if (!mri->mri_Tail) /* never set up */
        return;
    for (i = 0; i < 16; i++)
    {
        dispose_custom_frame(mri->mri_Tail->FrameImage[i]);

        mri->mri_Tail->FrameImage[i] = NULL;
    }
}

static __attribute__((noinline)) void InitRenderInfoPens(Object *obj, struct MUI_RenderInfo *mri,
    struct MUI_WindowData *data)
{
    static const UWORD fallback[MPEN_COUNT] = {
        SHINEPEN, BACKGROUNDPEN, BACKGROUNDPEN, BACKGROUNDPEN,
        SHADOWPEN, TEXTPEN, FILLPEN, HIGHLIGHTTEXTPEN
    };
    ULONG rgbtable[3 * 3];
    int i;

    /* Also supply valid borrowed pens if a requested allocation fails. */
    for (i = 0; i < MPEN_COUNT; i++)
    {
        data->wd_PenHandles[i] = -1;
        mri->mri_PensStorage[i] = mri->mri_DrawInfo->dri_Pens[fallback[i]];
    }
    mri->mri_Pens = mri->mri_PensStorage;
    /* shine, background, shadow: the fallback's first, second, fifth */
    for (i = 0; i < 9; i += 3)
        GetRGB32(mri->mri_Colormap,
            mri->mri_DrawInfo->dri_Pens[fallback[i == 6 ? 4 : i / 3]], 1,
            rgbtable + i);

    for (i = 0; i < MPEN_COUNT; i++)
    {
        struct MUI_PenSpec derived;
        struct MUI_PenSpec *spec = &MUIMB(MUIMasterBase)->defaultPens[i];
        LONG pen;

        if (!spec->buf[0] &&
            (i == MPEN_HALFSHINE || i == MPEN_HALFSHADOW))
        {
            ULONG gun[3];
            int c;

            for (c = 0; c < 3; c++)
                gun[c] = i == MPEN_HALFSHINE
                    ? DoHalfshineGun(rgbtable[c], rgbtable[3 + c])
                    : DoHalfshadowGun(rgbtable[6 + c], rgbtable[3 + c]);
            /* ObtainPen parses the spec immediately; no heap copy is needed. */
            snprintf(derived.buf, sizeof(derived.buf), "r%08x,%08x,%08x",
                (unsigned int)gun[0], (unsigned int)gun[1],
                (unsigned int)gun[2]);
            spec = &derived;
        }
        pen = MUI_ObtainPen(mri, spec, 0);
        data->wd_PenHandles[i] = pen;
        if (pen != -1)
            mri->mri_PensStorage[i] = MUIPEN(pen);
    }
    /* the active object's frame, after the MUI pens it may name ("m0",
     * shine, by default: no pen of its own then) */
    mri->mri_Tail->ActivePen = MUI_ObtainPen(mri, (struct MUI_PenSpec *)
        muiGlobalInfo(obj)->mgi_Prefs->active_object_color, 0);
}

/* Unlock the render info's public screen if it was locked, forget the
 * screen */
static __attribute__((noinline)) void ReleaseRenderScreen(
    struct MUI_WindowData *data, struct MUI_RenderInfo *mri)
{
    if (data->wd_Flags & MUIWF_SCREENLOCKED)
    {
        UnlockPubScreen(NULL, mri->mri_Screen);
        data->wd_Flags &= ~MUIWF_SCREENLOCKED;
    }
    mri->mri_Screen = NULL;
}

/* Select a supplied, application-custom, requested-public, preference-
 * public or default public screen; acquire DrawInfo, custom-frame cache
 * references, pens and optional scrollbar images. Borrow obj/data/mri and
 * require a connected application with preferences. Return FALSE on checked
 * screen/DrawInfo failure, TRUE otherwise. Public locks are owned until
 * cleanup; optional allocations remain incompletely checked.
 */
static __attribute__((noinline)) BOOL SetupRenderInfo(Object *obj, struct MUI_WindowData *data,
    struct MUI_RenderInfo *mri)
{
    /* the system images by the border scrollers that need them,
     * mri_LeftImage ... mri_SizeImage in this order (window.h) */
    static const UBYTE which[5] = {
        LEFTIMAGE, RIGHTIMAGE, UPIMAGE, DOWNIMAGE, SIZEIMAGE };
    static const UBYTE need[5] = {
        MUIWF_USEBOTTOMSCROLLER >> 8, MUIWF_USEBOTTOMSCROLLER >> 8,
        MUIWF_USERIGHTSCROLLER >> 8, MUIWF_USERIGHTSCROLLER >> 8,
        (MUIWF_USEBOTTOMSCROLLER | MUIWF_USERIGHTSCROLLER) >> 8 };
    struct MUI_GlobalInfo *mgi = muiGlobalInfo(obj);
    struct ZunePrefsNew *prefs = mgi->mgi_Prefs;
    struct Screen *scr;
    Object **img, *temp_obj;
    ULONG deep;
    int i;

    if (!mri->mri_Tail &&
        !(mri->mri_Tail = AllocVec(sizeof(*mri->mri_Tail), MEMF_ANY | MEMF_CLEAR)))
        return FALSE;

    /* The screen: the program's, else the settings' own screen mode
     * (opened once for the application), else the program's public
     * screen, else the settings' (opened through muiscreen.library if
     * it is not there), else the default public screen. A locked public
     * screen is unlocked at cleanup (MUIWF_SCREENLOCKED). */
    if (!data->wd_UserScreen && prefs->screenmodeid != ~0)
    {
        if (!mgi->mgi_CustomScreen)
            /* It's fine if this fails as there is a fallback case below */
            mgi->mgi_CustomScreen = OpenScreenTags(NULL,
                SA_DisplayID, prefs->screenmodeid, SA_SharePens, TRUE,
                SA_FullPalette, TRUE, SA_LikeWorkbench, TRUE, TAG_DONE);
        data->wd_UserScreen = mgi->mgi_CustomScreen;
    }
    if (!(scr = data->wd_UserScreen))
    {
        CONST_STRPTR name = prefs->publicscreen_name;

        if (data->wd_UserPublicScreen)
            scr = LockPubScreen(data->wd_UserPublicScreen);
        else if (name && name[0])
        {
            if (!(scr = LockPubScreen(name)) && mgi->mgi_Configdata
                && MUIScreenBase)
            {
                struct MUI_PubScreenDesc *desc = (struct MUI_PubScreenDesc *)
                    DoMethod(mgi->mgi_Configdata,
                        MUIM_Configdata_GetPubScrnDesc, name);
                if (desc)
                {
                    MUIS_OpenPubScreen(desc);
                    scr = LockPubScreen(name);
                }
            }
        }
        if (!scr && !(scr = LockPubScreen(NULL)))
            return FALSE;
        data->wd_Flags |= MUIWF_SCREENLOCKED;
    }
    mri->mri_Screen = scr;

    if (!(mri->mri_DrawInfo = GetScreenDrawInfo(scr)))
    {
        ReleaseRenderScreen(data, mri);
        return FALSE;
    }

    deep = GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH) >= 15;
#ifdef ZUNE68_GCC_NATIVE
    /* Planar windows never load or allocate RTG support. */
    if (deep)
        ZuneOpenCyberGfx();
#endif
    InitCustomFrames(obj, mri);

    mri->mri_Colormap = scr->ViewPort.ColorMap;
    /* MUI 3.8 opens every later window of an application in the stage
     * one of its windows had to be fitted into the screen in (measured:
     * MUI_Request's window after a 17-button column needed helvetica) */
    mri->mri_Fallback = MGI_FALLBACK(mgi);

    /* Decided again at every setup: the window may open on another
     * screen. MUI 3.8's frame thickness "auto" is thick when the screen
     * is at least twice as wide as tall (its size, not the display mode:
     * 640x320 thick, 640x321 thin, lores 320x256 thin). */
    mri->mri_Flags &= ~(MUIMRI_THINFRAMES | MUIMRI_TRUECOLOR);
    if (prefs->frame_thickness == FRAME_THICKNESS_THIN
        || (prefs->frame_thickness != FRAME_THICKNESS_THICK
            && scr->Width < 2 * scr->Height))
        mri->mri_Flags |= MUIMRI_THINFRAMES;
    if (deep)
        mri->mri_Flags |= MUIMRI_TRUECOLOR;

    InitRenderInfoPens(obj, mri, data);

    for (i = 0; i < -MUIV_Font_NegCount; i++)
        mri->mri_Fonts[i] = NULL;

    img = &mri->mri_LeftImage;
    for (i = 0; i < 5; i++)
        img[i] = ((data->wd_Flags >> 8) & need[i])
            ? CreateSysimage(mri->mri_DrawInfo, which[i]) : NULL;

    /* In fact borderless windows could also have borders (e.g. if they
     * have a window title) but since they look ugly anyway we ignore it
     * for now */
    mri->mri_BorderLeft = mri->mri_BorderRight = mri->mri_BorderTop =
        mri->mri_BorderBottom = 0;
    if (!(data->wd_CrtFlags & WFLG_BORDERLESS))
    {
        mri->mri_BorderLeft = scr->WBorLeft;
        mri->mri_BorderRight = scr->WBorRight;
        mri->mri_BorderTop = scr->WBorTop + scr->Font->ta_YSize + 1;
        mri->mri_BorderBottom = scr->WBorBottom;
        if ((temp_obj = CreateSysimage(mri->mri_DrawInfo, SIZEIMAGE)))
        {
            IPTR val;

            GetAttr(IA_Height, temp_obj, &val);
            DisposeObject(temp_obj);
            mri->mri_BorderBottom = val;
        }
    }

    return TRUE;
}

static void CleanupRenderInfoPens(struct MUI_RenderInfo *mri,
    struct MUI_WindowData *data)
{
    int i;

    for (i = 0; i < MPEN_COUNT; i++)
    {
        LONG pen = data->wd_PenHandles[i];
        data->wd_PenHandles[i] = -1;
        if (pen != -1)
            MUI_ReleasePen(mri, pen);
    }
    if (mri->mri_Tail->ActivePen != -1)
        MUI_ReleasePen(mri, mri->mri_Tail->ActivePen);
    mri->mri_Tail->ActivePen = -1;
}

static void CloseRenderInfoFonts(struct MUI_RenderInfo *mri)
{
    int i;

    for (i = 0; i < -MUIV_Font_NegCount; i++)
    {
        if (mri->mri_Fonts[i])
        {
            CloseFont(mri->mri_Fonts[i]);
            mri->mri_Fonts[i] = NULL;
        }
    }
}

/* Dispose acquired frame references and system images, close cached fonts,
 * release pens and DrawInfo, then release the public-screen lock or
 * conditionally close the application custom screen. Requires no remaining
 * users of these resources. Custom-screen closure tests an empty
 * application window list; CloseScreen failure is ignored. Clear the render
 * screen and return no status.
 */
static __attribute__((noinline)) void CleanupRenderInfo(Object *obj, struct MUI_WindowData *data,
    struct MUI_RenderInfo *mri)
{

    DisposeCustomFrames(mri);

    /* the five system images, mri_LeftImage ... mri_SizeImage in this
     * order in the render info (window.h) */
    {
        Object **img = &mri->mri_LeftImage;
        int i;

        for (i = 0; i < 5; i++, img++)
        {
            if (*img)
            {
                DisposeObject(*img);
                *img = NULL;
            }
        }
    }

    CloseRenderInfoFonts(mri);
    CleanupRenderInfoPens(mri, data);
    FreeScreenDrawInfo(mri->mri_Screen, mri->mri_DrawInfo);
    mri->mri_DrawInfo = NULL;

    /* Close the application custom screen only when the application window
     * list is empty. This tests membership, not the number of open windows. */
    if (muiGlobalInfo(obj)->mgi_CustomScreen)
    {
        BOOL screenclose = TRUE;
        Object *_app = _app(obj);
        if (_app != NULL)
        {
            struct List *store = NULL;
            get(_app, MUIA_Application_WindowList, &store);
            if (store != NULL)
            {
                if (!IsListEmpty(store))
                    screenclose = FALSE;
            }
        }
        if (screenclose)
        {
            /* If the window's user screen really was the custom screen,
             * clear the reference */
            if (data->wd_UserScreen == muiGlobalInfo(obj)->mgi_CustomScreen)
                data->wd_UserScreen = NULL;

            CloseScreen(muiGlobalInfo(obj)->mgi_CustomScreen);
            muiGlobalInfo(obj)->mgi_CustomScreen = NULL;
        }
    }

    ReleaseRenderScreen(data, mri);
}

/* Select the buffer RastPort when a buffer bitmap exists, otherwise the
 * live native window RastPort. Requires a native window and prepared render
 * info. Store a borrowed drawing-port pointer valid until hide; return no
 * status.
 */
static void ShowRenderInfo(struct MUI_RenderInfo *mri)
{
    if (mri->mri_BufferBM)
    {
        mri->mri_RastPort = &mri->mri_Tail->BufferRP;
    }
    else
    {
        mri->mri_RastPort = mri->mri_Window->RPort;
    }
}

/* Clear the borrowed drawing-port pointer after drawing has stopped. Does
 * not free the native RastPort or buffer and returns no status.
 */
static void HideRenderInfo(struct MUI_RenderInfo *mri)
{
    mri->mri_RastPort = NULL;
}

/* Return the fixed IDCMP mask for size/change/refresh, mouse buttons/move,
 * menu pick, close, raw keys, ticks, activation and gadget-up events. No
 * allocation or state change occurs.
 */
static ULONG GetDefaultEvents(void)
{
    return IDCMP_NEWSIZE | IDCMP_CHANGEWINDOW | IDCMP_REFRESHWINDOW
        | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_MENUPICK | IDCMP_MENUVERIFY
        | IDCMP_CLOSEWINDOW | IDCMP_RAWKEY | IDCMP_INTUITICKS
        | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_GADGETUP;
}

/* IDCMP_MENUVERIFY is there for MUIM_Menustrip_WillOpen (MUI 4/5). Each
 * menu opening then waits until the program's task answers it; MUI 3.8's
 * windows do not ask for it (their IDCMPFlags, opbench), so a MUI 3
 * program's windows do not either, unless an event handler asks (opbench
 * "menu open+close", cycle-exact FS-UAE: 20.0 -> ... ms, MUI 3.8 13.2) */
static ULONG Window_EventsMask(Object *obj)
{
    return (obj && muiGlobalInfo(obj) && ZUNE_APP_MUI38(obj))
        ? ~(ULONG)IDCMP_MENUVERIFY : ~(ULONG)0;
}

/* Combine new_events with every registered handler mask, then remove
 * IDCMP_VANILLAKEY. Update stored events and call ModifyIDCMP if the mask
 * changed while open. Requires stable valid handler links; return no status
 * and retain no new storage.
 */
static void ChangeEvents(struct MUI_WindowData *data, ULONG new_events)
{
    struct MinNode *mn;
    struct MUI_EventHandlerNode *ehn;
    ULONG old_events = data->wd_Events;

    new_events &= Window_EventsMask(data->wd_RenderInfo.mri_WindowObject);
    for (mn = data->wd_EHList.mlh_Head; mn->mln_Succ; mn = mn->mln_Succ)
    {
        ehn = (struct MUI_EventHandlerNode *)mn;
        new_events |= ehn->ehn_Events;
    }

    /* sba: kill the IDCMP_VANILLAKEY flag. MUI doesn't do this but programs
     ** which use this will behave different if they request for this flag
     ** (also on MUI)
     */
    new_events &= ~IDCMP_VANILLAKEY;

    data->wd_Events = new_events;
    if ((old_events != new_events) && (data->wd_Flags & MUIWF_OPENED))
    {
        ModifyIDCMP(data->wd_RenderInfo.mri_Window, new_events);
    }
}

static __attribute__((noinline)) void CalcWindowPosition(Object *obj, struct MUI_WindowData *data,
    ULONG flags);
static void CreateWindowScrollbars(Object *obj,
    struct MUI_WindowData *data);
/* Modify the supplied writable alternate IBox for the next opening using
 * screen size and content limits. NoChange and Moused coordinates both
 * become unchanged; Centered uses normal inner size. Both MinMax
 * calculations omit division by 100. Scaled and Delta have no special
 * branches. Clamp dimensions and return no status.
 */
static void CalcAltDimensions(Object *obj, struct MUI_WindowData *data,
    struct IBox *altdims);
static void RemoveEventHandler(struct MUI_WindowData *data,
    struct MUI_EventHandlerNode *handler);

void ZuneWatchWindow(Object *window, struct ZuneWindowGuard *guard)
{
    struct MUI_WindowData *data = muiWindowData(window);
    guard->closed = FALSE;
    guard->next = data->wd_Guards;
    data->wd_Guards = guard;
}

void ZuneUnwatchWindow(Object *window, struct ZuneWindowGuard *guard)
{
    struct MUI_WindowData *data = muiWindowData(window);
    struct ZuneWindowGuard **link = &data->wd_Guards;
    while (*link && *link != guard) link = &(*link)->next;
    if (*link) *link = guard->next;
}

/* Release verification messages before Hide/Cleanup can touch layers. */
static void CancelWindowGuards(struct MUI_WindowData *data)
{
    while (data->wd_Guards)
    {
        struct ZuneWindowGuard *guard = data->wd_Guards;
        data->wd_Guards = guard->next;
        if (guard->handler) RemoveEventHandler(data, guard->handler);
        if (guard->menu_message)
        {
            guard->menu_message->Code = MENUCANCEL;
            ReplyMsg((struct Message *)guard->menu_message);
            guard->menu_message = NULL;
        }
        guard->closed = TRUE;
    }

}

/* Optionally snapshot, clear exposed native pointers and active state,
 * detach/free native menus, remove and reply queued messages belonging to
 * this window under Forbid, detach the shared port and close Intuition.
 * Dispose scrollbar gadgets and release their IDs. Requires the application
 * and setup resources to remain live. Return no status; the AppWindow
 * registration is not removed and Sleep count is not reset.
 */
static void UndisplayWindow(Object *obj, struct MUI_WindowData *data);
/* The index of obj in the cycle chain, -1 if it is not there (NULL never
 * is) */
static LONG ChainIndex(struct MUI_WindowData *data, Object *obj);

#ifdef ZUNE68_GCC_NATIVE
/* The extra title bar gadgets as MUI 3.8 has them: Popup, Prefs, Snapshot
 * and Iconify, left to right, left of the zoom and depth gadgets. They are
 * off by default; the gadgets and the window menu are the title gadget
 * module (wintitle.c, modules.h), loaded when a window first gets one.
 * The library keeps their gadget class and frees it at expunge. */
static Class *TitleGadgetClass;

/* At library expunge: no window is left, so no gadget either. */
void zune_title_gadgets_free(void)
{
    if (TitleGadgetClass && FreeClass(TitleGadgetClass))
        TitleGadgetClass = NULL;
}

/* For the gadgets, which run in input.device's task */
BOOL zune_window_sleeping(Object *win)
{
    return muiWindowData(win)->wd_SleepCount != 0;
}

/* The active object as stored, unchecked (area.c's cleanup) */
Object *ZuneWindowActiveObject(Object *win)
{
    return win ? muiWindowData(win)->wd_ActiveObject : NULL;
}

static void CreateNativeTitleGadgets(Object *obj, struct MUI_WindowData *data,
    struct Window *win)
{
    ULONG buttons = muiGlobalInfo(obj)->mgi_Prefs->window_buttons;
    struct Gadget *g;
    LONG right = 0, height = win->BorderTop;
    BOOL depth = FALSE;

    /* Snapshot needs a window ID; it is left out, not disabled */
    if (!data->wd_ID)
        buttons &= ~MUIV_Window_Button_Snapshot;
    buttons &= MUIV_Window_Button_Popup | MUIV_Window_Button_MUI |
        MUIV_Window_Button_Snapshot | MUIV_Window_Button_Iconify;
    if (!buttons || (data->wd_Flags & MUIWF_TOOLBOX))
        return;

    /* Left of the zoom and depth gadgets; none without a depth gadget */
    for (g = win->FirstGadget; g; g = g->NextGadget)
    {
        UWORD type = g->GadgetType & GTYP_SYSTYPEMASK;
        if (!(g->GadgetType & GTYP_SYSGADGET) ||
            !(g->Flags & GFLG_RELRIGHT) || g->TopEdge >= height ||
            type == GTYP_SIZING)
            continue;
        if (type == GTYP_WDEPTH)
            depth = TRUE;
        if (-g->LeftEdge > right)
            right = -g->LeftEdge;
    }
    if (!depth)
        return;
    zune_wintitle_create(win, buttons, right, data->wd_TitleGadgets,
        &TitleGadgetClass);
}

static void FreeNativeTitleGadgets(struct MUI_WindowData *data)
{
    int i;
    /* the window is closed, so the gadgets are no longer attached */
    for (i = 0; i < 4; i++)
    {
        if (data->wd_TitleGadgets[i])
            DisposeObject(data->wd_TitleGadgets[i]);
        data->wd_TitleGadgets[i] = NULL;
    }
}

static int TitleGadgetIndex(struct MUI_WindowData *data, APTR gadget)
{
    int i;
    for (i = 0; i < 4; i++)
        if (gadget && gadget == data->wd_TitleGadgets[i])
            return i;
    return -1;
}

/* A click on a title gadget, after its message has been replied */
static __attribute__((noinline)) void TitleGadgetUp(Object *win, struct MUI_WindowData *data,
    int which, ULONG seconds, ULONG micros)
{
    Object *app = _app(win);
    int action = ZUNE_TA_ICONIFY;

    if (which == TG_POPUP)
        action = zune_wintitle_menu(data->wd_RenderInfo.mri_Window,
            data->wd_RenderInfo.mri_DrawInfo, data->wd_TitleGadgets[TG_POPUP],
            data->wd_ID != 0);
    else if (which == TG_PREFS)
        action = ZUNE_TA_SETTINGS;
    else if (which == TG_SNAPSHOT)
    {
        /* a double-click forgets: the first click snapshots */
        BOOL undo = data->wd_SnapshotClick && DoubleClick(
            data->wd_SnapshotSeconds, data->wd_SnapshotMicros,
            seconds, micros);
        data->wd_SnapshotClick = !undo;
        data->wd_SnapshotSeconds = seconds;
        data->wd_SnapshotMicros = micros;
        action = undo ? ZUNE_TA_UNSNAPSHOT : ZUNE_TA_SNAPSHOT;
    }
    switch (action)
    {
    case ZUNE_TA_ICONIFY: set(app, MUIA_Application_Iconified, TRUE); break;
    case ZUNE_TA_SNAPSHOT: DoMethod(win, MUIM_Window_Snapshot, 1); break;
    case ZUNE_TA_UNSNAPSHOT: DoMethod(win, MUIM_Window_Snapshot, 0); break;
    case ZUNE_TA_SETTINGS: DoMethod(app, MUIM_Application_OpenConfigWindow, 0); break;
    case ZUNE_TA_ABOUT: DoMethod(app, MUIM_Application_AboutMUI, (IPTR)win); break;
    }
}
#endif

/* The window's limits from its contents' and its borders */
static __attribute__((noinline)) void ContentLimits(
    struct MUI_WindowData *data, struct Window *win)
{
    int hborders = win->BorderLeft + win->BorderRight;
    int vborders = win->BorderTop + win->BorderBottom;

    WindowLimits(win,
        data->wd_MinMax.MinWidth + hborders,
        data->wd_MinMax.MinHeight + vborders,
        data->wd_MinMax.MaxWidth + hborders,
        data->wd_MinMax.MaxHeight + vborders);
}

/* The native menus of a menu strip object, laid out (NULL without) */
static __attribute__((noinline)) struct Menu *MakeMenus(APTR visinfo,
    Object *strip)
{
    struct NewMenu *newmenu = NULL;
    struct Menu *menu;
    struct TagItem tags[] = {
        {GTMN_NewLookMenus, TRUE},
        {TAG_DONE, 0}
    };

    get(strip, MUIA_Menuitem_NewMenu, &newmenu);
    if (!newmenu || !(menu = CreateMenusA(newmenu, NULL)))
        return NULL;
    LayoutMenusA(menu, visinfo, tags);
    return menu;
}

/* For a connected, successfully set-up window with computed content limits,
 * build native menus and scrollbars, calculate normal/alternate geometry
 * and apply a saved application position record. Open Intuition, install
 * the shared application port and borrowed object UserData, set limits, and
 * optionally register an AppWindow. Return TRUE on native opening, FALSE
 * after local native-resource cleanup otherwise. Menu, image, gadget and
 * AppWindow failures are not all reported.
 */
static __attribute__((noinline)) BOOL DisplayWindow(Object *obj, struct MUI_WindowData *data)
{
    struct Window *win;
    ULONG flags = data->wd_CrtFlags;
    struct IBox altdims;
    ULONG backfill;
#ifdef __AROS__
    ULONG buttons;
#endif

    struct Menu *menu = NULL;
    APTR visinfo = NULL;

    Object *gadgets;

    if (!(data->wd_Flags & MUIWF_DONTACTIVATE))
    {
        flags |= WFLG_ACTIVATE;
    }

    /* Toolboxes are handled differently on AmigaOS */
#ifdef __AROS__
    if (data->wd_Flags & MUIWF_TOOLBOX)
        flags |= WFLG_TOOLBOX;
#endif

    if (data->wd_MinMax.MinHeight == data->wd_MinMax.MaxHeight
        && data->wd_MinMax.MinWidth == data->wd_MinMax.MaxWidth)
        flags &= ~WFLG_SIZEGADGET;

    if ((flags & WFLG_SIZEGADGET) && !(flags & WFLG_SIZEBRIGHT))
        flags |= WFLG_SIZEBBOTTOM;

    CalcWindowPosition(obj, data, flags);

    struct Screen *scr = data->wd_RenderInfo.mri_Screen;
    if ((visinfo = GetVisualInfoA(scr, NULL)))
    {
        if (data->wd_Menustrip)
            menu = MakeMenus(visinfo, data->wd_Menustrip);
        FreeVisualInfo(visinfo);
    }

    CreateWindowScrollbars(obj, data);
    altdims = data->wd_AltDim;
    CalcAltDimensions(obj, data, &altdims);

    /* hack to account for border size, as we only know the innersize and
     * must give the total size.
     */
    altdims.Width +=
        scr->WBorLeft +
        scr->WBorRight;
    altdims.Height +=
        scr->WBorTop +
        scr->WBorBottom +
        data->wd_RenderInfo.mri_DrawInfo->dri_Font->tf_YSize + 1;

    if (muiGlobalInfo(obj)->mgi_Prefs->window_redraw ==
        WINDOW_REDRAW_WITHOUT_CLEAR)
        backfill = WA_BackFill;
    else
        backfill = TAG_IGNORE;

    if (muiGlobalInfo(obj)->mgi_Prefs->window_refresh ==
        WINDOW_REFRESH_SMART)
        flags &= ~WFLG_SIMPLE_REFRESH;
    set(_app(obj), MUIA_Application_SearchWinId, data->wd_ID);
    struct windowpos *winp = 0;
    get(_app(obj), MUIA_Application_GetWinPos, &winp);
    if (winp)
    {
        /* Within the window's own limits: another program with the
         * same application base and window ID (TheBar's demos all use
         * THEBAREXAMPLE/MAIN) leaves its window's size here, and a
         * window opened larger than its maximum stayed empty */
        if (scr->Width >
            (data->wd_X + data->wd_Width))
        {
            data->wd_X = winp->x1;
            data->wd_Width = CLAMP(winp->w1, data->wd_MinMax.MinWidth,
                data->wd_MinMax.MaxWidth);
        }
        if (scr->Height >
            (data->wd_Y + data->wd_Height))
        {
            data->wd_Y = winp->y1;
            data->wd_Height = CLAMP(winp->h1, data->wd_MinMax.MinHeight,
                data->wd_MinMax.MaxHeight);
        }
    }

    gadgets =
        (data->wd_VertProp !=
        NULL) ? data->wd_VertProp : data->wd_HorizProp;
#ifdef __AROS__
    buttons = muiGlobalInfo(obj)->mgi_Prefs->window_buttons;
#endif

    win = OpenWindowTags
        (NULL,
        WA_Left, (IPTR) data->wd_X,
        WA_Top, (IPTR) data->wd_Y,
        WA_Flags, (IPTR) flags,
        data->wd_Title ?
        WA_Title :
        TAG_IGNORE, (IPTR) data->wd_Title,
        data->wd_ScreenTitle ?
        WA_ScreenTitle :
        TAG_IGNORE, (IPTR) data->wd_ScreenTitle,
        /* Like MUI 3.9, a window on a locked public screen is one of its
         * visitors (tests/native/notitle.c) */
        (data->wd_Flags & MUIWF_SCREENLOCKED) ? WA_PubScreen :
        WA_CustomScreen, (IPTR) scr,
        WA_InnerWidth, (IPTR) data->wd_Width,
        WA_InnerHeight, (IPTR) data->wd_Height,
        WA_AutoAdjust, (IPTR) TRUE, WA_NewLookMenus, (IPTR) TRUE,
/* AmigaOS v4 extension */
#ifdef WA_ToolBox
        WA_ToolBox, (IPTR) ! !(data->wd_Flags & MUIWF_TOOLBOX),
#endif
/* MorphOS extensions */
#ifdef WA_ExtraGadget_MUI
        WA_ExtraGadget_MUI,
        (IPTR) ((buttons & MUIV_Window_Button_MUI) != 0) ? TRUE : FALSE,
        WA_ExtraGadget_PopUp,
        (IPTR) ((buttons & MUIV_Window_Button_Popup) != 0) ? TRUE : FALSE,
        WA_ExtraGadget_Snapshot,
        (IPTR) ((buttons & MUIV_Window_Button_Snapshot) !=
            0) ? TRUE : FALSE, WA_ExtraGadget_Iconify,
        (IPTR) ((buttons & MUIV_Window_Button_Iconify) != 0) ? TRUE : FALSE,
#endif
        /* MUI 3.9 also traps the right button without a menu strip */
        data->wd_NoMenus || !menu ?
        WA_RMBTrap :
        TAG_IGNORE, (IPTR) TRUE,
        WA_Gadgets, (IPTR) gadgets,
        /* MUI 3.9 adds no zoom gadget to a window without a size gadget
         * (tests/native/titlegadgets.c) */
        data->wd_ZoomGadget && (flags & WFLG_SIZEGADGET) ?
        WA_Zoom :
        TAG_IGNORE, (IPTR) & altdims,
        backfill, (IPTR) LAYERS_NOBACKFILL, TAG_DONE);

    if (win)
    {

        /* recalc window size (which will hopefully equal our requested
         * size) */
        data->wd_Width = win->GZZWidth;
        data->wd_Height = win->GZZHeight;

#ifdef ZUNE68_GCC_NATIVE
        CreateNativeTitleGadgets(obj, data, win);
#endif
        /* set window limits according to window contents */
        ContentLimits(data, win);

        win->UserData = (BYTE *) data->wd_RenderInfo.mri_WindowObject;
        win->UserPort = muiGlobalInfo(obj)->mgi_WindowsPort;
            /* Same port for all windows */
        ChangeEvents(data, GetDefaultEvents());
        ModifyIDCMP(win, data->wd_Events);

        data->wd_RenderInfo.mri_Window = win;
        data->wd_RenderInfo.mri_VertProp = data->wd_VertProp;
        data->wd_RenderInfo.mri_HorizProp = data->wd_HorizProp;
        SetDrMd(win->RPort, JAM1);
            //text is drawn wrong in toolbarclass if not set

        if (menu)
        {
            data->wd_Menu = menu;
            SetMenuStrip(win, menu);
        }

        if (flags & WFLG_ACTIVATE)
        {
            data->wd_Flags |= MUIWF_ACTIVE;
        }

        if ((data->wd_Flags & MUIWF_ISAPPWINDOW) && ZUNE_WB())
        {
            data->wd_AppWindow = AddAppWindowA(0, (IPTR) obj, win,
                muiGlobalInfo(obj)->mgi_AppPort, NULL);
        }

        return TRUE;
    }

    if (menu)
        FreeMenus(menu);
    UndisplayWindow(obj, data);

    return FALSE;
}


/* Optionally snapshot, clear exposed native pointers and active state,
 * detach/free native menus, remove and reply queued messages belonging to
 * this window under Forbid, detach the shared port and close Intuition.
 * Dispose scrollbar gadgets and release their IDs. Requires the application
 * and setup resources to remain live. Return no status; the AppWindow
 * registration is not removed and Sleep count is not reset.
 */
static void UndisplayWindow(Object *obj, struct MUI_WindowData *data)
{
    struct Window *win = data->wd_RenderInfo.mri_Window;
    BOOL prefssnap =
        data->wd_ID != 0 &&
        ((muiGlobalInfo(obj)->mgi_Prefs->window_position ==
            WINDOW_POSITION_REMEMBER_ON_EXIT)
        || (muiGlobalInfo(obj)->mgi_Prefs->window_position ==
            WINDOW_POSITION_SAVE_ON_EXIT));

    if (prefssnap)
    {
        ZuneRememberWindowPosition(obj);
    }


    data->wd_RenderInfo.mri_Window = NULL;
    data->wd_RenderInfo.mri_VertProp = NULL;
    data->wd_RenderInfo.mri_HorizProp = NULL;

    data->wd_Flags &= ~MUIWF_ACTIVE;

    if (win != NULL)
    {
        ClearMenuStrip(win);
        if (data->wd_Menu)
        {
            FreeMenus(data->wd_Menu);
            data->wd_Menu = NULL;
        }

        if (win->UserPort)
        {
            struct IntuiMessage *msg, *succ;

            /* remove all messages pending for this window */
            Forbid();
            for (msg =
                (struct IntuiMessage *)win->UserPort->mp_MsgList.lh_Head;
                (succ =
                    (struct IntuiMessage *)msg->ExecMessage.mn_Node.
                    ln_Succ); msg = succ)
            {
                if (msg->IDCMPWindow == win)
                {
                    Remove((struct Node *)msg);
                    ReplyMsg((struct Message *)msg);
                }
            }
            win->UserPort = NULL;
            ModifyIDCMP(win, 0);
            Permit();
        }

        /*  D(bug("before CloseWindow\n")); */
        CloseWindow(win);
        /*  D(bug("after CloseWindow\n")); */
    }

#ifdef ZUNE68_GCC_NATIVE
    FreeNativeTitleGadgets(data);
#endif

    /* the six border scroller gadgets, wd_VertProp ... wd_RightButton in
     * this order in the instance data */
    {
        Object **gad = &data->wd_VertProp;
        int i;

        for (i = 0; i < 6; i++, gad++)
        {
            if (*gad)
            {
                DoMethod(obj, MUIM_Window_FreeGadgetID,
                    ((struct Gadget *)*gad)->GadgetID);
                DisposeObject(*gad);
                *gad = NULL;
            }
        }
    }
}


/* Refresh an open, set-up window with a live root. Finish pending resize
 * layout/show and redraw according to preferences, or redraw inside
 * MUI_BeginRefresh/MUI_EndRefresh. Reestablish active-object state. Borrow
 * all inputs and return no status; this helper does not validate a closed
 * window.
 */
/* The window's background over all of its inside (between the borders),
 * as the window's own coordinates: a refresh's clip limits it to the
 * damage */
static __attribute__((noinline)) void WindowBackground(
    struct MUI_WindowData *data)
{
    struct Window *win = data->wd_RenderInfo.mri_Window;
    LONG left = win->BorderLeft, top = win->BorderTop;

    zune_imspec_draw(data->wd_Background, &data->wd_RenderInfo, left, top,
        win->Width - win->BorderRight - left,
        win->Height - win->BorderBottom - top, left, top, 0);
}

static VOID RefreshWindow(Object *oWin, struct MUI_WindowData *data)
{
    struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
    ULONG flags = MADF_DRAWALL;

    if (data->wd_Flags & MUIWF_RESIZING)
    {
        if (MUI_BeginRefresh(mri, 0))
            MUI_EndRefresh(mri, 0);
        RefreshWindowFrame(mri->mri_Window);

        data->wd_Flags &= ~MUIWF_RESIZING;
        _width(data->wd_RootObject) = data->wd_Width;
        _height(data->wd_RootObject) = data->wd_Height;
        DoLayoutMethod(data->wd_RootObject);
        DoShowMethod(data->wd_RootObject);

        if (data->wd_Flags & MUIWF_ERASEAREA)
            WindowBackground(data);
        else if (muiGlobalInfo(oWin)->mgi_Prefs->window_redraw
            != WINDOW_REDRAW_WITH_CLEAR)
            flags = MADF_DRAWOBJECT | MADF_DRAWFRAME;
        MUI_Redraw(data->wd_RootObject, flags);
        // but should only draw focus without using MUIM_GoActive !
        ActivateObject(data);
    }
    else if (MUI_BeginRefresh(mri, 0))
    {
        /* MUI 3.9's layer backfill hook paints damage with the window
         * background before objects redraw; IBrowse's toolbar leaves
         * the room between its buttons to it. The refresh clip limits
         * this to the damaged area. */
        WindowBackground(data);
        MUI_Redraw(data->wd_RootObject, MADF_DRAWALL);
        // but should only draw focus without using MUIM_GoActive !
        ActivateObject(data);
        MUI_EndRefresh(mri, 0);
    }
}


/* Initialize data->wd_X and data->wd_Y for DisplayWindow. The outer size
 * uses the borders the window will really get: mri_BorderBottom assumes a
 * size gadget, which fixed-size windows (IBrowse's requesters) do not
 * have, and MUI 3.9 centres those on their actual height. */
static __attribute__((noinline)) void CalcWindowPosition(Object *obj, struct MUI_WindowData *data,
    ULONG flags)
{
    struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
    struct Screen *scr = mri->mri_Screen;
    BOOL sizing = (flags & WFLG_SIZEGADGET) && !(flags & WFLG_BORDERLESS);
    WORD bottom = sizing && (flags & WFLG_SIZEBBOTTOM) ? mri->mri_BorderBottom
        : (flags & WFLG_BORDERLESS) ? 0 : scr->WBorBottom;
    WORD right = mri->mri_BorderRight, top = mri->mri_BorderTop;
    WORD width, height;

    /* without a title and title bar gadgets Intuition gives the window
     * just the plain top border (YAM's splash window) */
    if (!(flags & WFLG_BORDERLESS) && !data->wd_Title
        && !(flags & (WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET)))
        top = scr->WBorTop;

    /* Intuition widens the borders to fit border scrollers, so MUI 3.9
     * centres IBrowse's Preferences window on its real width */
    if ((data->wd_Flags & MUIWF_USERIGHTSCROLLER) && mri->mri_UpImage
        && IM(mri->mri_UpImage)->Width > right)
        right = IM(mri->mri_UpImage)->Width;
    if ((data->wd_Flags & MUIWF_USEBOTTOMSCROLLER) && mri->mri_LeftImage
        && IM(mri->mri_LeftImage)->Height > bottom)
        bottom = IM(mri->mri_LeftImage)->Height;
    width = mri->mri_BorderLeft + data->wd_Width + right;
    height = top + data->wd_Height + bottom;
    /* the reference window's outer box: left, top, width, height
     * (unsigned, as before: a reference narrower than the window puts
     * it far left, which Intuition's WA_AutoAdjust brings back) */
    ULONG ref[4] = { 0, 0, 0, 0 };
    Object *refwin = data->wd_RefWindow;
    LONG x = data->wd_ReqX, y = data->wd_ReqY;

    if (refwin)
    {
        static const ULONG attrs[4] = { MUIA_Window_LeftEdge,
            MUIA_Window_TopEdge, MUIA_Window_Width, MUIA_Window_Height };
        struct Window *w = NULL;
        int i;

        /* MUI 3.9 centres on the reference window's outer box (YAM's
         * Configuration Warning over its Configuration window) */
        get(refwin, MUIA_Window_Window, &w);
        if (w)
        {
            ref[0] = w->LeftEdge;
            ref[1] = w->TopEdge;
            ref[2] = w->Width;
            ref[3] = w->Height;
        }
        else
            for (i = 0; i < 4; i++)
                get(refwin, attrs[i], &ref[i]);
    }

    if (x == MUIV_Window_LeftEdge_Centered)
        x = refwin ? ref[0] + (ref[2] - width) / 2
            : (scr->ViewPort.DWidth - width) / 2;
    else if (x == MUIV_Window_LeftEdge_Moused)
        x = scr->MouseX - width / 2;
    else if (refwin)
        x += ref[0];

    if (y == MUIV_Window_TopEdge_Centered)
        y = refwin ? ref[1] + (ref[3] - height) / 2
            : (scr->ViewPort.DHeight - height) / 2;
    else if (y == MUIV_Window_TopEdge_Moused)
        y = scr->MouseY - height / 2;
    else if (y <= MUIV_Window_TopEdge_Delta(0)
        && y > MUIV_Window_TopEdge_Delta(7))
        y = MUIV_Window_TopEdge_Delta(0) - y + (refwin
            ? ref[1] + mri->mri_BorderTop : scr->BarHeight + 1);
    else if (refwin)
        y += ref[1];
    data->wd_X = x;
    data->wd_Y = y;
}

/* Initialize alternative dimensions for DisplayWindow */
/* Modify the supplied writable alternate IBox for the next opening using
 * screen size and content limits. NoChange and Moused coordinates both
 * become unchanged; Centered uses normal inner size. Both MinMax
 * calculations omit division by 100. Scaled and Delta have no special
 * branches. Clamp dimensions and return no status.
 */
/* One alternate (zoomed) size: MinMax, Screen or Visible percentages (the
 * MinMax one without the division by 100, as before), within the limits */
static __attribute__((noinline)) WORD AltSize(WORD v, WORD min, WORD max,
    WORD screen)
{
    if (_between(MUIV_Window_AltWidth_MinMax(100), v,
            MUIV_Window_AltWidth_MinMax(0)))
        v = min - v * (max - min);
    else if (_between(MUIV_Window_AltWidth_Screen(100), v,
            MUIV_Window_AltWidth_Screen(0)))
        v = screen * (-(v + 200)) / 100;
    else if (_between(MUIV_Window_AltWidth_Visible(100), v,
            MUIV_Window_AltWidth_Visible(0)))
        v = screen * (-(v + 100)) / 100;
    return CLAMP(v, min, max);
}

/* One alternate position: NoChange and Moused leave it, Centered centres
 * the normal inner size */
static WORD AltPos(WORD v, WORD screen, WORD size)
{
    if (v == MUIV_Window_AltTopEdge_NoChange
        || v == MUIV_Window_AltTopEdge_Moused)
        return ~0;
    if (v == MUIV_Window_AltTopEdge_Centered)
        return (screen - size) / 2;
    return v;
}

static void CalcAltDimensions(Object *obj, struct MUI_WindowData *data,
    struct IBox *altdims)
{
    struct Screen *scr = data->wd_RenderInfo.mri_Screen;

    altdims->Top = AltPos(altdims->Top, scr->Height, data->wd_Height);
    altdims->Left = AltPos(altdims->Left, scr->Width, data->wd_Width);
    altdims->Width = AltSize(altdims->Width, data->wd_MinMax.MinWidth,
        data->wd_MinMax.MaxWidth, scr->Width);
    altdims->Height = AltSize(altdims->Height, data->wd_MinMax.MinHeight,
        data->wd_MinMax.MaxHeight, scr->Height);
}


/* Create horiz/vert window scrollbars for DisplayWindow */
/* Create requested right/bottom native prop and button gadgets using setup-
 * owned system images and allocated gadget IDs; store owned gadget pointers
 * for display/undisplay. Requires live non-NULL images. Allocation failures
 * are not propagated or fully unwound here; there is no result. Failed
 * gadget creation can leave an ID allocated until pool disposal.
 */
/* One border arrow button; its gadget ID is allocated before it is made,
 * as before (the IDs come out in the same order) */
static __attribute__((noinline)) Object *BorderButton(Object *obj,
    Object *prev, struct Image *img, LONG relright, LONG relbottom,
    ULONG border)
{
    return NewObject(NULL, "buttongclass",
        GA_Image, (IPTR) img,
        GA_RelRight, relright,
        GA_RelBottom, relbottom,
        border, TRUE,
        GA_Previous, (IPTR) prev,
        GA_ID, DoMethod(obj, MUIM_Window_AllocGadgetID),
        ICA_TARGET, ICTARGET_IDCMP, TAG_DONE);
}

/* One border prop gadget: the four position and size tags are those of
 * the scroller's direction */
static __attribute__((noinline)) Object *BorderProp(Object *obj,
    Object *prev, ULONG relpostag, LONG relpos, ULONG postag, LONG pos,
    ULONG sizetag, LONG size, ULONG relsizetag, LONG relsize,
    ULONG border, ULONG freedom)
{
    return NewObject(NULL, "propgclass",
        relpostag, relpos,
        postag, pos,
        sizetag, size,
        relsizetag, relsize,
        border, TRUE,
        GA_ID, DoMethod(obj, MUIM_Window_AllocGadgetID),
        prev ? GA_Previous : TAG_IGNORE, (IPTR) prev,
        PGA_Borderless, TRUE,
        PGA_NewLook, TRUE,
        PGA_Freedom, freedom,
        PGA_Top, 0,
        PGA_Total, 2,
        PGA_Visible, 1, ICA_TARGET, ICTARGET_IDCMP, TAG_DONE);
}

static void CreateWindowScrollbars(Object *obj,
    struct MUI_WindowData *data)
{
    struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
    Object *prevgad = NULL;
    struct Image *up = IM(mri->mri_UpImage), *down = IM(mri->mri_DownImage);
    struct Image *left = IM(mri->mri_LeftImage);
    struct Image *right = IM(mri->mri_RightImage);
    struct Image *size = IM(mri->mri_SizeImage);

    /* Create the right border scrollers now if requested */
    if (data->wd_Flags & MUIWF_USERIGHTSCROLLER)
    {
        int voffset = down->Width / 4;

        prevgad = data->wd_VertProp = BorderProp(obj, NULL,
            GA_RelRight, 1 - (up->Width - voffset),
            GA_Top, mri->mri_BorderTop + 2,
            GA_Width, up->Width - voffset * 2,
            GA_RelHeight, -(mri->mri_BorderTop + 2)
            - up->Height - down->Height - size->Height - 2,
            GA_RightBorder, FREEVERT);
        prevgad = data->wd_UpButton = BorderButton(obj, prevgad, up,
            1 - up->Width, 1 - up->Height - down->Height - size->Height,
            GA_RightBorder);
        prevgad = data->wd_DownButton = BorderButton(obj, prevgad, down,
            1 - down->Width, 1 - down->Height - size->Height,
            GA_RightBorder);
    }

    /* Create the bottom border scrollers now if requested */
    if (data->wd_Flags & MUIWF_USEBOTTOMSCROLLER)
    {
        int hoffset = right->Height / 4;

        prevgad = data->wd_HorizProp = BorderProp(obj, prevgad,
            GA_RelBottom, 1 - (left->Height - hoffset),
            GA_Left, mri->mri_BorderLeft,
            GA_Height, left->Height - hoffset * 2,
            GA_RelWidth, -(mri->mri_BorderLeft)
            - left->Width - right->Width - size->Width - 2,
            GA_BottomBorder, FREEHORIZ);
        prevgad = data->wd_LeftButton = BorderButton(obj, prevgad, left,
            1 - left->Width - right->Width - size->Width, 1 - left->Height,
            GA_BottomBorder);
        data->wd_RightButton = BorderButton(obj, prevgad, right,
            1 - right->Width - size->Width, 1 - right->Height,
            GA_BottomBorder);
    }
}

/* return FALSE only if no resize (dx=dy=0) occured */
/* For a live native window, temporarily relax limits, request the
 * difference between desired inner and current outer dimensions, then
 * restore content-derived limits. Return whether a nonzero delta was
 * requested, not whether Intuition completed a resize. Borrow data and
 * window; no new ownership.
 */
/* not inlined: its locals stay off the stack of RecalcDisplay's layout */
static __attribute__((noinline)) BOOL WindowResize(struct MUI_WindowData *data)
{
    struct Window *win = data->wd_RenderInfo.mri_Window;
    int hborders = win->BorderLeft + win->BorderRight;
    int vborders = win->BorderTop + win->BorderBottom;
    WORD dx = data->wd_Width - win->Width + hborders;
    WORD dy = data->wd_Height - win->Height + vborders;

    if (dx || dy)
    {
        struct Screen *scr = win->WScreen;
        WORD left = win->LeftEdge, top = win->TopEdge;

        /* Temporarily disable window limits to let SizeWindow below work
           regardless of the previous limits */
        WindowLimits(win, 1, 1, -1, -1);
/*      D(bug("_zune_window_resize : dx=%d, dy=%d\n", dx, dy)); */
        /* A window that grows past the screen's right or bottom edge is
         * moved left or up just enough, as MUI 3.8 does (measured:
         * docs/lean/README.md "A window that grows past the screen");
         * SizeWindow alone stopped at the edge and cut the contents. */
        if (scr && left + win->Width + dx > scr->Width)
            left = MAX(0, scr->Width - (win->Width + dx));
        if (scr && top + win->Height + dy > scr->Height)
            top = MAX(0, scr->Height - (win->Height + dy));
        if (left != win->LeftEdge || top != win->TopEdge)
            ChangeWindowBox(win, left, top, win->Width + dx, win->Height + dy);
        else
            SizeWindow(win, dx, dy);
    }

    /* Set new window limits. A zero-delta SizeWindow would still make
     * Intuition refresh the frame, title and system gadgets; MUI 3.8
     * resizes nothing when the size is unchanged, and WindowLimits is
     * only needed when the limits changed (relayout cycles). */
    if (dx || dy
        || win->MinWidth != data->wd_MinMax.MinWidth + hborders
        || win->MinHeight != data->wd_MinMax.MinHeight + vborders
        || win->MaxWidth != data->wd_MinMax.MaxWidth + hborders
        || win->MaxHeight != data->wd_MinMax.MaxHeight + vborders)
        WindowLimits
            (win, data->wd_MinMax.MinWidth + hborders,
            data->wd_MinMax.MinHeight + vborders,
            data->wd_MinMax.MaxWidth + hborders,
            data->wd_MinMax.MaxHeight + vborders);

    return dx || dy;
}

/* Ask the retained help object to delete its owned bubble and clear the
 * borrowed object/bubble pointers. Optionally leave bubble mode, then reset
 * the first/later help ticker. Requires the help object to remain alive
 * through its callback; obj is unused and no status is returned.
 */
/* The ticks until the next help bubble: the settings' first delay, the
 * later one in bubble mode (a bubble was shown since the last click or
 * key) */
#define HELP_TICKER(data) ((data)->wd_HelpTicker = \
    ((data)->wd_Flags & MUIWF_BUBBLEMODE) \
    ? BUBBLEHELP_TICKER_LATER : BUBBLEHELP_TICKER_FIRST)

static void KillHelpBubble(struct MUI_WindowData *data, Object *obj,
    BOOL kill_bubblemode)
{
    if (data->wd_HelpObject)
    {
        DoMethod(data->wd_HelpObject, MUIM_DeleteBubble,
            (IPTR) data->wd_HelpBubble);
        data->wd_HelpObject = NULL;
        data->wd_HelpBubble = NULL;
    }

    if (kill_bubblemode)
        data->wd_Flags &= ~MUIWF_BUBBLEMODE;
    HELP_TICKER(data);
}

/**************/

/* Synchronous Area predicate used by ObjectUnderPointer. Receive a borrowed
 * live object and return whether it qualifies; do not change hierarchy or
 * retain ownership during traversal.
 */
typedef BOOL(*UNDERCHECK_FUNC) (Object *obj);

/* Return whether a live Area-compatible object has a non-NULL short-help
 * pointer. The pointer is only tested, not copied or retained; no state is
 * changed.
 */
static BOOL ShortHelpUnderPointerCheck(Object *obj)
{
    return muiAreaData(obj)->mad_ShortHelp ? TRUE : FALSE;
}

/* Recursively find the first drawable child/subtree at inclusive x/y bounds
 * that satisfies the supplied synchronous predicate, otherwise test obj
 * itself. Require a live Area root, stable child lists and callable
 * predicate. Return a borrowed match or NULL; do not retain ownership or
 * modify hierarchy during callbacks.
 */
static Object *ObjectUnderPointer(struct MUI_WindowData *data, Object *obj,
    LONG x, LONG y, UNDERCHECK_FUNC func)
{
    Object *cstate;
    Object *child;
    struct MinList *ChildList = NULL;

    if (!obj || !(muiAreaData(obj)->mad_Flags & MADF_CANDRAW))
        return NULL;

    if (!(x >= _left(obj) && x <= _right(obj)
            && y >= _top(obj) && y <= _bottom(obj)))
    {
        return NULL;
    }

    if ((get(obj, MUIA_Group_ChildList, &(ChildList)))
        && (ChildList != NULL))
    {
        cstate = ChildList ? (Object *)ChildList->mlh_Head : NULL;
        while ((child = NextObject(&cstate)))
        {
            Object *ret;

            if ((x >= _left(child) && x <= _right(child)
                    &&
                    y >= _top(child) && y <= _bottom(child))
                && (ret = ObjectUnderPointer(data, child, x, y, func)))
            {
                return ret;
            }
        }
    }

    if (!(*func) (obj))
        return NULL;

    return obj;
}

void ZuneResetWindowPointer(Object *window)
{
    struct MUI_WindowData *data = muiWindowData(window);

    if (data->wd_RenderInfo.mri_Window && !data->wd_SleepCount &&
        data->wd_PointerType != MUIV_PointerType_Normal)
    {
        SetWindowPointerA(data->wd_RenderInfo.mri_Window, NULL);
        data->wd_PointerType = MUIV_PointerType_Normal;
    }
}

static BOOL PointerUnderCheck(Object *obj)
{
    return TRUE;
}

/* The pointer's shape from the object under it (MUI 4/5's
 * MUIM_WhichPointerType, MUIA_PointerType): MUI 3.8 has none, so a MUI 3
 * program's mouse moves do not walk the tree for it. FALSE if the window
 * was closed or disposed meanwhile. Not inlined: HandleInputEvent's frame
 * is under every event's handlers. */
static __attribute__((noinline)) BOOL UpdatePointer(Object *oWin,
    struct MUI_WindowData *data, LONG x, LONG y,
    struct WindowEventCursor *cursor)
{
    Object *under;
    IPTR type = MUIV_PointerType_Parent;

    if (!data->wd_RenderInfo.mri_Window || data->wd_SleepCount ||
        data->wd_DragObject || !ZUNE_APP_MUI45(oWin))
        return TRUE;

    under = ObjectUnderPointer(data, data->wd_RootObject, x, y,
        PointerUnderCheck);
    while (under && under != oWin)
    {
        type = DoMethod(under, MUIM_WhichPointerType, x, y);
        /* Closing or disposing the window cancels this event. */
        if (!cursor->next || !data->wd_RenderInfo.mri_Window)
            return FALSE;
        if (type != (IPTR)MUIV_PointerType_Parent)
            break;
        under = _parent(under);
    }
    if (type == (IPTR)MUIV_PointerType_Parent)
        type = MUIV_PointerType_Normal;
    if (type != data->wd_PointerType)
        data->wd_PointerType = ZuneSetPointer(data->wd_RenderInfo.mri_Window,
            type, &data->wd_Pointer);
    return TRUE;
}

/* The test for a context menu under the pointer (ObjectUnderPointer
 * has checked that the object is drawn): the same walk as for the help
 * bubble and the pointer shape, which before had a copy of its own */
static BOOL ContextMenuUnderPointerCheck(Object *obj)
{
    return AREA_CONTEXTMENU(muiAreaData(obj)) ? TRUE : FALSE;
}

/**************/

/* If the stored active object is in the cycle chain, send GoActive once
 * according to the sent flag; otherwise clear the active pointer. Requires
 * stable live chain members and callback-safe window lifetime. Return no
 * status; this does not choose a replacement focus object.
 */
static void ActivateObject(struct MUI_WindowData *data)
{
    if (ChainIndex(data, data->wd_ActiveObject) >= 0)
    {
        if (!(data->wd_Flags & MUIWF_OBJECTGOACTIVESENT))
        {
            data->wd_Flags |= MUIWF_OBJECTGOACTIVESENT;
            DoMethod(data->wd_ActiveObject, MUIM_GoActive);
        }
    }
    else
        data->wd_ActiveObject = NULL;
}

/**************/

/* Process a live dequeued IntuiMessage while a drag context exists. Track
 * application-window targets, issue synchronous drag
 * query/begin/report/drop/finish callbacks and draw the drag image. Keep
 * source, destination, application lists and context alive through
 * callbacks. Return FALSE after replying when drag continues; TRUE after
 * releasing drag resources when finished, leaving the message for ordinary
 * event processing. The Lock report is not implemented.
 */
static BOOL HandleDragging(Object *oWin, struct MUI_WindowData *data,
    struct IntuiMessage *imsg)
{
    BOOL finish_drag = zune_drag_event(oWin, &data->wd_Drag, imsg);

    /* stop listening to IDCMP_MOUSEMOVE */
    if (finish_drag)
        ChangeEvents(data, GetDefaultEvents());
    return finish_drag;
}

static struct MenuItem *FindCaseShortcut(struct MenuItem *item, UBYTE key)
{
    for (; item; item = item->NextItem)
    {
        Object *object = (Object *)GTMENUITEM_USERDATA(item);
        STRPTR shortcut = NULL;
        IPTR command = FALSE;
        struct MenuItem *sub;
        if (!(item->Flags & ITEMENABLED)) continue;
        if (object)
        {
            get(object, MUIA_Menuitem_CommandString, &command);
            get(object, MUIA_Menuitem_Shortcut, &shortcut);
            if (shortcut == (STRPTR)MUIV_Menuitem_Shortcut_Check)
                get(object, MUIA_Menuitem_Title, &shortcut);
            if (!command && shortcut && shortcut[0] == key && !shortcut[1])
                return item;
        }
        if (item->SubItem && (sub = FindCaseShortcut(item->SubItem, key)))
            return sub;
    }
    return NULL;
}

/* IDCMP_MENUVERIFY (only windows of programs that get MUI 4/5 ask for
 * it): MUIM_Menustrip_WillOpen before a menu opens, the message answered
 * after it (or by the window's closing). Not inlined: the guard stays off
 * HandleWindowEvent's frame, which every event (a resize's whole redraw
 * too) runs under. */
static __attribute__((noinline)) void MenuVerify(Object *oWin,
    struct MUI_WindowData *data, struct IntuiMessage *imsg)
{
    struct ZuneWindowGuard guard = {0};

    if (data->wd_Menustrip && imsg->Code == MENUHOT)
    {
        guard.menu_message = imsg;
        ZuneWatchWindow(oWin, &guard);
        DoMethod(data->wd_Menustrip, MUIM_Menustrip_WillOpen);
        if (guard.closed)
            return;
        ZuneUnwatchWindow(oWin, &guard);
    }
    ReplyMsg((struct Message *)imsg);
}

/* IDCMP_MENUPICK, after the message was replied: the item's object gets
 * its check state and MUIA_Menuitem_Trigger (a copy of the item: the
 * notifications may rebuild the menus), then the application and the
 * window MUIA_..._MenuAction and the application the item's ID. Not
 * inlined: the copy stays off HandleWindowEvent's frame. */
static __attribute__((noinline)) void MenuPick(Object *oWin,
    struct MUI_WindowData *data, UWORD code)
{
    struct MenuItem *item, selected;
    Object *item_obj, *app = NULL;
    IPTR udata = 0;

    if (!data->wd_Menu || MENUNUM(code) == NOMENU || ITEMNUM(code) == NOITEM
        || !(item = ItemAddress(data->wd_Menu, code))
        || !(item_obj = (Object *) GTMENUITEM_USERDATA(item)))
        return;
    selected = *item;
    if (selected.Flags & CHECKIT)
        set(item_obj, MUIA_Menuitem_Checked, !!(selected.Flags & CHECKED));
    set(item_obj, MUIA_Menuitem_Trigger, (IPTR) &selected);
    if (get(oWin, MUIA_ApplicationObject, &app) && app
        && get(item_obj, MUIA_UserData, &udata))
    {
        set(app, MUIA_Application_MenuAction, udata);
        set(oWin, MUIA_Window_MenuAction, udata);
        DoMethod(app, MUIM_Application_ReturnID, udata);
    }
}

/* Reply to imsg if handled */
/* Handle activation, geometry, refresh, close, menu, scrollbar and help
 * events for a live open window. Return TRUE for consumed events and
 * normally reply those messages; FALSE leaves the message for further
 * dispatch. Callback targets and root must remain live. The MENUPICK branch
 * replies before subsequently reading imsg->Code, so its ordering does not
 * preserve that message field lifetime.
 */
BOOL HandleWindowEvent(Object *oWin, struct MUI_WindowData *data,
    struct IntuiMessage *imsg)
{
    struct Window *iWin;
    BOOL is_handled = TRUE;
    BOOL replied = FALSE;

    iWin = imsg->IDCMPWindow;
    switch (imsg->Class)
    {
    case IDCMP_ACTIVEWINDOW:
        data->wd_Flags |= MUIWF_ACTIVE | MUIWF_ACTIVEEVENT;
        if (data->wd_OldActive)
            set(oWin, MUIA_Window_ActiveObject, data->wd_OldActive);
        set(oWin, MUIA_Window_Activate, TRUE);
        data->wd_Flags &= ~MUIWF_ACTIVEEVENT;
        is_handled = FALSE;     /* forwardable to area event handlers */
        break;

    case IDCMP_INACTIVEWINDOW:
        KillHelpBubble(data, oWin, TRUE);
        if (data->wd_ActiveObject)
        {
            data->wd_OldActive = data->wd_ActiveObject;
            set(oWin, MUIA_Window_ActiveObject,
                MUIV_Window_ActiveObject_None);
        }
        data->wd_Flags &= ~MUIWF_ACTIVE;
        set(oWin, MUIA_Window_Activate, FALSE);
        is_handled = FALSE;     /* forwardable to area event handlers */
        break;

    case IDCMP_NEWSIZE:
    case IDCMP_CHANGEWINDOW:
        /* set window limits according to window contents */
        ContentLimits(data, iWin);

        /* The notifications go through wd_Class, the Window class, not
         * OCLASS(oWin): a subclass's OM_SET would reach Window's, which
         * takes MUIA_Window_Width/Height as requests (the next move would
         * then look like a resize). */
        if ((iWin->GZZWidth != data->wd_Width)
            || (iWin->GZZHeight != data->wd_Height))
        {
            data->wd_Width = iWin->GZZWidth;
            data->wd_Height = iWin->GZZHeight;
            DoHideMethod(data->wd_RootObject);

            data->wd_Flags |= MUIWF_RESIZING;
            RefreshWindow(oWin, data);
            superset(data->wd_Class, oWin, MUIA_Window_Width, data->wd_Width);
            superset(data->wd_Class, oWin, MUIA_Window_Height, data->wd_Height);
        }
        if (iWin->LeftEdge != data->wd_X)
        {
            data->wd_X = iWin->LeftEdge;
            superset(data->wd_Class, oWin, MUIA_Window_LeftEdge, data->wd_X);
        }
        if (iWin->TopEdge != data->wd_Y)
        {
            data->wd_Y = iWin->TopEdge;
            superset(data->wd_Class, oWin, MUIA_Window_TopEdge, data->wd_Y);
        }

        is_handled = FALSE;     /* forwardable to area event handlers */
        break;

    case IDCMP_REFRESHWINDOW:
        ReplyMsg((struct Message *)imsg);
        replied = TRUE;
        RefreshWindow(oWin, data);
        break;

    case IDCMP_CLOSEWINDOW:
        ReplyMsg((struct Message *)imsg);
        replied = TRUE;
        set(oWin, MUIA_Window_CloseRequest, TRUE);
        break;

    case IDCMP_MENUVERIFY:
        MenuVerify(oWin, data, imsg);
        return TRUE;

    case IDCMP_MENUPICK:
    {
        UWORD code = imsg->Code;
        ReplyMsg((struct Message *)imsg);
        replied = TRUE;
        MenuPick(oWin, data, code);
        break;
    }

    case IDCMP_IDCMPUPDATE:
        is_handled = FALSE;     /* forwardable to area event handlers */
        if (data->wd_VertProp || data->wd_HorizProp)
        {
            /* the border arrows: decrease or increase their scroller's
             * prop object (the prop gadget's user data, classes/prop.c);
             * wd_VertProp, its up and down button, wd_HorizProp, its left
             * and right button in this order */
            struct TagItem *tag =
                FindTagItem(GA_ID, (struct TagItem *)imsg->IAddress);
            Object **g = &data->wd_VertProp;
            int i;

            for (i = 0; tag && i < 6; i += 3)
            {
                ULONG method = 0;

                if (!g[i])
                    continue;
                if (tag->ti_Data == GADGETID(g[i + 1]))
                    method = MUIM_Prop_Decrease;
                else if (tag->ti_Data == GADGETID(g[i + 2]))
                    method = MUIM_Prop_Increase;
                if (method)
                {
                    Object *prop = (Object *)G(g[i])->UserData;

                    is_handled = TRUE;
                    if (prop)
                        DoMethod(prop, method, 1);
                    break;
                }
            }
        }
        break;

    case IDCMP_INTUITICKS:
        if (data->wd_HelpTicker && !--data->wd_HelpTicker)
        {
            Object *underobj = ObjectUnderPointer(data, data->wd_RootObject,
                imsg->MouseX, imsg->MouseY, ShortHelpUnderPointerCheck);

            if (underobj != data->wd_HelpObject)
            {
                KillHelpBubble(data, oWin, FALSE);
                if (underobj && (data->wd_HelpBubble = (APTR)DoMethod(
                        underobj, MUIM_CreateBubble, imsg->MouseX,
                        imsg->MouseY, 0, 0)))
                {
                    data->wd_HelpObject = underobj;
                    data->wd_Flags |= MUIWF_BUBBLEMODE;
                }
            }
            HELP_TICKER(data);
        }

        is_handled = FALSE;     /* forwardable to area event handlers */
        break;

    case IDCMP_MOUSEBUTTONS:
        KillHelpBubble(data, oWin, TRUE);
        is_handled = FALSE;
        break;


    case IDCMP_MOUSEMOVE:
        KillHelpBubble(data, oWin, FALSE);
        is_handled = FALSE;
        break;

    default:
        is_handled = FALSE;
        break;
    }

    if (is_handled && !replied)
        ReplyMsg((struct Message *)imsg);

    return is_handled;
}

/* The cursor lives on the dispatching function's stack while it is linked
 * here: both callers unlink it at "finished" before they return, and
 * OM_DISPOSE unlinks every cursor (next = NULL) when a handler disposes the
 * window, so GCC's -Wdangling-pointer is a false alarm (code audit,
 * stability round 2). */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
static void BeginEventDispatch(struct MUI_WindowData *data,
    struct WindowEventCursor *cursor)
{
    cursor->previous = data->wd_EventCursors;
    cursor->next = data->wd_EHList.mlh_Head;
    data->wd_EventCursors = cursor;
}
#pragma GCC diagnostic pop

static struct MinNode *NextEventHandler(struct WindowEventCursor *cursor)
{
    struct MinNode *node = cursor->next;

    if (!node || !node->mln_Succ)
        return NULL;
    /* Never revisit this node after a callback: its owner may free it. */
    cursor->next = node->mln_Succ;
    return node;
}

static void RemoveEventHandler(struct MUI_WindowData *data,
    struct MUI_EventHandlerNode *handler)
{
    struct WindowEventCursor *cursor;
    struct MinNode *node = (struct MinNode *)handler;

    /* Repair outer dispatches as well as a possible nested dispatch. */
    for (cursor = data->wd_EventCursors; cursor; cursor = cursor->previous)
    {
        if (cursor->next == node)
            cursor->next = node->mln_Succ;
    }
    Remove((struct Node *)node);
}

/* Invoke one borrowed live handler with a borrowed message and muikey after
 * GUIMODE visibility/draw and virtual click tests. Use CoerceMethod when
 * ehn_Class exists, otherwise DoMethod. Return HandleEvent result or zero
 * for skipped/private HandleInput delivery; HandleInput results are
 * discarded. Disabled checks are performed by caller paths, not uniformly
 * here.
 */
static ULONG InvokeEventHandler(struct MUI_EventHandlerNode *ehn,
    struct IntuiMessage *event, ULONG muikey,
    struct WindowEventCursor *cursor)
{
    ULONG res;

    if (ehn->ehn_Flags & MUI_EHF_GUIMODE)
    {
        if (!(_flags(ehn->ehn_Object) & MADF_CANDRAW))
            return 0;
        if (!(_flags(ehn->ehn_Object) & MADF_SHOWME))
            return 0;
    }

    if (event != NULL
        && event->Class == IDCMP_MOUSEBUTTONS
        && event->Code == SELECTDOWN
        && (_flags(ehn->ehn_Object) & MADF_INVIRTUALGROUP))
    {
        /*
           Here we filter out SELECTDOWN messages if objects is in a virtual
           group but the click went out of the virtual group
         */
        Object *obj = ehn->ehn_Object;
        Object *parent = obj;
        Object *wnd = _win(obj);

        while (get(parent, MUIA_Parent, &parent))
        {
            if (!cursor->next)
                return 0;
            if (!parent)
                break;
            if (wnd == parent)
                break;
            if (_flags(parent) & MADF_ISVIRTUALGROUP)
            {
                if (event->MouseX < _mleft(parent)
                    || event->MouseX > _mright(parent)
                    || event->MouseY < _mtop(parent)
                    || event->MouseY > _mbottom(parent))
                {
                    return 0;
                }
            }
        }
        if (!cursor->next)
            return 0;

    }

    if (ehn->ehn_Flags & MUI_EHF_HANDLEINPUT)
    {
        DoMethod(ehn->ehn_Object, MUIM_HandleInput, (IPTR) event, muikey);
        res = 0;
    }
    else
    {
        if (ehn->ehn_Class)
            res = CoerceMethod
                (ehn->ehn_Class, ehn->ehn_Object, MUIM_HandleEvent,
                (IPTR) event, muikey);
        else
            res =
                DoMethod(ehn->ehn_Object, MUIM_HandleEvent, (IPTR) event,
                muikey);
    }

    return res;
}

/* Construct a temporary InputEvent for synchronous notifications, copy the
 * raw IntuiMessage and dead-key address data, then reply the original
 * before dispatch. Apply configured built-in actions unless masked, then
 * active/parent/default/other/control-character handlers. Borrow all nodes
 * and targets throughout traversal. Return no status. The parent loop
 * starts with the active object again; callbacks must not retain stack
 * event pointers or alter traversed lists.
 */
/* A case-sensitive menu's shortcut (the displayed command string, the
 * raw key and its qualifiers matched exactly): trigger the item as a menu
 * pick does. TRUE when one was found. Not inlined: its copy of the item
 * and the guard stay off HandleRawkey's frame, which every key passes. */
static __attribute__((noinline)) BOOL CaseShortcut(Object *win,
    struct MUI_WindowData *data, struct IntuiMessage *event, ULONG key)
{
    IPTR sensitive = FALSE;
    if (!(event->Code & IECODE_UP_PREFIX) &&
        (event->Qualifier & IEQUALIFIER_RCOMMAND) && data->wd_Menustrip &&
        get(data->wd_Menustrip, MUIA_Menustrip_CaseSensitive, &sensitive) && sensitive)
    {
        struct Menu *menu;
        for (menu = data->wd_Menu; menu; menu = menu->NextMenu)
        {
            struct MenuItem *item;
            if (!(menu->Flags & MENUENABLED) ||
                !(item = FindCaseShortcut(menu->FirstItem, key))) continue;
            Object *object = (Object *)GTMENUITEM_USERDATA(item);
            Object *app = _app(win);
            struct MenuItem selected = *item;
            struct ZuneWindowGuard guard = {0};
            IPTR id = 0;
            get(object, MUIA_UserData, &id);
            ZuneWatchWindow(win, &guard);
            if (selected.Flags & CHECKIT)
            {
                if (selected.Flags & MENUTOGGLE) selected.Flags ^= CHECKED;
                else selected.Flags |= CHECKED;
                set(object, MUIA_Menuitem_Checked, !!(selected.Flags & CHECKED));
                if (guard.closed) return TRUE;
            }
            set(object, MUIA_Menuitem_Trigger, &selected);
            if (guard.closed) return TRUE;
            set(app, MUIA_Application_MenuAction, id);
            if (guard.closed) return TRUE;
            set(win, MUIA_Window_MenuAction, id);
            if (guard.closed) return TRUE;
            DoMethod(app, MUIM_Application_ReturnID, id);
            if (!guard.closed) ZuneUnwatchWindow(win, &guard);
            return TRUE;
        }
    }
    return FALSE;
}

/* The key handlers of one object (IDCMP_RAWKEY or MUI_EHF_ALWAYSKEYS) in
 * the list's order; TRUE when the key was eaten or the dispatch must end.
 * With watch, stop at the first handler after which another object is
 * active. */
static __attribute__((noinline)) BOOL KeyToObject(struct MUI_WindowData *data,
    Object *target, struct IntuiMessage *event, LONG muikey,
    struct WindowEventCursor *cursor, Object *active, BOOL watch)
{
    struct MinNode *mn;

    for (cursor->next = data->wd_EHList.mlh_Head;
        (mn = NextEventHandler(cursor)); )
    {
        struct MUI_EventHandlerNode *ehn = (struct MUI_EventHandlerNode *)mn;

        if ((ehn->ehn_Object == target)
            && ((ehn->ehn_Events & IDCMP_RAWKEY)
                || (ehn->ehn_Flags & MUI_EHF_ALWAYSKEYS)))
        {
            ULONG res = InvokeEventHandler(ehn, event, muikey, cursor);
            if (!cursor->next || (res & MUI_EventHandlerRC_Eat))
                return TRUE;
            /* Leave the loop if a different object has been activated */
            if (watch && active != data->wd_ActiveObject)
                break;
        }
    }
    return FALSE;
}

/* The MUI key (the settings' key page) a raw key event is, or MUIKEY_NONE.
 * A key whose code differs in the bits an expression compares cannot match
 * it: the commodities call is skipped then (every key event asked MatchIX
 * for all 23 keys, the prefs pointer read again for each). */
static __attribute__((noinline)) LONG MatchMuiKey(struct ZunePrefsNew *prefs,
    struct InputEvent *ie)
{
    LONG muikey;

    for (muikey = MUIKEY_COUNT - 1; muikey >= MUIKEY_PRESS; muikey--)
    {
        if (prefs->muikeys[muikey].ix_well != 0
            && !((ie->ie_Code ^ prefs->muikeys[muikey].ix.ix_Code)
                & prefs->muikeys[muikey].ix.ix_CodeMask)
            && MatchIX(ie, &prefs->muikeys[muikey].ix))
        {
            if (muikey == MUIKEY_PRESS && (ie->ie_Code & IECODE_UP_PREFIX))
                muikey = MUIKEY_RELEASE;
            return muikey;
        }
    }
    return MUIKEY_NONE;
}

extern const struct __MUIBuiltinClass _MUI_Window_desc;
IPTR Notify__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg);

/* HandleRawkey's input event for a raw key message: MUIA_Window_InputEvent
 * set to it (its notifications), then the MUI key it is (MatchMuiKey, which
 * has no side effects; MatchIX compares class, code and qualifier only, so
 * the event serves without its dead keys). Window's OM_SET passes
 * MUIA_Window_InputEvent on to Notify's: for the Window class itself (no
 * subclass that may want the set) Notify's is called directly, without
 * intuition's SetAttrs and Window's tag switch. Its own frame holds the
 * event, so HandleRawkey's, which every key handler runs under, does not. */
static __attribute__((noinline)) LONG WindowInputEvent(Object *win,
    struct IntuiMessage *event)
{
    struct IClass *cl = OCLASS(win);
    struct InputEvent ie = { 0 };
    struct TagItem tags[] = {{MUIA_Window_InputEvent, (IPTR)&ie},
        {TAG_DONE, 0}};
    struct opSet msg = {OM_SET, tags, NULL};

    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = event->Code;
    ie.ie_Qualifier = event->Qualifier;
    ie.ie_EventAddress = event->IAddress ? *(APTR *)event->IAddress : NULL;
#ifdef __AMIGAOS4__
    ie.ie_TimeStamp.Seconds = event->Seconds;
    ie.ie_TimeStamp.Microseconds = event->Micros;
#else
    ie.ie_TimeStamp.tv_secs = event->Seconds;
    ie.ie_TimeStamp.tv_micro = event->Micros;
#endif
    if (cl->cl_Dispatcher.h_SubEntry == (HOOKFUNC)_MUI_Window_desc.dispatcher)
        Notify__OM_SET(cl->cl_Super, win, &msg);
    else
        SetAttrsA(win, tags);
    ie.ie_EventAddress = NULL;
    return MatchMuiKey(muiGlobalInfo(win)->mgi_Prefs, &ie);
}

static __attribute__((noinline)) BOOL ActiveInTree(struct MUI_WindowData *data)
{
    Object *o = data->wd_ActiveObject;
    int depth;

    for (depth = 0; o && !((IPTR)o & 1) && depth < 64; depth++)
    {
        if (o == data->wd_RootObject)
            return TRUE;
        o = _parent(o);
    }
    return DoMethod(data->wd_RootObject, MUIM_FindAreaObject,
        (IPTR) data->wd_ActiveObject) != (IPTR) NULL;
}

static __attribute__((noinline)) void HandleRawkey(Object *win, struct MUI_WindowData *data,
    struct IntuiMessage *event)
{
    struct MinNode *mn;
    struct WindowEventCursor cursor;
    struct MUI_EventHandlerNode *ehn;
    struct IntuiMessage imsg_copy;
    ULONG res;
    LONG muikey;
    Object *active_object = NULL;
    IPTR disabled = 0;
    ULONG key;
    ULONG deadkey;

    KillHelpBubble(data, win, BUBBLEHELP_TICKER_FIRST);

    /* MUIA_Window_InputEvent's notifications, and the MUI key the event
     * is (computed here, used below unless a menu shortcut takes it) */
    muikey = WindowInputEvent(win, event);

    /* get the vanilla key for control char */
    {
        UWORD msg_code;

        /* Remove the up prefix as convert key does not convert upkey event */
        msg_code = event->Code;
        event->Code &= ~IECODE_UP_PREFIX;
        key = ConvertKey(event);
        event->Code = msg_code;
    }

    imsg_copy = *event;
    deadkey = *(ULONG *) event->IAddress;
    imsg_copy.IAddress = &deadkey;
    ReplyMsg((struct Message *)event);
    event = &imsg_copy;

    /* Case-sensitive menus use displayed command strings, leaving the
     * raw key and its qualifiers available for exact keymap matching. */
    if (CaseShortcut(win, data, event, key))
        return;
    //bug("rawkey: code=%lx, qual=%lx\n", event->Code, event->Qualifier);

    /* the predefined keystroke the message is: muikey, above */

    if ((muikey != MUIKEY_NONE) && !(data->wd_DisabledKeys & (1 << muikey)))
    {
        D(bug("HandleRawkey: try MUIKEY %ld on window %0x08lx\n", muikey,
                win));
        switch (muikey)
        {
        case MUIKEY_PRESS:
            break;
        case MUIKEY_TOGGLE:
            break;
        case MUIKEY_UP:
            break;
        case MUIKEY_DOWN:
            break;
        case MUIKEY_PAGEUP:
            break;
        case MUIKEY_PAGEDOWN:
            break;
        case MUIKEY_TOP:
            break;
        case MUIKEY_BOTTOM:
            break;
        case MUIKEY_LEFT:
            break;
        case MUIKEY_RIGHT:
            break;
        case MUIKEY_WORDLEFT:
            break;
        case MUIKEY_WORDRIGHT:
            break;
        case MUIKEY_LINESTART:
            break;
        case MUIKEY_LINEEND:
            break;
        case MUIKEY_GADGET_NEXT:
            set(win, MUIA_Window_ActiveObject,
                MUIV_Window_ActiveObject_Next);
            break;
        case MUIKEY_GADGET_PREV:
            set(win, MUIA_Window_ActiveObject,
                MUIV_Window_ActiveObject_Prev);
            break;
        case MUIKEY_GADGET_OFF:
            set(win, MUIA_Window_ActiveObject,
                MUIV_Window_ActiveObject_None);
            break;
        case MUIKEY_WINDOW_CLOSE:
            set(win, MUIA_Window_CloseRequest, TRUE);
            break;
        case MUIKEY_WINDOW_NEXT:
            break;
        case MUIKEY_WINDOW_PREV:
            break;
        case MUIKEY_HELP:
            break;
        case MUIKEY_POPUP:
            break;
        default:
            break;
        }
    }

    active_object = NULL;
    if ((data->wd_ActiveObject != NULL) && ActiveInTree(data))
    {
        active_object = data->wd_ActiveObject;
        /* an Area in the tree: its own state, no GetAttr (two per key,
         * opbench "count") */
        disabled = muiAreaData(active_object)->mad_DisableCount;
    }
    else
        data->wd_ActiveObject = NULL;

    BeginEventDispatch(data, &cursor);

    /* try ActiveObject */
    if ((active_object != NULL) && !disabled)
    {
#if 0
        /* sba:
         ** Which method should be used for muikeys? MUIM_HandleInput or
         ** MUIM_HandleEvent. Also note that there is a flag MUI_EHF_ALWAYSKEYS
         ** which probably means that all keys events are requested??
         ** For now MUIM_HandleEvent is used as this is currently implemented
         ** in Area class ;) although I guess it should be MUIM_HandleInput as
         ** this was earlier
         */

        if (muikey != MUIKEY_NONE)
        {
            res =
                DoMethod(active_object, MUIM_HandleEvent, (IPTR) event,
                muikey);
            if (!cursor.next || (res & MUI_EventHandlerRC_Eat))
                goto finished;
        }
#endif
        if (KeyToObject(data, active_object, event, muikey, &cursor,
                active_object, TRUE))
            goto finished;

        /* Not eaten: MUI 3.8 and MUI 5 offer the key to the other
         * handlers in the list's order next (tests/native/keyoffers.c:
         * the active object once, then an object with a higher priority
         * before the active object's parent group). The parents are not
         * walked first: that offered the key to the active object a
         * second time and to a parent twice. */
    }

    D(bug("HandleRawkey: try default object handlers\n"));

    /* try DefaultObject */
    disabled = FALSE;
    if (data->wd_DefaultObject != NULL)
        get(data->wd_DefaultObject, MUIA_Disabled, &disabled);
    if (!cursor.next)
        goto finished;

    if ((data->wd_DefaultObject != NULL) && !disabled
        && (active_object != data->wd_DefaultObject))
    {
        /* No, we only should do this if the object actually has requested
         * this via RequestIDCMP()! */
//     if (muikey != MUIKEY_NONE
//         && (_flags(data->wd_DefaultObject) & MADF_CANDRAW))
//     {
//         DoMethod(data->wd_DefaultObject, MUIM_HandleInput, event, muikey);
//         goto finished;
//     }

        if (KeyToObject(data, data->wd_DefaultObject, event, muikey,
                &cursor, NULL, FALSE))
            goto finished;
    }

    D(bug("HandleRawkey: try other handlers\n"));

    // try other handlers
    for (cursor.next = data->wd_EHList.mlh_Head;
            (mn = NextEventHandler(&cursor)); )
    {
        ehn = (struct MUI_EventHandlerNode *)mn;

        // skip Active and Default object as they have already been
        // handled
        if (ehn->ehn_Object == data->wd_ActiveObject
            || ehn->ehn_Object == data->wd_DefaultObject)
            continue;

        if (ehn->ehn_Events & IDCMP_RAWKEY)
        {
            //D(bug("HandleRawkey: (others) invoking on %p (ehn=%p) "
            //"event=%p muikey=%p\n",
            //ehn->ehn_Object, ehn, event, muikey));
            res = InvokeEventHandler(ehn, event, MUIKEY_NONE, &cursor);
            //D(bug("HandleRawkey: (others) got res=%d\n", res));
            if (!cursor.next || (res & MUI_EventHandlerRC_Eat))
                goto finished;
        }
    }

    D(bug("HandleRawkey: try control chars handlers\n"));

    /* try Control Chars */
    //bug("ctrlchar, key='%c' code=0x%08lx\n", key, event->Code);
    if (key)
    {
        for (cursor.next = data->wd_CCList.mlh_Head;
            (mn = NextEventHandler(&cursor)); )
        {
            ehn = (struct MUI_EventHandlerNode *)mn;

            if (ehn->ehn_Events == key)
            {
                IPTR disabled = 0;
                LONG muikey2 = ehn->ehn_Flags;

                get(ehn->ehn_Object, MUIA_Disabled, &disabled);
                if (!cursor.next)
                    goto finished;
                if (disabled)
                    continue;

                //bug("control char\n");
                if (event->Code & IECODE_UP_PREFIX)
                {
                    /* simulate a release */
                    if (muikey2 == MUIKEY_PRESS)
                        muikey2 = MUIKEY_RELEASE;
                    else
                        goto finished;
                }

                if ((muikey2 != MUIKEY_NONE)
                    && (_flags(ehn->ehn_Object) & MADF_CANDRAW)
                    && (_flags(ehn->ehn_Object) & MADF_SHOWME))
                {
                    res = CoerceMethod
                        (ehn->ehn_Class, ehn->ehn_Object, MUIM_HandleEvent,
                        (IPTR) NULL, muikey2);
                    if (!cursor.next || (res & MUI_EventHandlerRC_Eat))
                        goto finished;
                }
            }
        }
    }
finished:
    if (cursor.next)
        data->wd_EventCursors = cursor.previous;
}

/* forward non-keystroke events to event handlers */
/* Dispatch a borrowed event to matching non-disabled handler objects. For
 * events other than IDCMPUPDATE, copy the message, clear IAddress and reply
 * before callbacks. IDCMPUPDATE retains the original until final ReplyMsg;
 * an Eat return currently bypasses that reply. Update the native context-
 * menu trap on mouse move. Return no status and require stable live handler
 * nodes.
 */
static void HandleInputEvent(Object *win, struct MUI_WindowData *data,
    struct IntuiMessage *event)
{
    struct MinNode *mn;
    struct WindowEventCursor cursor;
    struct MUI_EventHandlerNode *ehn;
    struct IntuiMessage imsg_copy;
    ULONG res;
    ULONG mask = event->Class;

    if (mask != IDCMP_IDCMPUPDATE)
    {
        imsg_copy = *event;
        imsg_copy.IAddress = NULL;      /* be sure to trap access to that */
        ReplyMsg((struct Message *)event);
        event = &imsg_copy;
    }

    BeginEventDispatch(data, &cursor);
    if (mask == IDCMP_MOUSEMOVE)
    {
        struct Window *iWin;
        iWin = event->IDCMPWindow;
        if (!UpdatePointer(win, data, event->MouseX, event->MouseY, &cursor))
            goto finished;

        if (ObjectUnderPointer(data, data->wd_RootObject,
                event->MouseX, event->MouseY, ContextMenuUnderPointerCheck))
        {
            AROS_ATOMIC_OR(iWin->Flags, WFLG_RMBTRAP);
        }
        else if (!data->wd_NoMenus && data->wd_Menu)
        {
            AROS_ATOMIC_AND(iWin->Flags, ~WFLG_RMBTRAP);
        }
    }

    for (cursor.next = data->wd_EHList.mlh_Head;
        (mn = NextEventHandler(&cursor)); )
    {
        ehn = (struct MUI_EventHandlerNode *)mn;

        if (ehn->ehn_Events & mask)
        {
            /* The object's own state, not an OM_GET through intuition's
             * GetAttr and every class of the object: 32 calls per button
             * click (opbench "count"). MUI 3.8 and MUI 5 send no OM_GET
             * of MUIA_Disabled here (tests/native/disabledget.c); a
             * handler's object is an Area (InvokeEventHandler reads its
             * flags too). */
            if (muiAreaData(ehn->ehn_Object)->mad_DisableCount)
                continue;

            res = InvokeEventHandler(ehn, event, MUIKEY_NONE, &cursor);
            if (!cursor.next || (res & MUI_EventHandlerRC_Eat))
                goto finished;

        }
    }

    if (mask == IDCMP_IDCMPUPDATE)
        ReplyMsg((struct Message *)event);
finished:
    if (cursor.next)
        data->wd_EventCursors = cursor.previous;
}


/* Called from application.c */
/* Consume a dequeued application-port IntuiMessage whose native IDCMPWindow
 * and UserData identify a live Zune Window. Route sleeping, drag, window,
 * raw-key and other handler paths. No result is returned; callbacks may
 * synchronously act on the application. Most paths reply, but an eaten
 * IDCMPUPDATE lacks a reply. Callers must not assume unconditional reply
 * or reuse a possibly replied message.
 */
void _zune_window_message(struct IntuiMessage *imsg)
{
    struct Window *iWin;
    Object *oWin;
    struct MUI_WindowData *data;
    BOOL handled;

    iWin = imsg->IDCMPWindow;
    if (!iWin || !iWin->UserData) {
        ReplyMsg((struct Message *)imsg);
        return;
    }
    oWin = (Object *) iWin->UserData;
    data = muiWindowData(oWin);

    if (data->wd_SleepCount > 0)
    {
        BOOL refresh = FALSE;
        /* Window is sleeping, so we just ignore (and reply) all messages.
         * MUI 3.8/AmigaOS3 also receives all messages (IDCMP Flags
         * are not modified during sleeping). MUI refreshes the window
         * contents, so it seems to handle IDCMP_REFRESHWINDOW messages.
         * If any other messages are handled by MUI is unsure/not tested.
         */
        if (imsg->Class == IDCMP_REFRESHWINDOW)
        {
            refresh=TRUE;
        }
        ReplyMsg((struct Message *)imsg);
        if (refresh)
        {
            RefreshWindow(oWin, data);
        }
        return;
    }

    if (data->wd_DragObject)
    {
        if (!HandleDragging(oWin, data, imsg))
            return;
    }

    handled = HandleWindowEvent(oWin, data, imsg);
    if (!handled)
    {
        if (IDCMP_RAWKEY == imsg->Class)
            HandleRawkey(oWin, data, imsg);
        else if (IDCMP_GADGETUP == imsg->Class)
        {
#ifdef ZUNE68_GCC_NATIVE
            int title = TitleGadgetIndex(data, imsg->IAddress);
            if (title >= 0)
            {
                ULONG seconds = imsg->Seconds, micros = imsg->Micros;
                /* Iconification can close this window and free its gadgets. */
                ReplyMsg((struct Message *)imsg);
                TitleGadgetUp(oWin, data, title, seconds, micros);
            }
#else
            ULONG action = 0;
#if defined(__AROS__)
            if (imsg->IAddress)
            {
                if (ETI_MUI == ((struct Gadget *)imsg->IAddress)->GadgetID)
                    action = MUIV_Window_Button_MUI;
                else if (ETI_Iconify == ((struct Gadget *)imsg->IAddress)->GadgetID)
                    action = MUIV_Window_Button_Iconify;
            }
#endif
            if (action)
            {
                Object *app = _app(oWin);
                /* Iconification can close this window and free its gadgets. */
                ReplyMsg((struct Message *)imsg);
                if (action == MUIV_Window_Button_MUI)
                    DoMethod(app, MUIM_Application_OpenConfigWindow);
                else if (action == MUIV_Window_Button_Iconify)
                    set(app, MUIA_Application_Iconified, TRUE);
            }
#endif
            else
                HandleInputEvent(oWin, data, imsg);
        }
        else
        {
            HandleInputEvent(oWin, data, imsg);
        }
    }
}

/**************************************************************************/
/**************************************************************************/

static ULONG WindowClose(struct IClass *cl, Object *obj);

/* code for setting MUIA_Window_RootObject */
/* While closed, disconnect the old root, install the new borrowed input as
 * the owned current root, and connect it when global information exists. Do
 * not dispose the detached old root; the caller must manage it. While open
 * do nothing. Clear focus only when equal to the old root, not a
 * descendant. Return no status.
 */
static __attribute__((noinline)) void ChangeRootObject(struct MUI_WindowData *data, Object *obj,
    Object *newRoot)
{
    Object *oldRoot;

    ASSERT_VALID_PTR(data);
    ASSERT_VALID_PTR(obj);

    oldRoot = data->wd_RootObject;
    if (!(data->wd_Flags & MUIWF_OPENED))
    {
        if (oldRoot)
        {
            if (data->wd_ActiveObject == oldRoot)
                set(obj, MUIA_Window_ActiveObject,
                    MUIV_Window_ActiveObject_None);
            DoMethod(oldRoot, MUIM_DisconnectParent);
            /* MUI 5 disposes the Scrollgroup it added (its ChangeLog) */
            if (data->wd_Flags & MUIWF_SCROLLWRAP)
            {
                zune_window_scrollwrap(oldRoot, ZUNE_SW_UNWRAP);
                data->wd_Flags &= ~MUIWF_SCROLLWRAP;
            }
        }

        data->wd_RootObject = newRoot;
        if (newRoot)
        {
            /* if window is in App tree, inform child */
            if (muiNotifyData(obj)->mnd_GlobalInfo)
                DoMethod(newRoot, MUIM_ConnectParent, (IPTR) obj);
        }
    }
}

static LONG ChainIndex(struct MUI_WindowData *data, Object *obj)
{
    Object **p = data->wd_Chain;
    LONG i, n = data->wd_ChainCount;

    if (obj)
        for (i = 0; i < n; i++)
            if (p[i] == obj)
                return i;
    return -1;
}

/* From chain index i forwards (next) or backwards, the first non-disabled
 * object with SHOWME set; NULL at the end (no wrapping). With i = -1 or the
 * count and no filter (first/last): the first or last object at all. */
static __attribute__((noinline)) Object *ChainStep(
    struct MUI_WindowData *data, LONG i, BOOL next,
    BOOL filter)
{
    for (;;)
    {
        Object *obj;

        i += next ? 1 : -1;
        if (i < 0 || i >= data->wd_ChainCount)
            return NULL;
        if ((obj = data->wd_Chain[i]))
        {
            IPTR is_disabled = 0;

            if (!filter)
                return obj;
            get(obj, MUIA_Disabled, &is_disabled);
            if (!is_disabled && (_flags(obj) & MADF_SHOWME))
                return obj;
        }
    }
}

/**************************************************************************
 Code for setting MUIA_Window_ActiveObject
Basically, it will:
- remove focus drawing for current active object
- find (if needed) the new active object
- set data->wd_ActiveObject to the new object
- draw focus around the new active object
**************************************************************************/
/* While closed, remember the pointer or special selection value for later.
 * While open, deactivate the previous valid setup target, resolve
 * None/Next/Prev or store an explicit borrowed target, and send GoActive
 * only to an eligible root descendant. Wraparound first/last fallback does
 * not filter hidden/disabled state. Return no status; keep all callback
 * targets alive.
 */
static void SetActiveObject(struct MUI_WindowData *data, Object *obj,
    IPTR newval)
{
    LONG old_active = -1;

    ASSERT_VALID_PTR(data);
    ASSERT_VALID_PTR(obj);

    D(bug("MUIC_Window:SetActiveObject(data, obj, %08lx) Active=%p\n",
            newval, data->wd_ActiveObject));

    /* If the window is closed, we just store the object for later activation
     * when the window is (re)opened */
    if (!(data->wd_Flags & MUIWF_OPENED))
    {
        data->wd_OldActive = (Object *)newval;
        return;
    }

    if ((data->wd_ActiveObject != NULL)
        && (DoMethod(data->wd_RootObject, MUIM_FindAreaObject,
                (IPTR) data->wd_ActiveObject) != (IPTR) NULL))
    {
        if ((IPTR) data->wd_ActiveObject != newval)
        {
            old_active = ChainIndex(data, data->wd_ActiveObject);
            if ((data->wd_Flags & MUIWF_OBJECTGOACTIVESENT)
                && (_flags(data->wd_ActiveObject) & MADF_SETUP))
            {
                D(bug("Deactivate=%p\n", data->wd_ActiveObject));
                DoMethod(data->wd_ActiveObject, MUIM_GoInactive);
            }
        }
        else
            return; /* Don't activate an active object second time */
    }

    data->wd_ActiveObject = NULL;
    data->wd_Flags &= ~MUIWF_OBJECTGOACTIVESENT;

    switch (newval)
    {
    case MUIV_Window_ActiveObject_None:
        break;

    case MUIV_Window_ActiveObject_Next:
    case MUIV_Window_ActiveObject_Prev:
    {
        BOOL next = newval == MUIV_Window_ActiveObject_Next;

        if (old_active >= 0)
            data->wd_ActiveObject = ChainStep(data, old_active, next, TRUE);
        /* else the first or last of the chain, disabled or hidden or not */
        if (NULL == data->wd_ActiveObject)
            data->wd_ActiveObject = ChainStep(data,
                next ? -1 : data->wd_ChainCount, next, FALSE);
        break;
    }

    default:
        data->wd_ActiveObject = (Object *) newval;
        break;
    }


    if (data->wd_ActiveObject != NULL
        && DoMethod(data->wd_RootObject, MUIM_FindAreaObject,
            (IPTR) data->wd_ActiveObject)
        && (_flags(data->wd_ActiveObject) & MADF_CANDRAW))
    {
        D(bug("Activate=%p\n", data->wd_ActiveObject));
        DoMethod(data->wd_ActiveObject, MUIM_GoActive);
        data->wd_Flags |= MUIWF_OBJECTGOACTIVESENT;
    }
}


/* Return whether WORD x/y lie within a readable IBox using inclusive
 * left/top and exclusive right/bottom bounds. No allocation, mutation or
 * ownership transfer occurs.
 */
static BOOL InBox(struct IBox *box, WORD x, WORD y)
{
    return x >= box->Left && x < box->Left + box->Width
        && y >= box->Top && y < box->Top + box->Height;
}


/*
 * Pass on an AppMessage to all objects that it landed on.
 */
/* Set a borrowed live AppMessage on the root/current child, then recurse
 * into Group children whose boxes contain its coordinates. Require a non-
 * NULL Area-compatible child and stable hierarchy. Return no status;
 * neither copy nor reply the message, which remains owned by the
 * application input path.
 */
static void ForwardAppMessage(struct MUI_WindowData *data, Object *child,
    struct AppMessage *appmsg)
{
    WORD x = appmsg->am_MouseX, y = appmsg->am_MouseY;
    Object *cstate;
    struct List *children = NULL;

    ASSERT_VALID_PTR(data);
    ASSERT_VALID_PTR(child);

    set(child, MUIA_AppMessage, appmsg);

    children = (struct List *)XGET(child, MUIA_Group_ChildList);

    if (children != NULL)
    {
        cstate = (Object *) children->lh_Head;
        while ((child = NextObject(&cstate)))
        {
            if (InBox(&muiAreaData(child)->mad_Box, x, y))
            {
                ForwardAppMessage(data, child, appmsg);
            }
        }
    }
}


/* The right and bottom borders the window will get (see
 * DisplayWindow()): mri_BorderBottom is the size gadget's, but a window
 * without one, or of fixed size, gets the screen's plain border. */
static void WindowRealBorders(struct MUI_WindowData *data, LONG *rightp,
    LONG *bottomp)
{
    struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
    struct Screen *scr = mri->mri_Screen;
    ULONG flags = data->wd_CrtFlags;
    BOOL sizing;
    LONG right, bottom;

    sizing = (flags & WFLG_SIZEGADGET) && !(flags & WFLG_BORDERLESS)
        && !(data->wd_MinMax.MinWidth == data->wd_MinMax.MaxWidth
        && data->wd_MinMax.MinHeight == data->wd_MinMax.MaxHeight);
    right = mri->mri_BorderRight;
    bottom = (flags & WFLG_BORDERLESS) ? 0 : scr->WBorBottom;
    if (sizing && !(flags & WFLG_SIZEBRIGHT))
        bottom = mri->mri_BorderBottom;
    if (sizing && (flags & WFLG_SIZEBRIGHT) && mri->mri_SizeImage)
        right = MAX(right, IM(mri->mri_SizeImage)->Width);
    if ((data->wd_Flags & MUIWF_USERIGHTSCROLLER) && mri->mri_UpImage)
        right = MAX(right, IM(mri->mri_UpImage)->Width);
    if ((data->wd_Flags & MUIWF_USEBOTTOMSCROLLER) && mri->mri_LeftImage)
        bottom = MAX(bottom, IM(mri->mri_LeftImage)->Height);
    *rightp = right;
    *bottomp = bottom;
}

/*
 * calculate real dimensions from programmer requirements.
 * may be overridden by user settings if MUIA_Window_ID is set.
 */
/* One of the requested inner sizes (a = 0: the width, 1: the height; the
 * same rules and values for both: window.h): a pixel size or a screen
 * percentage less the borders, the default, a share between minimum and
 * maximum; without a rule that applies the size stays */
static __attribute__((noinline)) void SelectDimension(
    struct MUI_WindowData *data, int a, LONG borders)
{
    LONG *dim = a ? &data->wd_Height : &data->wd_Width;
    LONG req = a ? data->wd_ReqHeight : data->wd_ReqWidth;
    WORD *mm = &data->wd_MinMax.MinWidth + a;  /* min, max +2, def +4 */
    LONG size = a ? data->wd_RenderInfo.mri_Screen->Height
        : data->wd_RenderInfo.mri_Screen->Width;

    if (req > 0)
        *dim = req - borders;
    else if (req == MUIV_Window_Width_Default)
        *dim = mm[4];
    else if (_between(MUIV_Window_Width_MinMax(100), req,
            MUIV_Window_Width_MinMax(0)))
        *dim = mm[0] - (req * (mm[2] - mm[0])) / 100;
    else if (_between(MUIV_Window_Width_Screen(100), req,
            MUIV_Window_Width_Screen(0)))
        *dim = size * (-(req + 200)) / 100 - borders;
    else if (_between(MUIV_Window_Width_Visible(100), req,
            MUIV_Window_Width_Visible(0)))
        *dim = size * (-(req + 100)) / 100 - borders;
}

/* (no deferred pops: the three calls' arguments would pile up on the
 * frame) */
static __attribute__((optimize("no-defer-pop"))) void WindowSelectDimensions(
    struct MUI_WindowData *data)
{
    if (!data->wd_Width)
    {
        /* Pixel sizes and screen percentages give the outer window size
         * in MUI 3.8/3.9 (a 400 pixel MUIA_Window_Width opens a window 400
         * wide), while wd_Width/wd_Height are inner dimensions. */
        LONG right, bottom;

        WindowRealBorders(data, &right, &bottom);
        SelectDimension(data, 0, data->wd_RenderInfo.mri_BorderLeft + right);
        SelectDimension(data, 1, data->wd_RenderInfo.mri_BorderTop + bottom);

        /* scaled */
        if (data->wd_ReqWidth == MUIV_Window_Width_Scaled)
            data->wd_Width = data->wd_Height * data->wd_MinMax.MinWidth
                / data->wd_MinMax.MinHeight;
        else if (data->wd_ReqHeight == MUIV_Window_Width_Scaled)
            data->wd_Height = data->wd_Width * data->wd_MinMax.MinHeight
                / data->wd_MinMax.MinWidth;
    }
    data->wd_Width = CLAMP(data->wd_Width, data->wd_MinMax.MinWidth,
        data->wd_MinMax.MaxWidth);
    data->wd_Height = CLAMP(data->wd_Height, data->wd_MinMax.MinHeight,
        data->wd_MinMax.MaxHeight);
}


/* The window's flag attributes: which flag word (1: the creation flags,
 * 0: wd_Flags) and which bit, for OM_NEW (OM_SET has its own cases for
 * the last three; through a call they made its frame 4 bytes larger) */
static const struct WindowBoolTag { ULONG tag; ULONG bit; UWORD crt; } window_bool_tags[] =
{
    { MUIA_Window_CloseGadget, WFLG_CLOSEGADGET, 1 },
    { MUIA_Window_SizeGadget, WFLG_SIZEGADGET, 1 },
    { MUIA_Window_Backdrop, WFLG_BACKDROP, 1 },
    { MUIA_Window_Borderless, WFLG_BORDERLESS, 1 },
    { MUIA_Window_DepthGadget, WFLG_DEPTHGADGET, 1 },
    { MUIA_Window_DragBar, WFLG_DRAGBAR, 1 },
    { MUIA_Window_SizeRight, WFLG_SIZEBRIGHT, 1 },
    { MUIA_Window_EraseArea, MUIWF_ERASEAREA, 0 },
    { MUIA_Window_ToolBox, MUIWF_TOOLBOX, 0 },
    { MUIA_Window_AppWindow, MUIWF_ISAPPWINDOW, 0 },
    { MUIA_Window_IsSubWindow, MUIWF_ISSUBWINDOW, 0 },
    { MUIA_Window_UseBottomBorderScroller, MUIWF_USEBOTTOMSCROLLER, 0 },
    { MUIA_Window_UseRightBorderScroller, MUIWF_USERIGHTSCROLLER, 0 },
};

/* The flag and plain attributes, from OM_NEW's and OM_SET's default
 * label; first: the first flag entry the method takes (OM_NEW all,
 * OM_SET from IsSubWindow on: the others are for the creation only). At
 * OM_NEW the instance data is zero, so the size's "forget the current
 * one" changes nothing. */
#define WINDOW_SET_FLAGS 10     /* window_bool_tags' IsSubWindow */
static __attribute__((noinline)) void Window_SetPlain(
    struct MUI_WindowData *data, struct TagItem *tag, int first)
{
    IPTR v = tag->ti_Data;
    const struct WindowBoolTag *b = &window_bool_tags[first];

    do
    {
        if (b->tag == tag->ti_Tag)
        {
            ULONG *flags = b->crt ? &data->wd_CrtFlags : &data->wd_Flags;
            _handle_bool_tag(*flags, v, b->bit);
            return;
        }
    }
    while (++b < &window_bool_tags[sizeof(window_bool_tags)
        / sizeof(window_bool_tags[0])]);

    switch (tag->ti_Tag)
    {
    case MUIA_Window_DisableKeys:
        data->wd_DisabledKeys = v;
        break;

    case MUIA_Window_RefWindow:
        data->wd_RefWindow = (Object *) v;
        break;

    case MUIA_Window_LeftEdge:
        data->wd_ReqX = v;
        break;

    case MUIA_Window_TopEdge:
        data->wd_ReqY = v;
        break;

    case MUIA_Window_Width:
        data->wd_ReqWidth = (LONG) v;
        data->wd_Width = 0; /* otherwise windowselectdimensions()
                             * ignores ReqWidth */
        break;

    case MUIA_Window_Height:
        data->wd_ReqHeight = (LONG) v;
        data->wd_Height = 0;
        break;

    case MUIA_Window_Screen:
        data->wd_UserScreen = (struct Screen *) v;
        break;

    case MUIA_Window_PublicScreen:
        data->wd_UserPublicScreen = (STRPTR) v;
        break;
    }
}

/* Construct Notify, create the private node pool, initialize lists/defaults
 * and process supported tags in order. Return the object or NULL after
 * superclass/pool failure or explicit NULL RootObject. Adopt the current
 * root/local menustrip; borrow strings, screen and reference pointers.
 * Omitted root is allowed. Do not open the native window here.
 */
IPTR Window__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg);

/* An OM_NEW tag set through the window's own OM_SET, not through set():
 * set() dispatches to the subclass, whose OM_SET runs before its own
 * OM_NEW has finished. BWin.mcc (Alfonso Ranieri's borderless window)
 * catches MUIA_Window_RootObject in its OM_SET and never passed it on, so
 * its window had no root object and never opened; on MUI 3.8 and MUI 5 it
 * opens (the mcc group of the real-program suite, 2026-11-11). */
static __attribute__((noinline)) void OwnSet(struct IClass *cl, Object *obj, IPTR attr, IPTR val)
{
    struct TagItem tags[2] = { { attr, val }, { TAG_DONE, 0 } };
    struct opSet msg = { OM_SET, tags, NULL };

    Window__OM_SET(cl, obj, &msg);
}

IPTR Window__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_WindowData *data;
    struct TagItem *tags;
    struct TagItem *tag;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
    {
        zune_dispose_children(msg->ops_AttrList);
        return FALSE;
    }

    /* Initial local instance data */
    data = INST_DATA(cl, obj);

    /* (the instance data is 0: only what is not is set) */
    data->wd_AreaRenderInfo = window_area_renderinfo;
    data->wd_Opacity = 255;
    data->wd_Class = cl;
    data->wd_RenderInfo.mri_WindowObject = obj;

    NewList((struct List *)&(data->wd_EHList));
    NewList((struct List *)&(data->wd_CCList));

    data->wd_CrtFlags = WFLG_SIZEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET
        | WFLG_CLOSEGADGET | WFLG_SIMPLE_REFRESH
        | WFLG_REPORTMOUSE | WFLG_NEWLOOKMENUS;
    data->wd_ZoomGadget = TRUE;
    data->wd_Events = GetDefaultEvents();
    data->wd_ReqHeight = MUIV_Window_Height_Default;
    data->wd_ReqWidth = MUIV_Window_Width_Default;

    data->wd_AltDim.Top = MUIV_Window_AltTopEdge_NoChange;
    data->wd_AltDim.Left = MUIV_Window_AltLeftEdge_NoChange;
    data->wd_AltDim.Width = MUIV_Window_AltWidth_MinMax(0);
    data->wd_AltDim.Height = MUIV_Window_AltHeight_MinMax(0);
    data->wd_ReqX = MUIV_Window_LeftEdge_Centered;
    data->wd_ReqY = MUIV_Window_TopEdge_Centered;
    data->wd_HelpTicker = BUBBLEHELP_TICKER_DEFAULT;

    /* parse initial taglist */

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_Window_Opacity:
            data->wd_Opacity = MIN(tag->ti_Data, 255);
            break;
        case MUIA_Window_ZoomGadget:
            data->wd_ZoomGadget = tag->ti_Data;
            break;

        case MUIA_Window_RootObject:
            if (!tag->ti_Data)
            {
                CoerceMethod(cl, obj, OM_DISPOSE);
                return 0;
            }
            /* fall through */
        case MUIA_Window_ID:
        case MUIA_Window_Title:
        case MUIA_Window_ScreenTitle:
        case MUIA_Window_DefaultObject:
            OwnSet(cl, obj, tag->ti_Tag, tag->ti_Data);
            break;

        case MUIA_Window_Activate:
            _handle_bool_tag(data->wd_Flags, !tag->ti_Data,
                MUIWF_DONTACTIVATE);
            break;

        case MUIA_Window_Menustrip:
            data->wd_ChildMenustrip = (Object *) tag->ti_Data;
            break;

        case MUIA_Window_NoMenus:
            data->wd_NoMenus = (BOOL) tag->ti_Data;
            break;

        /* the zoomed box (WORDs) */
        case MUIA_Window_AltLeftEdge:
            data->wd_AltDim.Left = (WORD) tag->ti_Data;
            break;
        case MUIA_Window_AltTopEdge:
            data->wd_AltDim.Top = (WORD) tag->ti_Data;
            break;
        case MUIA_Window_AltWidth:
            data->wd_AltDim.Width = (WORD) tag->ti_Data;
            break;
        case MUIA_Window_AltHeight:
            data->wd_AltDim.Height = (WORD) tag->ti_Data;
            break;

        default:
            Window_SetPlain(data, tag, 0);
            break;

        }
    }

/*      D(bug("muimaster.library/window.c: Window Object created at " */
/*          "0x%p back=%lx\n", */
/*          obj,data->wd_Background)); */

    return (IPTR) obj;
}

/* Dispose the current owned root and local menustrip, delete the private
 * node pool and return superclass disposal result. The caller must already
 * have closed the native window, detached application membership and
 * stopped callbacks. This handler intentionally does not perform close or
 * remove membership.
 */
/* The explicit cycle chain, the chain, the gadget IDs and the render
 * info's tail, in one call that keeps OM_DISPOSE's frame as small as one
 * FreeVec did */
static void __attribute__((noinline)) WindowFreeBlocks(struct MUI_WindowData *data)
{
    FreeVec(data->wd_CycleOrder);
    FreeVec(data->wd_Chain);
    FreeVec(data->wd_IDMap);
    FreeVec(data->wd_RenderInfo.mri_Tail);
}

IPTR Window__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    Object *root = data->wd_RootObject;
    struct WindowEventCursor *cursor;

    /* A handler may dispose its Window. Invalidate every stack cursor
     * before teardown so neither nested dispatch nor its caller accesses
     * this instance or its former handler list after the callback returns.
     */
    while ((cursor = data->wd_EventCursors))
    {
        data->wd_EventCursors = cursor->previous;
        cursor->next = NULL;
    }

    /* Close directly: OM_DISPOSE must finish even if a subclass filters
       MUIA_Window_Open. Closing detaches IDCMP before objects are freed. */
    if (data->wd_Flags & MUIWF_OPENED)
        WindowClose(cl, obj);
    if (root) {
        if (_flags(root) & MADF_CANDRAW) DoHideMethod(root);
        if (_flags(root) & MADF_SETUP) DoMethod(root, MUIM_Cleanup);
        MUI_DisposeObject(root);
        data->wd_RootObject = NULL;
    }
    if (data->wd_ChildMenustrip)
        MUI_DisposeObject(data->wd_ChildMenustrip);
    WindowFreeBlocks(data);
    return DoSuperMethodA(cl, obj, msg);
}

/* Require a root, run Window setup then root setup, compute dimensions,
 * choose local/application menu and open the native window. On checked
 * failure unwind completed setup and return FALSE. On success mark open,
 * show/redraw and restore remembered focus, then return TRUE. Requires
 * connected application state; optional resource failures may remain
 * unchecked.
 */
static ULONG WindowOpen(struct IClass *cl, Object *obj);

/* Apply supported tags in order, including open/close, focus, borrowed
 * selectors, sleeping and private iconification visibility; forward the
 * same list to Notify and return its result. Local operation failures are
 * not collected into a success result. Require valid application/root state
 * for operations that use it; pointer lifetimes follow attribute contracts.
 */
IPTR Window__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList;
    struct TagItem *tag;

    while ((tag = NextTagItem(&tags)) != NULL)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_AppMessage:
            ForwardAppMessage(data, data->wd_RootObject,
                (struct AppMessage *)tag->ti_Data);
            break;

        case MUIA_Window_Activate:
            if (data->wd_RenderInfo.mri_Window)
            {
                /* MUIWF_ACTIVE lags behind Intuition: a popup opened just
                 * before may have taken the activation already. Only the
                 * report of an IDCMP_ACTIVEWINDOW must not activate, as
                 * another window may be active by now. */
                if (tag->ti_Data && !(data->wd_Flags & MUIWF_ACTIVEEVENT))
                {
                    ActivateWindow(data->wd_RenderInfo.mri_Window);
                    _handle_bool_tag(data->wd_Flags, tag->ti_Data,
                        MUIWF_ACTIVE);
                }
            }
            else
                _handle_bool_tag(data->wd_Flags, !tag->ti_Data,
                    MUIWF_DONTACTIVATE);
            break;

        case MUIA_Window_ActiveObject:
/*              D(bug("MUIA_Window_ActiveObject %ld (%p)\n", */
/*                  tag->ti_Data, tag->ti_Data)); */
            SetActiveObject(data, obj, tag->ti_Data);
            break;

        case MUIA_Window_DefaultObject:
            data->wd_DefaultObject = (APTR) tag->ti_Data;
            break;

        case MUIA_Window_ID:
            data->wd_ID = tag->ti_Data;
            break;

        case MUIA_Window_Open:
            if (tag->ti_Data)
            {
                if (data->wd_Flags & MUIWF_HIDDEN)
                    data->wd_Flags |= MUIWF_OPENONUNHIDE;
                else if (!(data->wd_Flags & MUIWF_OPENED))
                    WindowOpen(cl, obj);
                else
                {
                    DoMethod(obj, MUIM_Window_ToFront);
                    set(obj, MUIA_Window_Activate, TRUE);
                }
            }
            else if (data->wd_Flags & MUIWF_HIDDEN)
                data->wd_Flags &= ~MUIWF_OPENONUNHIDE;
            else if (data->wd_Flags & MUIWF_OPENED)
                WindowClose(cl, obj);
            break;

        case MUIA_ShowMe:      /* PRIVATE *abuse* of the Area's ShowMe attr */
            if (tag->ti_Data)
            {
                /* Deiconify */

                if (data->wd_Flags & MUIWF_HIDDEN)
                {
                    data->wd_Flags &= ~MUIWF_HIDDEN;

                    if (data->wd_Flags & MUIWF_OPENONUNHIDE)
                    {
                        data->wd_Flags &= ~MUIWF_OPENONUNHIDE;
                        set(obj, MUIA_Window_Open, TRUE);
                    }
                }
            }
            else
            {
                /* Iconify */

                if (data->wd_Flags & MUIWF_OPENED)
                {
                    data->wd_Flags |= MUIWF_OPENONUNHIDE;

                    set(obj, MUIA_Window_Open, FALSE);
                }

                data->wd_Flags |= MUIWF_HIDDEN;
            }
            break;

        case MUIA_Window_RootObject:
            ChangeRootObject(data, obj, (Object *) tag->ti_Data);
            break;

        case MUIA_Window_Title:
            data->wd_Title = (STRPTR) tag->ti_Data;
            if (data->wd_RenderInfo.mri_Window)
                SetWindowTitles(data->wd_RenderInfo.mri_Window,
                    data->wd_Title, (CONST_STRPTR) ~ 0);
            break;

        case MUIA_Window_ScreenTitle:
            data->wd_ScreenTitle = (STRPTR) tag->ti_Data;
            if (data->wd_RenderInfo.mri_Window)
                SetWindowTitles(data->wd_RenderInfo.mri_Window,
                    (CONST_STRPTR) ~ 0, data->wd_ScreenTitle);
            break;

        case MUIA_Window_NoMenus:
            data->wd_NoMenus = (BOOL) tag->ti_Data;
            if (data->wd_RenderInfo.mri_Window)
            {
                if (data->wd_NoMenus || !data->wd_Menu)
                    AROS_ATOMIC_OR(data->wd_RenderInfo.mri_Window->Flags, WFLG_RMBTRAP);
                else
                    AROS_ATOMIC_AND(data->wd_RenderInfo.mri_Window->Flags, ~WFLG_RMBTRAP);
            }
            break;

        case MUIA_Window_Opacity:
            /* OS3 has no window compositor; retain the public setting. */
            data->wd_Opacity = MIN(tag->ti_Data, 255);
            break;
        case MUIA_Window_Sleep:
            if (tag->ti_Data)
            {
                if (!(data->wd_Flags & MUIWF_OPENED))
                    break;

                data->wd_SleepCount++;
                if (data->wd_RenderInfo.mri_Window
                    && (data->wd_SleepCount == 1))
                {
                    SetWindowPointer
                        (data->wd_RenderInfo.mri_Window,
                        WA_BusyPointer, TRUE,
                        WA_PointerDelay, TRUE, TAG_DONE);

                    /* event handling is disabled in _zune_window_message() */

                    data->wd_SleepMaxHeight=data->wd_RenderInfo.mri_Window->MaxHeight;
                    data->wd_SleepMinHeight=data->wd_RenderInfo.mri_Window->MinHeight;
                    data->wd_SleepMaxWidth=data->wd_RenderInfo.mri_Window->MaxWidth;
                    data->wd_SleepMinWidth=data->wd_RenderInfo.mri_Window->MinWidth;
                    /* According to MUI autodocs, sleeping windows can't be resized.
                     * MUI 3.8/AmigaOS also changes min/max values with WindowLimits */
                    WindowLimits(data->wd_RenderInfo.mri_Window,
                        data->wd_RenderInfo.mri_Window->Width,
                        data->wd_RenderInfo.mri_Window->Height,
                        data->wd_RenderInfo.mri_Window->Width,
                        data->wd_RenderInfo.mri_Window->Height);
                }
            }
            else
            {
                if (data->wd_SleepCount == 0)
                    break;

                data->wd_SleepCount--;
                if (data->wd_RenderInfo.mri_Window
                    && (data->wd_SleepCount == 0))
                {
                    SetWindowPointerA(data->wd_RenderInfo.mri_Window, NULL);
                    data->wd_PointerType = MUIV_PointerType_Normal;

                    /* Only restore settings, if they have been saved
                     * during (MUIA_Window_Sleep, TRUE) call */
                    if (data->wd_SleepMaxHeight > 0)
                    {
                        WindowLimits(data->wd_RenderInfo.mri_Window,
                            data->wd_SleepMinWidth,
                            data->wd_SleepMinHeight,
                            data->wd_SleepMaxWidth,
                            data->wd_SleepMaxHeight);
                        data->wd_SleepMinHeight=0;
                        data->wd_SleepMaxHeight=0;
                        data->wd_SleepMinWidth=0;
                        data->wd_SleepMaxWidth=0;
                    }
                }
            }
            break;

        default:
            Window_SetPlain(data, tag, WINDOW_SET_FLAGS);
            break;
        }
    }

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/* Initialize writable opg_Storage to zero, then return recognized stored or
 * native-window fields, or delegate unknown attributes to Notify. Native
 * geometry is zero while closed; pointer results are borrowed. Some
 * declared header getters have no case here and do not become implemented
 * merely through zero initialization.
 */
IPTR Window__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    UBYTE *w;
    IPTR v;

    /* (data in an address register: GCC otherwise keeps the class's
     * instance offset in a data register for (d8,An,Xn) addressing, a
     * frame 4 bytes larger) */
    __asm__("" : "+a"(data));
    /* Nothing is stored for an attribute this class does not know: a
     * subclass that stores its own value and then calls the superclass
     * (tipwindow.mcc's MUIA_Tip_WindowObject) keeps it, as on MUI 3.8 */
    switch (msg->opg_AttrID)
    {
    case MUIA_WindowObject:
        v = (IPTR)obj;
        break;
    case MUIA_Window_Menustrip:
        v = (IPTR)(data->wd_ChildMenustrip ? data->wd_ChildMenustrip
            : data->wd_Menustrip);
        break;
    case MUIA_Window_Activate:
        v = (data->wd_Flags & (MUIWF_ACTIVE | MUIWF_OPENED))
            == (MUIWF_ACTIVE | MUIWF_OPENED);
        break;
    case MUIA_Window_Window:
        v = (IPTR)data->wd_RenderInfo.mri_Window;
        break;
    case MUIA_Window_Screen:
        v = (IPTR)data->wd_RenderInfo.mri_Screen;
        break;
    case MUIA_Window_PublicScreen:
        v = (IPTR)data->wd_UserPublicScreen;
        break;
    case MUIA_Window_ActiveObject:
        v = 0;
        if (data->wd_ActiveObject && DoMethod(data->wd_RootObject,
                MUIM_FindAreaObject, (IPTR)data->wd_ActiveObject))
            v = (IPTR)data->wd_ActiveObject;
        break;
    case MUIA_Window_CloseRequest:
        v = FALSE;
        break;
    case MUIA_Window_DefaultObject:
        v = (IPTR)data->wd_DefaultObject;
        break;
    case MUIA_Window_DisableKeys:
        v = data->wd_DisabledKeys;
        break;
    case MUIA_Window_ID:
        v = data->wd_ID;
        break;
    case MUIA_Window_IsSubWindow:
        v = (data->wd_Flags & MUIWF_ISSUBWINDOW) != 0;
        break;
    case MUIA_Window_Open:
        v = (data->wd_Flags & MUIWF_OPENED) != 0;
        break;
    case MUIA_Window_RootObject:
        v = (IPTR)data->wd_RootObject;
        if (data->wd_Flags & MUIWF_SCROLLWRAP)
            v = (IPTR)zune_window_scrollwrap((Object *)v, ZUNE_SW_PEEK);
        break;
    case MUIA_Window_ScreenTitle:
        v = (IPTR)data->wd_ScreenTitle;
        break;
    case MUIA_Window_Title:
        v = (IPTR)data->wd_Title;
        break;
    case MUIA_Window_Opacity:
        v = data->wd_Opacity;
        break;
    case MUIA_Window_Sleep:
        v = data->wd_SleepCount != 0;
        break;
    case MUIA_Version:
        v = ZUNE68_BUILTIN_VERSION;
        break;
    case MUIA_Revision:
        v = ZUNE68_BUILTIN_REVISION;
        break;

    /* The native window's box, 0 while closed: the whole window, borders
     * included, as MUI 3.8 answers (and as the pixel value given at
     * OM_NEW is taken) */
    case MUIA_Window_LeftEdge:
        v = offsetof(struct Window, LeftEdge);
        goto native;
    case MUIA_Window_TopEdge:
        v = offsetof(struct Window, TopEdge);
        goto native;
    case MUIA_Window_Width:
        v = offsetof(struct Window, Width);
        goto native;
    case MUIA_Window_Height:
        v = offsetof(struct Window, Height);
    native:
        w = (UBYTE *)data->wd_RenderInfo.mri_Window;
        v = w ? (LONG)*(WORD *)(w + v) : 0;
        break;

    /* the zoomed box as given */
    case MUIA_Window_AltLeftEdge:
        v = offsetof(struct IBox, Left);
        goto alt;
    case MUIA_Window_AltTopEdge:
        v = offsetof(struct IBox, Top);
        goto alt;
    case MUIA_Window_AltWidth:
        v = offsetof(struct IBox, Width);
        goto alt;
    case MUIA_Window_AltHeight:
        v = offsetof(struct IBox, Height);
    alt:
        v = (LONG)*(WORD *)((UBYTE *)&data->wd_AltDim + v);
        break;

    default:
        return DoSuperMethodA(cl, obj, (Msg) msg);
    }
    *msg->opg_Storage = v;
    return TRUE;
}

/* Match window UserData first, then search its root subtree if present.
 * Return a borrowed matching object or zero. Do not search the menustrip;
 * keep hierarchy alive through delegated calls.
 */
IPTR Window__MUIM_FindUData(struct IClass *cl, Object *obj,
    struct MUIP_FindUData *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    if (ZUNE_FIND_MATCH(obj, msg))
        return (IPTR) obj;

    if (data->wd_RootObject)
        return DoMethodA(data->wd_RootObject, (Msg) msg);

    return 0;
}


/* On a window UserData match, get the requested attribute into writable
 * storage and return TRUE without checking getter success; otherwise
 * delegate to the root if present. Return FALSE without a match. No
 * menustrip traversal or ownership transfer occurs.
 */
IPTR Window__MUIM_GetUData(struct IClass *cl, Object *obj,
    struct MUIP_GetUData *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
    {
        get(obj, msg->attr, msg->storage);
        return TRUE;
    }

    if (data->wd_RootObject)
        return DoMethodA(data->wd_RootObject, (Msg) msg);

    return FALSE;
}


/* Set the window attribute on a UserData match, then visit the root if
 * present. Return TRUE regardless of matches or setter results. Keep the
 * root/window alive and traversal stable through synchronous notifications.
 */
IPTR Window__MUIM_SetUData(struct IClass *cl, Object *obj,
    struct MUIP_SetUData *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
        set(obj, msg->attr, msg->val);

    if (data->wd_RootObject)
        DoMethodA(data->wd_RootObject, (Msg) msg);

    return TRUE;
}


/* Set the window on a UserData match and return TRUE without checking
 * setter success; otherwise delegate to the root or return FALSE. Stop at
 * the first delegated match and do not search menus.
 */
IPTR Window__MUIM_SetUDataOnce(struct IClass *cl, Object *obj,
    struct MUIP_SetUDataOnce *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
    {
        set(obj, msg->attr, msg->val);
        return TRUE;
    }

    if (data->wd_RootObject)
        return DoMethodA(data->wd_RootObject, (Msg) msg);

    return FALSE;
}

/**************************************************************************
 Called by Application (parent) object whenever this object is added.
 init GlobalInfo
**************************************************************************/
/* Delegate connection to Notify and, if successful, connect current root
 * and local menustrip to this Window. Return TRUE on that path, FALSE on
 * superclass failure. Borrow a live application-parent message; child
 * results are ignored and no membership insertion is performed here.
 */
IPTR Window__MUIM_ConnectParent(struct IClass *cl, Object *obj,
    struct MUIP_ConnectParent *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    if (!DoSuperMethodA(cl, obj, (Msg) msg))
        return 0;

    if (data->wd_RootObject)
        DoMethod(data->wd_RootObject, MUIM_ConnectParent, (IPTR) obj);

    if (data->wd_ChildMenustrip)
        DoMethod(data->wd_ChildMenustrip, MUIM_ConnectParent, (IPTR) obj);

    return TRUE;
}


/**************************************************************************
 called by parent object
**************************************************************************/
/* If global information exists, close an open window, then disconnect local
 * menustrip and root before delegating to Notify. Return zero. Requires
 * live application state during closing; global info is not cleared by
 * Notify and is not a lifetime guarantee.
 */
IPTR Window__MUIM_DisconnectParent(struct IClass *cl, Object *obj,
    struct MUIP_DisconnectParent *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

/*      D(bug("Window_DisconnectParent(%p) : muiGlobalInfo=%p\n", */
/*          muiGlobalInfo(obj))); */
    if (muiGlobalInfo(obj))
    {
        /* Close the window before disconnecting all the childs */
        if ((data->wd_Flags & MUIWF_OPENED))
        {
/*            D(bug(" Window_DisconnectParent(%p) : closing window\n", */
/*                muiGlobalInfo(obj))); */
            set(obj, MUIA_Window_Open, FALSE);
        }
        if (data->wd_ChildMenustrip)
            DoMethod(data->wd_ChildMenustrip, MUIM_DisconnectParent,
                (IPTR) obj);

        if (data->wd_RootObject)
            DoMethodA(data->wd_RootObject, (Msg) msg);

/*        D(bug(" Window_DisconnectParent(%p) : calling supermethod\n", */
/*            muiGlobalInfo(obj))); */
        return DoSuperMethodA(cl, obj, (Msg) msg);
    }
    else
    {
        return 0;
    }
}

/* Set each root inner-spacing component from window preferences unless
 * explicitly specified by the Area object; use zero for borderless windows.
 * Require a live root and connected preferences. Mutate root layout inputs,
 * return no status and retain no pointers.
 */
static void SetRootObjInnerSpacing(Object *obj,
    struct MUI_WindowData *data)
{
    UWORD wd_innerLeft, wd_innerRight, wd_innerTop, wd_innerBottom;

    if (data->wd_CrtFlags & WFLG_BORDERLESS)
    {
        wd_innerLeft = 0;
        wd_innerRight = 0;
        wd_innerTop = 0;
        wd_innerBottom = 0;
    }
    else
    {
        wd_innerLeft = muiGlobalInfo(obj)->mgi_Prefs->window_inner_left;
        wd_innerRight = muiGlobalInfo(obj)->mgi_Prefs->window_inner_right;
        wd_innerTop = muiGlobalInfo(obj)->mgi_Prefs->window_inner_top;
        wd_innerBottom = muiGlobalInfo(obj)->mgi_Prefs->window_inner_bottom;
        /* MUI 3.8 keeps 2 pixels of window spacing left and right and 1
         * above and below from the third stage of fitting a window into
         * the screen */
        if (data->wd_RenderInfo.mri_Fallback >= FALLBACK_SPACING)
        {
            wd_innerLeft = MIN(wd_innerLeft, 2);
            wd_innerRight = MIN(wd_innerRight, 2);
            wd_innerTop = MIN(wd_innerTop, 1);
            wd_innerBottom = MIN(wd_innerBottom, 1);
        }
    }

    if (!(muiAreaData(data->wd_RootObject)->mad_Flags & MADF_INNERLEFT))
    {
        muiAreaData(data->wd_RootObject)->mad_InnerLeft = wd_innerLeft;
    }

    if (!(muiAreaData(data->wd_RootObject)->mad_Flags & MADF_INNERTOP))
    {
        muiAreaData(data->wd_RootObject)->mad_InnerTop = wd_innerTop;
    }

    if (!(muiAreaData(data->wd_RootObject)->mad_Flags & MADF_INNERRIGHT))
    {
        muiAreaData(data->wd_RootObject)->mad_InnerRight = wd_innerRight;
    }

    if (!(muiAreaData(data->wd_RootObject)->mad_Flags & MADF_INNERBOTTOM))
    {
        muiAreaData(data->wd_RootObject)->mad_InnerBottom = wd_innerBottom;
    }
}

/*
 * Called before window is opened or resized. It determines its bounds,
 * so you can call WindowSelectDimensions() to find the final dims.
 */
/* Set root spacing, ask and finalize its min/max dimensions, then cap
 * maxima to screen dimensions. Require live root and render screen. Store
 * the result in window data and return no status; this does not open or
 * resize Intuition.
 */
/* The inner maximum leaves room for the window borders on screen: the
 * real ones, a size gadget in the right border leaves a thin bottom
 * border (AmTelnet opens 256 high on MUI 3.8, was 248). Not inlined:
 * WindowMinMax's frame is on the stack through the whole AskMinMax. */
static __attribute__((noinline)) void WindowScreenMax(
    struct MUI_WindowData *data)
{
    struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
    LONG right, bottom, maxw, maxh;

    WindowRealBorders(data, &right, &bottom);
    maxw = mri->mri_Screen->Width - mri->mri_BorderLeft - right;
    maxh = mri->mri_Screen->Height - mri->mri_BorderTop - bottom;

    if (data->wd_MinMax.MaxWidth > maxw)
        data->wd_MinMax.MaxWidth = MAX(maxw, data->wd_MinMax.MinWidth);
    if (data->wd_MinMax.MaxHeight > maxh)
        data->wd_MinMax.MaxHeight = MAX(maxh, data->wd_MinMax.MinHeight);
}

/* MUI 3.8 has every preset's font open while a window is set up (its
 * render info holds them all); MUI 3 programs read them there (IBrowse
 * 2.5.9's toolbar). Zune opens a preset's font when an object first uses
 * it: for MUI 3 programs, open them all before the sizes are asked. */
static __attribute__((noinline)) void OpenAllPresetFonts(Object *root)
{
    struct MUI_RenderInfo *mri = muiRenderInfo(root);
    LONG i;

    if (!ZUNE_APP_MUI38(root))
        return;
    for (i = -1; i > (LONG)MUIV_Font_NegCount; i--)
    {
        struct TextFont *f = zune_font_get(root, (IPTR)i);

        /* a preset without a font of its own gets the normal font
         * uncached: keep a reference of its own in its slot, closed
         * with the others at cleanup */
        if (f && !mri->mri_Fonts[-i])
        {
            struct TextAttr ta;

            ta.ta_Name = f->tf_Message.mn_Node.ln_Name;
            ta.ta_YSize = f->tf_YSize;
            ta.ta_Style = f->tf_Style;
            ta.ta_Flags = f->tf_Flags;
            mri->mri_Fonts[-i] = OpenFont(&ta);
        }
    }
}

static void WindowMinMax(Object *obj, struct MUI_WindowData *data)
{
    SetRootObjInnerSpacing(obj, data);
    /* inquire about sizes; like MUI, Area stores them in the root's own
     * minmax */
    DoMethod(data->wd_RootObject, MUIM_AskMinMax, (IPTR) & data->wd_MinMax);
    {
        const struct MUI_MinMax *mm =
            &muiAreaData(data->wd_RootObject)->mad_MinMax;

        data->wd_MinMax.MinWidth = mm->MinWidth;
        data->wd_MinMax.MinHeight = mm->MinHeight;
        data->wd_MinMax.MaxWidth = mm->MaxWidth;
        data->wd_MinMax.MaxHeight = mm->MaxHeight;
        data->wd_MinMax.DefWidth = mm->DefWidth;
        data->wd_MinMax.DefHeight = mm->DefHeight;
    }
/*      D(bug("*** root minmax = %ld,%ld => %ld,%ld\n", */
/*            data->wd_MinMax.MinWidth, */
/*            data->wd_MinMax.MinHeight, */
/*            data->wd_MinMax.MaxWidth, data->wd_MinMax.MaxHeight)); */
    __area_finish_minmax(data->wd_RootObject, &data->wd_MinMax);
/*      D(bug("*** root minmax2 = %ld,%ld => %ld,%ld\n", */
/*            data->wd_MinMax.MinWidth, */
/*            data->wd_MinMax.MinHeight, */
/*            data->wd_MinMax.MaxWidth, data->wd_MinMax.MaxHeight)); */
    WindowScreenMax(data);
}


/* Initialize a buffer RastPort and assign its bitmap only if a buffer
 * bitmap already exists. Bitmap allocation code is disabled. Borrow
 * instance state and return no status; this helper does not currently
 * enable buffering by itself.
 */
static void InstallBackbuffer(struct IClass *cl, Object *obj)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
#if 0
    struct Window *win = data->wd_RenderInfo.mri_Window;

    data->wd_RenderInfo.mri_BufferBM =
        AllocBitMap(win->Width, win->Height, win->RPort->BitMap->Depth,
        0, win->RPort->BitMap);
#endif
    if (data->wd_RenderInfo.mri_BufferBM)
    {
/*        D(bug("install_backbuffer : allocated bitmap %dx%dx%d " */
/*            "with friend %p\n", */
/*            win->Width, win->Height, win->RPort->BitMap->Depth, */
/*            win->RPort->BitMap)); */
        InitRastPort(&data->wd_RenderInfo.mri_Tail->BufferRP);
        data->wd_RenderInfo.mri_Tail->BufferRP.BitMap =
            data->wd_RenderInfo.mri_BufferBM;
    }
}

/* (DeinstallBackbuffer is gone: no buffer bitmap is ever allocated, see
 * InstallBackbuffer above, so there was never one to free at closing) */
/*
 * Called after window is opened or resized.
 * An expose event is already queued, it will trigger
 * MUIM_Draw for us when going back to main loop.
 */
/* Use the live native window borders and cached inner dimensions to lay out
 * the root, select the drawing port, show the background and show the root.
 * Requires successful setup and opening. Return no status; callbacks must
 * not invalidate the window during this sequence.
 */
static void WindowShow(struct IClass *cl, Object *obj)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    struct Window *win = data->wd_RenderInfo.mri_Window;
/*      D(bug("WindowShow %s %d\n", __FILE__, __LINE__)); */

    _left(data->wd_RootObject) = win->BorderLeft;
    _top(data->wd_RootObject) = win->BorderTop;
    muiAreaData(data->wd_RootObject)->mad_VirtualTop = win->BorderTop;
    _width(data->wd_RootObject) = data->wd_Width;
    _height(data->wd_RootObject) = data->wd_Height;

    DoLayoutMethod(data->wd_RootObject);

    ShowRenderInfo(&data->wd_RenderInfo);
/*      D(bug("zune_imspec_show %s %d\n", __FILE__, __LINE__)); */
    zune_imspec_show(data->wd_Background, obj);
    DoShowMethod(data->wd_RootObject);
}

/* Base-owned setup resources must also unwind when a subclass rejects
 * Setup after calling its superclass, or filters Cleanup entirely.
 */
static __attribute__((noinline)) void CleanupWindowSetup(struct MUI_WindowData *data)
{
    struct MUI_ImageSpec_intern *background = data->wd_Background;

    data->wd_Background = NULL;
    zune_imspec_cleanup(background);

    if (data->wd_dnd)
    {
        struct DragNDrop *dnd = data->wd_dnd;
        data->wd_dnd = NULL;
        DeleteDragNDrop(dnd);
    }
}

/* Undo the setup of an opening: the root's (level 2), the window's
 * (MUIM_Window_Cleanup, level 1 and 2), then the render info's */
static __attribute__((noinline)) void WindowUnsetup(Object *obj,
    struct MUI_WindowData *data, LONG level)
{
    if (level > 1)
        DoMethod(data->wd_RootObject, MUIM_Cleanup);
    if (level > 0)
        DoMethod(obj, MUIM_Window_Cleanup);
    CleanupWindowSetup(data);
    CleanupRenderInfo(obj, data, &data->wd_RenderInfo);
}

/* Does the window at its minimum size fit the screen? */
static BOOL WindowFits(struct MUI_WindowData *data)
{
    struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
    struct Screen *scr = mri->mri_Screen;
    LONG right, bottom;

    if (!scr || scr->Width <= 0 || scr->Height <= 0)
        return TRUE;
    WindowRealBorders(data, &right, &bottom);
    return data->wd_MinMax.MinWidth + mri->mri_BorderLeft + right
        <= scr->Width
        && data->wd_MinMax.MinHeight + mri->mri_BorderTop + bottom
        <= scr->Height;
}

/* Require a root, run Window setup then root setup, compute dimensions,
 * choose local/application menu and open the native window. On checked
 * failure unwind completed setup and return FALSE. On success mark open,
 * show/redraw and restore remembered focus, then return TRUE. Requires
 * connected application state; optional resource failures may remain
 * unchecked.
 */
static __attribute__((noinline)) ULONG WindowOpen(struct IClass *cl, Object *obj)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    if (!data->wd_RootObject)
        return FALSE;

    /* The screen belongs to the whole subclass setup/cleanup chain. */
    if (!SetupRenderInfo(obj, data, &data->wd_RenderInfo))
        return FALSE;

    if (!DoMethod(obj, MUIM_Window_Setup))
    {
        WindowUnsetup(obj, data, 0);
        return FALSE;
    }

    /* I got display info, so calculate your display dependant data */
    if (!DoSetupMethod(data->wd_RootObject, &data->wd_RenderInfo))
    {
        WindowUnsetup(obj, data, 1);
        return FALSE;
    }

    /* inquire about sizes */
    OpenAllPresetFonts(data->wd_RootObject);
    WindowMinMax(obj, data);

    /* Like MUI 3.8, set a window larger than the screen up again with
     * smaller fonts and spacing, stage by stage (helvetica 11; helvetica
     * 9; less spacing; no inner frame spacing; topaz 8: FALLBACK_ in
     * muimaster_intern.h), and
     * do not open it if it still does not fit */
    while (!WindowFits(data))
    {
        struct MUI_RenderInfo *mri = &data->wd_RenderInfo;
        Object *sg = NULL;

        DoMethod(data->wd_RootObject, MUIM_Cleanup);
        CloseRenderInfoFonts(mri);
        /* MUI 5 (its ChangeLog, 5.0-2018R1) first puts the contents into a
         * scrollable virtual group, for this window only, the stages being
         * the fallback; for a program that gets MUI 4 or 5 (MUI 5 is what a
         * program asking for version 20 meets today: its own EmbeddedImages
         * and Pointers demos open scrollable on real MUI 5, 2026-11-11),
         * never for MUI 3.8; the code in the title gadget module */
        if (MGI_IDENTITY(muiGlobalInfo(obj)) >= ZUNE_ID_MUI4
            && !(data->wd_Flags & MUIWF_SCROLLWRAP)
            && (sg = zune_window_scrollwrap(obj, ZUNE_SW_WRAP)))
        {
            data->wd_RootObject = sg;
            data->wd_Flags |= MUIWF_SCROLLWRAP;
        }
        if ((!sg && (mri->mri_Fallback >= FALLBACK_TOPAZ
                || (++mri->mri_Fallback, FALSE)))
            || !DoSetupMethod(data->wd_RootObject, mri))
        {
            WindowUnsetup(obj, data, 1);
            return FALSE;
        }
        OpenAllPresetFonts(data->wd_RootObject);
        WindowMinMax(obj, data);
    }
    if (data->wd_RenderInfo.mri_Fallback > MGI_FALLBACK(muiGlobalInfo(obj)))
        MGI_FALLBACK(muiGlobalInfo(obj)) = data->wd_RenderInfo.mri_Fallback;
    WindowSelectDimensions(data);

    /* Decide which menustrip should be used */
    if (!data->wd_ChildMenustrip)
        get(_app(obj), MUIA_Application_Menustrip, &data->wd_Menustrip);
    else
        data->wd_Menustrip = data->wd_ChildMenustrip;

    /* open window here ... */
    if (!DisplayWindow(obj, data))
    {
        /* free display dependant data */
        data->wd_Menustrip = NULL;
        WindowUnsetup(obj, data, 2);
        return FALSE;
    }

    InstallBackbuffer(cl, obj);

    data->wd_Flags |= MUIWF_OPENED;

    WindowShow(cl, obj);
    WindowBackground(data);

    if (data->wd_RenderInfo.mri_Screen
        && muiGlobalInfo(obj)->mgi_Prefs->publicscreen_pop_to_front)
    {
        ScreenToFront(data->wd_RenderInfo.mri_Screen);
    }

    MUI_Redraw(data->wd_RootObject, MADF_DRAWOBJECT | MADF_DRAWFRAME);

    D(bug("MUIC_Window:windowOpen() ActiveObject=%p\n",
            data->wd_ActiveObject));
    if (data->wd_OldActive != NULL)
    {
        set(obj, MUIA_Window_ActiveObject, data->wd_OldActive);
    }

    return TRUE;
}

/**************************************************************************/
/**************************************************************************/

/* Remember focus, clear it, delete help bubble, hide root/background,
 * release backbuffer and native display, clear open/effective-menu state
 * and clean up root then Window resources. Requires a live open window and
 * application. Return TRUE; Sleep nesting and AppWindow registration are
 * not reset here.
 */
static ULONG WindowClose(struct IClass *cl, Object *obj)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    CancelWindowGuards(data);
    data->wd_Flags &= ~MUIWF_REFITPENDING;

    if (data->wd_ActiveObject != NULL)
    {
        data->wd_OldActive = data->wd_ActiveObject;
        set(obj, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None);
    }

    KillHelpBubble(data, obj, BUBBLEHELP_TICKER_FIRST);

    /* Reset before classes dispose any borrowed pointer images. */
    if (data->wd_PointerType != MUIV_PointerType_Normal || data->wd_Pointer)
        ZuneFreePointer(data->wd_RenderInfo.mri_Window, &data->wd_Pointer);
    data->wd_PointerType = MUIV_PointerType_Normal;

    /* remove from window */
    if (data->wd_RootObject) DoHideMethod(data->wd_RootObject);
    zune_imspec_hide(data->wd_Background);


    HideRenderInfo(&data->wd_RenderInfo);

    /* close here ... */
    UndisplayWindow(obj, data);

    data->wd_Flags &= ~MUIWF_OPENED;
    data->wd_Menustrip = NULL;

    /* free display dependant data */
    WindowUnsetup(obj, data, data->wd_RootObject ? 2 : 1);
    return TRUE;
}

/* calculate a new layout
 * see MUIA_ShowMe
 * see Group_ExitChange
 * see Group_Columns
 * see Group_Rows
 */
/* Private MUIM_Window_RecalcDisplay: use the borrowed originator and
 * ancestor chain to choose relayout scope, recompute root limits, resize
 * the live native window, layout/show/redraw and reactivate focus. Return
 * zero while closed, TRUE after the sequence. Require an appropriate live
 * Area originator or NULL and stable hierarchy; intermediate results are
 * ignored.
 */
/* Push MUIM_Window_Refit to the application's input loop (not inlined:
 * its message stays off the stack of RecalcDisplay's layout) */
static __attribute__((noinline)) void WindowRefitLater(Object *obj,
    struct MUI_WindowData *data)
{
    data->wd_Flags |= MUIWF_REFITPENDING;
    DoMethod(_app(obj), MUIM_Application_PushMethod, (IPTR) obj, 1,
        MUIM_Window_Refit);
}

IPTR Window__MUIM_RecalcDisplay(struct IClass *cl, Object *obj,
    struct MUIP_Window_RecalcDisplay *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    BOOL reshow = FALSE;
    Object *current_obj;

    if (!(data->wd_Flags & MUIWF_OPENED))
        return 0;

    current_obj = msg->originator;

    // typically originator is a group which has been added/removed a child
    // calculate minmax of current obj
    // if new minmax can accomodate current obj size, stop
    // else try with its parent
    // the resulting object will get a new layout
    // it currently produces some redundant AskMinMax but allows
    // to not always relayout the whole window

    D(bug("RecalcDisplay on %p\n", current_obj));
    /* One MinMax pass over the whole tree, as MUI 3.8 does; the fit test
     * below reads the fresh values of the originator chain instead of
     * asking them a second time. Groups skip hidden children, so an
     * object inside a hidden subtree is still asked itself. */
    WindowMinMax(obj, data);

    /* Contents that no longer fit the screen at all: MUI 3.8 closes the
     * window and opens it again, which fits it stage by stage (smaller
     * fonts and spacing) and centres it; a window that fits no stage
     * stays closed (measured: docs/lean/README.md "A window that grows
     * past the screen"). Done from the application's input loop, since
     * the change may come from an object inside this window; until then
     * the window is laid out as before. */
    if (!WindowFits(data) && !(data->wd_Flags & MUIWF_REFITPENDING)
        && _app(obj))
        WindowRefitLater(obj, data);
    while (current_obj != NULL)
    {
        Object *o;

        for (o = current_obj; o && o != data->wd_RootObject; o = _parent(o))
            if (!(_flags(o) & MADF_SHOWME))
                break;
        if (o && o != data->wd_RootObject)
        {
            DoMethod(current_obj, MUIM_AskMinMax,
                (IPTR) & muiAreaData(current_obj)->mad_MinMax);
            __area_finish_minmax(current_obj,
                &muiAreaData(current_obj)->mad_MinMax);
        }

        D(bug("size w = %d, h = %d\n", _width(current_obj),
                _height(current_obj)));
        D(bug("new w = %d-%d, h = %d-%d\n", _minwidth(current_obj),
                _maxwidth(current_obj), _minheight(current_obj),
                _maxheight(current_obj)));

        if (!_between(_minwidth(current_obj), _width(current_obj),
                _maxwidth(current_obj))
            || !_between(_minheight(current_obj), _height(current_obj),
                _maxheight(current_obj)))
        {
            current_obj = _parent(current_obj);
            D(bug("RecalcDisplay, try parent %p\n", current_obj));
        }
        else
        {
            D(bug("found it\n"));
            break;
        }
    }

    if (!current_obj)
        current_obj = data->wd_RootObject;

    /* Important: current_obj could be hidden, like in an inactive page! */
    if (_flags(current_obj) & MADF_CANDRAW)
    {
        reshow = TRUE;
    }

    if (reshow)
        DoHideMethod(current_obj);

    /* resize window ? */
    WindowSelectDimensions(data);
    WindowResize(data);

    {
        struct Window *win = data->wd_RenderInfo.mri_Window;
        _left(data->wd_RootObject) = win->BorderLeft;
        _top(data->wd_RootObject) = win->BorderTop;
        muiAreaData(data->wd_RootObject)->mad_VirtualTop = win->BorderTop;
        _width(data->wd_RootObject) = data->wd_Width;
        _height(data->wd_RootObject) = data->wd_Height;
    }
    DoLayoutMethod(current_obj);

    if (reshow)
        DoShowMethod(current_obj);

    if (muiGlobalInfo(obj)->mgi_Prefs->window_redraw ==
        WINDOW_REDRAW_WITHOUT_CLEAR)
    {
        if (reshow)
        {
            /* An object that does not fill its area keeps the old child
             * pixels after relayout; clear them with the inherited
             * background, as MUI 3.8/3.9 do for hidden/removed children */
            /* A virtual group paints its background itself, inside its
             * BeginPaint/EndPaint pair: Voyager restricts its page there
             * to the part it is updating, and a clear outside the pair
             * erased the links above that part */
            if (!(_flags(current_obj) & (MADF_FILLAREA | MADF_ISVIRTUALGROUP)))
                DoMethod(current_obj, MUIM_DrawBackground,
                    _left(current_obj), _top(current_obj),
                    _width(current_obj), _height(current_obj),
                    _left(current_obj), _top(current_obj), 0);
            /* Its size is unchanged, so like MUI it keeps its own frame
             * unless the clear above painted over it */
            MUI_Redraw(current_obj, MADF_DRAWOBJECT |
                ((_flags(current_obj) & MADF_FILLAREA) ? 0 : MADF_DRAWFRAME));
        }
    }
    else
    {
        WindowBackground(data);
        MUI_Redraw(data->wd_RootObject, MADF_DRAWALL);
    }

    ActivateObject(data);

    return TRUE;
}


/****** Window.mui/MUIM_Window_AddEventHandler ************
*
*   NAME
*       MUIM_Window_AddEventHandler
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_AddEventHandler, (IPTR)ehnode);
*
*   FUNCTION
*       Link a borrowed event-handler node and recompute the native IDCMP mask
*       from default events and all registered masks. IDCMP_VANILLAKEY is always
*       removed.
*
*   INPUTS
*       ehnode - Non-NULL MUI_EventHandlerNode not already linked. Clear
*       reserved fields and set ehn_Priority, ehn_Flags, ehn_Object, optional
*       ehn_Class, and ehn_Events before registering. Keep the node, object and
*       class alive until removal.
*
*   RESULT
*       Always TRUE; there is no duplicate or NULL check.
*
*   NOTES
*       Handlers are ordered by descending signed BYTE priority, then descending
*       node address. ehn_Class selects CoerceMethod; NULL selects DoMethod.
*       Normal delivery calls MUIM_HandleEvent with a borrowed message and a
*       muikey value. MUI_EventHandlerRC_Eat stops further delivery on paths
*       that test it.
*       MUI_EHF_ALWAYSKEYS participates in the active/parent/default raw-key
*       paths even without an IDCMP_RAWKEY bit. MUI_EHF_GUIMODE filters
*       drawing/visibility. Virtual-group SELECTDOWN bounds are checked
*       independently of that flag; caller paths also test disabled state.
*       MUI_EHF_HANDLEINPUT is private. Remove handlers before freeing them;
*       removal is allowed during dispatch.
*
*   BUGS
*       Most delivered IntuiMessages are temporary copies whose original was
*       already replied to; IAddress is cleared except for raw-key dead-key
*       data. IDCMP_IDCMPUPDATE uses the original message. Never reply to or
*       retain either form. An Eat return in the IDCMPUPDATE path skips the
*       implementation's final ReplyMsg. The raw-key parent loop can deliver to
*       the active object twice.
*
*/
IPTR Window__MUIM_AddEventHandler(struct IClass *cl, Object *obj,
    struct MUIP_Window_AddEventHandler *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    //D(bug("muimaster.library/window.c: Add Eventhandler %p\n", msg->ehnode));

    msg->ehnode->ehn_Priority = msg->ehnode->ehn_Priority;
    EnqueueByPriAndAddress((struct List *)&data->wd_EHList,
        (struct Node *)msg->ehnode);
    ChangeEvents(data, GetDefaultEvents());
    return TRUE;
}

/****** Window.mui/MUIM_Window_RemEventHandler ************
*
*   NAME
*       MUIM_Window_RemEventHandler
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_RemEventHandler, (IPTR)ehnode);
*
*   FUNCTION
*       Unlink a registered event handler and recompute the IDCMP mask.
*
*   INPUTS
*       ehnode - The exact live node currently linked to this window.
*
*   RESULT
*       Always TRUE.
*
*   NOTES
*       The node is not freed. Removal is allowed during dispatch, also from the
*       node's own callback; release caller-owned storage after removal.
*
*   BUGS
*       No membership or NULL validation is performed. Repeated removal or
*       removal from the wrong list is invalid.
*
*/
IPTR Window__MUIM_RemEventHandler(struct IClass *cl, Object *obj,
    struct MUIP_Window_RemEventHandler *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    //D(bug("muimaster.library/window.c: Rem Eventhandler %p\n", msg->ehnode));

    RemoveEventHandler(data, msg->ehnode);
    ChangeEvents(data, GetDefaultEvents());
    return TRUE;
}

/**************************************************************************
 Note that this is MUIM_Window_Setup, not MUIM_Setup
**************************************************************************/
/****** Window.mui/MUIM_Window_Setup ************
*
*   NAME
*       MUIM_Window_Setup
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_Setup);
*
*   FUNCTION
*       Create the window background specification and force EraseArea on when
*       the preferences select redraw with clearing or smart refresh. Used by
*       the Window open sequence and custom Window subclasses.
*
*   INPUTS
*       No arguments beyond MethodID. The window must be attached to a live
*       application with valid preferences, and must not already own an active
*       setup.
*
*   RESULT
*       Always TRUE.
*
*   NOTES
*       This is MUIM_Window_Setup, distinct from the root Area's MUIM_Setup.
*       Opening calls it after acquiring render-info resources and before root
*       setup and native window creation. Subclasses must preserve the
*       superclass resource lifecycle and propagate failure. Pair a successful
*       setup with Cleanup in the close/unwind sequence.
*
*   BUGS
*       Background creation is not checked. TRUE is not proof that every render
*       resource exists.
*
*/
IPTR Window__MUIM_Setup(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    data->wd_Background =
        zune_imspec_setup(MUII_WindowBack, &data->wd_RenderInfo);

    /* MUI 3.8 also clears smart refresh windows after a resize; IBrowse's
     * toolbar leaves its box to the window background and otherwise kept
     * the logo from the previous size (tests/native/resizeclear.c) */
    if (muiGlobalInfo(obj)->mgi_Prefs->window_redraw ==
        WINDOW_REDRAW_WITH_CLEAR
        || muiGlobalInfo(obj)->mgi_Prefs->window_refresh ==
        WINDOW_REFRESH_SMART)
        data->wd_Flags |= MUIWF_ERASEAREA;

    return TRUE;
}

/****** Window.mui/MUIM_Window_Cleanup ************
*
*   NAME
*       MUIM_Window_Cleanup
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_Cleanup);
*
*   FUNCTION
*       Release the background and any drag context after hiding and closing.
*       Used by the Window close sequence and custom subclasses.
*
*   INPUTS
*       No arguments beyond MethodID. Requires resources from a successful setup
*       and no remaining users of them.
*
*   RESULT
*       Always TRUE.
*
*   NOTES
*       This does not close the native window, dispose the root or release
*       render-info resources; the close sequence releases those after this
*       method returns. It is not a public close operation. Subclasses must
*       release their dependent resources before calling the superclass
*       cleanup.
*
*/
IPTR Window__MUIM_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    CleanupWindowSetup(data);
    return TRUE;
}


/**************************************************************************
 This adds the the control char handler and also does the MUIA_CycleChain
 stuff. Orginal MUI does this another way.
**************************************************************************/
static LONG CycleOrder(struct MUI_WindowData *data, Object *obj)
{
    LONG i;
    if (!data->wd_CycleOrder) return -1;
    for (i = 0; data->wd_CycleOrder[i]; i++)
        if (data->wd_CycleOrder[i] == obj) return i;
    return -1;
}

/* A block of newsize bytes (cleared) with the oldsize bytes of old at
 * its start, old freed; NULL (old kept) if there is no memory */
static __attribute__((noinline)) APTR Regrow(APTR old, ULONG oldsize,
    ULONG newsize)
{
    APTR p = AllocVec(newsize, MEMF_ANY | MEMF_CLEAR);

    if (p)
    {
        CopyMem(old, p, oldsize);
        FreeVec(old);
    }
    return p;
}

/* Room for one more object in the chain: the removed ones' NULLs dropped,
 * else a block twice as large. FALSE if there is none. */
static __attribute__((noinline)) BOOL ChainRoom(struct MUI_WindowData *data)
{
    Object **p = data->wd_Chain;
    UWORD n = data->wd_ChainCount, i, k = 0;

    if (n < data->wd_ChainSize)
        return TRUE;
    for (i = 0; i < n; i++)
        if (p[i])
            p[k++] = p[i];
    data->wd_ChainCount = k;
    if (k < n)
        return TRUE;
    n = n ? n * 2 : 8;
    if (!(p = Regrow(p, k * sizeof(Object *), n * sizeof(Object *))))
        return FALSE;
    data->wd_Chain = p;
    data->wd_ChainSize = n;
    return TRUE;
}

/* An object of the explicit (MUIM_Window_SetCycleChain) order into the
 * chain, before the first one later in that order */
static __attribute__((noinline)) void ChainInsert(
    struct MUI_WindowData *data, Object *target)
{
    LONG rank = CycleOrder(data, target);
    Object **p = data->wd_Chain;
    UWORD i = 0, n = data->wd_ChainCount;

    while (i < n && CycleOrder(data, p[i]) <= rank)
        i++;
    for (; n > i; n--)
        p[n] = p[n - 1];
    p[i] = target;
    data->wd_ChainCount++;
}

IPTR Window__MUIM_SetCycleChain(struct IClass *cl, Object *obj,
    struct MUIP_Window_SetCycleChain *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    Object **order, **chain;
    ULONG count = 0, i, k;

    while (msg->obj[count]) count++;
    order = AllocVec((count + 1) * sizeof(Object *), MEMF_ANY);
    if (!order) return FALSE;
    chain = AllocVec((count + 1) * sizeof(Object *), MEMF_ANY);
    if (!chain)
    {
        FreeVec(order);
        return FALSE;
    }
    CopyMem(msg->obj, order, (count + 1) * sizeof(Object *));
    for (i = k = 0; i < count; i++)
    {
        Object *child = order[i];
        ULONG j;

        for (j = 0; j < k && chain[j] != child; j++)
            ;
        if (j == k && (_flags(child) & MADF_SETUP) && muiRenderInfo(child)
            && _win(child) == obj)
            chain[k++] = child;
    }
    FreeVec(data->wd_Chain);
    data->wd_Chain = chain;
    data->wd_ChainCount = k;
    data->wd_ChainSize = count + 1;
    FreeVec(data->wd_CycleOrder);
    data->wd_CycleOrder = order;
    /* Arrange control-handler setup/cleanup for legacy gadgets as well. */
    for (i = 0; i < count; i++)
        set(order[i], MUIA_CycleChain, TRUE);
    return TRUE;
}

/* Private MUIM_Window_AddControlCharHandler: borrow ccnode; enqueue it when
 * ehn_Events is nonzero and append a pooled cycle node when its live Area
 * target has CYCLECHAIN. Return TRUE even if cycle-node allocation fails.
 * Do not duplicate registration; node and target must remain valid until
 * matching removal.
 */
IPTR Window__MUIM_AddControlCharHandler(struct IClass *cl, Object *obj,
    struct MUIP_Window_AddControlCharHandler *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    Object *target = msg->ccnode->ehn_Object;

    if (msg->ccnode->ehn_Events)
        Enqueue((struct List *)&data->wd_CCList, (struct Node *)msg->ccnode);
    /* Due to the lack of a better idea ... */
    if (!data->wd_CycleOrder)
    {
        if ((muiAreaData(target)->mad_Flags & MADF_CYCLECHAIN)
            && ChainRoom(data))
            data->wd_Chain[data->wd_ChainCount++] = target;
    }
    else if (CycleOrder(data, target) >= 0 && ChainIndex(data, target) < 0
        && ChainRoom(data))
        ChainInsert(data, target);
    return TRUE;
}

/* Private MUIM_Window_RemControlCharHandler: unlink ccnode when its event
 * field is nonzero and remove the first chain entry of its target.
 * Return TRUE. Require the original live registration and unchanged
 * linkage-relevant fields; membership is not validated and caller-owned
 * ccnode is not freed.
 */
IPTR Window__MUIM_RemControlCharHandler(struct IClass *cl, Object *obj,
    struct MUIP_Window_RemControlCharHandler *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    LONG i = ChainIndex(data, msg->ccnode->ehn_Object);

    if (msg->ccnode->ehn_Events)
        RemoveEventHandler(data, msg->ccnode);

    if (i >= 0)
    {
        UWORD n = data->wd_ChainCount;
        Object **p = data->wd_Chain;

        p[i] = NULL;
        while (n && !p[n - 1])
            n--;
        data->wd_ChainCount = n;
    }

    return TRUE;
}

#if !defined(ZUNE68_GCC_NATIVE)
#include "../dragwin.c"
#endif

/* Private MUIM_Window_DragObject: ask the live source obj for a drag image
 * using touchx/touchy/flags, create a drag context and bitmap node,
 * attach/prepare it for the render screen, then mark the source dragging
 * and retain borrowed source/image pointers. Return 1 on success or 0 after
 * local allocation failure cleanup. Require an open window with no existing
 * drag; no overlap guard is implemented.
 */
IPTR Window__MUIM_DragObject(struct IClass *cl, Object *obj,
    struct MUIP_Window_DragObject *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    if (msg->obj)
        return zune_drag_begin(&data->wd_Drag, msg->obj, msg->touchx,
            msg->touchy, msg->flags, data->wd_RenderInfo.mri_Screen);
    return 0;
}

/****** Window.mui/MUIM_Window_AllocGadgetID ************
*
*   NAME
*       MUIM_Window_AllocGadgetID
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_AllocGadgetID);
*
*   FUNCTION
*       Allocate a pool node and choose a numeric identifier for a native BOOPSI
*       gadget.
*
*   INPUTS
*       No arguments beyond MethodID. Serialize use with other window
*       operations.
*
*   RESULT
*       Zero on pool allocation failure; otherwise the selected integer,
*       starting at 1.
*
*   NOTES
*       The allocation belongs to the window; release it with FreeGadgetID when
*       the gadget no longer uses it. Remaining nodes are reclaimed with the
*       window pool.
*
*   BUGS
*       The search assumes ascending IDs, but inserts a reused gap after the
*       larger node. After allocating 1 and 2, freeing 1 and allocating twice
*       can return 1 twice. There is no UWORD overflow check, and the returned
*       integer can differ from its truncated stored value. This implementation
*       cannot guarantee uniqueness after reuse.
*
*/
/* 32 more gadget IDs */
static __attribute__((noinline)) BOOL IDMapGrow(struct MUI_WindowData *data)
{
    UWORD n = data->wd_IDBytes;
    UBYTE *map = Regrow(data->wd_IDMap, n, n + 32);

    if (!map)
        return FALSE;
    data->wd_IDMap = map;
    data->wd_IDBytes = n + 32;
    return TRUE;
}

IPTR Window__MUIM_AllocGadgetID(struct IClass *cl, Object *obj,
    struct MUIP_Window_AllocGadgetID *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    /* the lowest free ID, as the sorted list gave it */
    do
    {
        UBYTE *map = data->wd_IDMap;
        ULONG i, n = data->wd_IDBytes;

        for (i = 0; i < n; i++)
            if (!map[i])
            {
                map[i] = 1;
                return i + 1;
            }
    }
    while (IDMapGrow(data));
    return 0;
}

/****** Window.mui/MUIM_Window_FreeGadgetID ************
*
*   NAME
*       MUIM_Window_FreeGadgetID
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_FreeGadgetID, gadgetid);
*
*   FUNCTION
*       Remove and free the first stored gadget-ID node equal to the argument.
*
*   INPUTS
*       gadgetid - LONG identifier previously obtained from this window. Stop
*       using the corresponding gadget before releasing its ID.
*
*   RESULT
*       Always zero, whether found or not.
*
*   NOTES
*       This frees only the allocation record, not the gadget. Duplicate or
*       truncated IDs from AllocGadgetID limit reliable matching.
*
*/
IPTR Window__MUIM_FreeGadgetID(struct IClass *cl, Object *obj,
    struct MUIP_Window_FreeGadgetID *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    ULONG id = msg->gadgetid - 1;

    if (id < data->wd_IDBytes)
        data->wd_IDMap[id] = 0;
    return 0;
}


/****** Window.mui/MUIM_Window_GetMenuCheck ************
*
*   NAME
*       MUIM_Window_GetMenuCheck
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_GetMenuCheck, MenuID);
*
*   FUNCTION
*       Get Menuitem_Checked on the first object found by UserData in the local
*       menustrip, or in the effective fallback menu while open.
*
*   INPUTS
*       MenuID - ULONG UserData value. Matching includes the menustrip object
*       itself.
*
*   RESULT
*       The queried value, or zero when no menu/object is available or the
*       getter does not supply it.
*
*   NOTES
*       These obsolete declarations require MUI_OBSOLETE, but their dispatch
*       cases are implemented. Prefer the menu object attributes when the caller
*       already holds the object. The application fallback menu is not selected
*       until opening. Zero does not distinguish a false state from an
*       unsuccessful lookup.
*
*/
IPTR Window__MUIM_GetMenuCheck(struct IClass *cl, Object *obj,
    struct MUIP_Window_GetMenuCheck *msg)
{
    IPTR stat = 0;
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    Object *item;
    Object *strip = data->wd_ChildMenustrip;
    if (!strip)
        strip = data->wd_Menustrip;
    if (!strip)
        return 0;
    if (!(item = (Object *) DoMethod(strip, MUIM_FindUData, msg->MenuID)))
        return 0;
    get(item, MUIA_Menuitem_Checked, &stat);
    return stat;
}

/****** Window.mui/MUIM_Window_SetMenuCheck ************
*
*   NAME
*       MUIM_Window_SetMenuCheck
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_SetMenuCheck, MenuID, stat);
*
*   FUNCTION
*       Set Menuitem_Checked on the first object found by UserData in the local
*       menustrip, or in the effective fallback menu while open.
*
*   INPUTS
*       MenuID - ULONG UserData value. Matching includes the menustrip object
*       itself. stat - LONG value passed to the menu-item setter.
*
*   RESULT
*       Always zero; absence and setter results are not reported.
*
*   NOTES
*       These obsolete declarations require MUI_OBSOLETE, but their dispatch
*       cases are implemented. Prefer the menu object attributes when the caller
*       already holds the object. The application fallback menu is not selected
*       until opening. Changing a menu attribute can synchronously rebuild
*       native menu structures; do not retain native MenuItem pointers.
*
*/
IPTR Window__MUIM_SetMenuCheck(struct IClass *cl, Object *obj,
    struct MUIP_Window_SetMenuCheck *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    Object *item;
    Object *strip = data->wd_ChildMenustrip;
    if (!strip)
        strip = data->wd_Menustrip;
    if (!strip)
        return 0;
    if (!(item = (Object *) DoMethod(strip, MUIM_FindUData, msg->MenuID)))
        return 0;
    set(item, MUIA_Menuitem_Checked, msg->stat);
    return 0;
}

/****** Window.mui/MUIM_Window_GetMenuState ************
*
*   NAME
*       MUIM_Window_GetMenuState
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_GetMenuState, MenuID);
*
*   FUNCTION
*       Get Menuitem_Enabled on the first object found by UserData in the local
*       menustrip, or in the effective fallback menu while open.
*
*   INPUTS
*       MenuID - ULONG UserData value. Matching includes the menustrip object
*       itself.
*
*   RESULT
*       The queried value, or zero when no menu/object is available or the
*       getter does not supply it.
*
*   NOTES
*       These obsolete declarations require MUI_OBSOLETE, but their dispatch
*       cases are implemented. Prefer the menu object attributes when the caller
*       already holds the object. The application fallback menu is not selected
*       until opening. Zero does not distinguish a false state from an
*       unsuccessful lookup.
*
*/
IPTR Window__MUIM_GetMenuState(struct IClass *cl, Object *obj,
    struct MUIP_Window_GetMenuState *msg)
{
    IPTR stat = 0;
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    Object *item;
    Object *strip = data->wd_ChildMenustrip;
    if (!strip)
        strip = data->wd_Menustrip;
    if (!strip)
        return 0;
    if (!(item = (Object *) DoMethod(strip, MUIM_FindUData, msg->MenuID)))
        return 0;
    get(item, MUIA_Menuitem_Enabled, &stat);
    return stat;
}

/****** Window.mui/MUIM_Window_SetMenuState ************
*
*   NAME
*       MUIM_Window_SetMenuState
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_SetMenuState, MenuID, stat);
*
*   FUNCTION
*       Set Menuitem_Enabled on the first object found by UserData in the local
*       menustrip, or in the effective fallback menu while open.
*
*   INPUTS
*       MenuID - ULONG UserData value. Matching includes the menustrip object
*       itself. stat - LONG value passed to the menu-item setter.
*
*   RESULT
*       Always zero; absence and setter results are not reported.
*
*   NOTES
*       These obsolete declarations require MUI_OBSOLETE, but their dispatch
*       cases are implemented. Prefer the menu object attributes when the caller
*       already holds the object. The application fallback menu is not selected
*       until opening. Changing a menu attribute can synchronously rebuild
*       native menu structures; do not retain native MenuItem pointers.
*
*/
IPTR Window__MUIM_SetMenuState(struct IClass *cl, Object *obj,
    struct MUIP_Window_SetMenuState *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    Object *item;
    Object *strip = data->wd_ChildMenustrip;
    if (!strip)
        strip = data->wd_Menustrip;
    if (!strip)
        return 0;
    if (!(item = (Object *) DoMethod(strip, MUIM_FindUData, msg->MenuID)))
        return 0;
    set(item, MUIA_Menuitem_Enabled, msg->stat);
    return 0;
}

/* Private MUIM_Window_DrawBackground: draw the setup-owned background over
 * left/top/width/height with xoffset/yoffset while open. Ignore message
 * flags and return zero, also while closed. Require valid render resources
 * and drawing lifetime; do not interpret zero as a draw-failure report.
 */
IPTR Window__MUIM_DrawBackground(struct IClass *cl, Object *obj,
    struct MUIP_Window_DrawBackground *msg);

static struct MUI_WindowData *window_data(Object *win)
{
    struct IClass *cl;

    for (cl = win ? OCLASS(win) : NULL; cl; cl = cl->cl_Super)
        if (cl->cl_Dispatcher.h_SubEntry
            == (HOOKFUNC) _MUI_Window_desc.dispatcher)
            return INST_DATA(cl, win);
    return NULL;
}

/* TRUE when a window background fill does not depend on its rectangle */
BOOL zune_window_background_is_position_free(Object *win)
{
    struct MUI_WindowData *data = window_data(win);

    return data && zune_imspec_is_position_free(data->wd_Background);
}

/* TRUE when the window background is a plain fill with MUI pen muipen */
BOOL zune_window_background_is_pen(Object *win, LONG muipen)
{
    struct MUI_WindowData *data = window_data(win);

    return data && zune_imspec_is_mui_pen(data->wd_Background, muipen);
}

/* The method is Zune-private, so the built-in implementation is the
 * one a dispatch would reach whatever the window's subclass. */
IPTR zune_window_draw_background(Object *win, LONG left, LONG top,
    LONG width, LONG height, LONG xoffset, LONG yoffset, ULONG flags)
{
    struct IClass *cl;

    for (cl = win ? OCLASS(win) : NULL; cl; cl = cl->cl_Super)
        if (cl->cl_Dispatcher.h_SubEntry
            == (HOOKFUNC) _MUI_Window_desc.dispatcher)
        {
            struct MUIP_Window_DrawBackground msg =
                { MUIM_Window_DrawBackground, left, top, width, height,
                  xoffset, yoffset, flags };

            return Window__MUIM_DrawBackground(cl, win, &msg);
        }
    return DoMethod(win, MUIM_Window_DrawBackground, left, top, width,
        height, xoffset, yoffset, flags);
}

IPTR Window__MUIM_DrawBackground(struct IClass *cl, Object *obj,
    struct MUIP_Window_DrawBackground *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    if (!(data->wd_RenderInfo.mri_Window))      /* not between show/hide */
        return FALSE;

//    D(bug("%d:zune_imspec_draw(%p) l=%d t=%d w=%d h=%d xo=%d yo=%d\n",
//          __LINE__, data->wd_Background, msg->left, msg->top, msg->width,
//          msg->height, msg->xoffset, msg->yoffset));
    zune_imspec_draw(data->wd_Background, &data->wd_RenderInfo,
        msg->left, msg->top, msg->width, msg->height,
        msg->xoffset, msg->yoffset, 0);
    return 0;
}

/****** Window.mui/MUIM_Window_ToFront ************
*
*   NAME
*       MUIM_Window_ToFront
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_ToFront);
*
*   FUNCTION
*       Move the open native window to the front.
*
*   INPUTS
*       No arguments beyond MethodID.
*
*   RESULT
*       Zero when no native window exists; otherwise 1 after the Intuition call.
*
*   NOTES
*       This does not open the window or report the final stacking order. It
*       does not separately request keyboard activation.
*
*/
/* MUIM_Window_ToFront, ToBack, ScreenToFront and ScreenToBack: nothing
 * (0) unless the window is open */
static IPTR Window__MUIM_Arrange(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    struct Window *win = data->wd_RenderInfo.mri_Window;
    struct Screen *scr = data->wd_RenderInfo.mri_Screen;

    if (!win)      /* not between show/hide */
        return 0;

    switch (msg->MethodID)
    {
    case MUIM_Window_ToFront: WindowToFront(win); break;
    case MUIM_Window_ToBack: WindowToBack(win); break;
    case MUIM_Window_ScreenToFront: ScreenToFront(scr); break;
    default: ScreenToBack(scr); break;
    }
    return 1;
}

/****** Window.mui/MUIM_Window_ToBack ************
*
*   NAME
*       MUIM_Window_ToBack
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_ToBack);
*
*   FUNCTION
*       Move the open native window to the back.
*
*   INPUTS
*       No arguments beyond MethodID.
*
*   RESULT
*       Zero when no native window exists; otherwise 1 after the Intuition call.
*
*   NOTES
*       This does not open the window or report the final stacking order. It
*       does not separately request keyboard activation.
*
*/

/****** Window.mui/MUIM_Window_ScreenToBack ************
*
*   NAME
*       MUIM_Window_ScreenToBack
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_ScreenToBack);
*
*   FUNCTION
*       Move the screen containing the open native window to the back.
*
*   INPUTS
*       No arguments beyond MethodID.
*
*   RESULT
*       Zero when no native window exists; otherwise 1 after the Intuition call.
*
*   NOTES
*       This does not open the window or report the final stacking order. It
*       does not separately request keyboard activation.
*
*/

/****** Window.mui/MUIM_Window_ScreenToFront ************
*
*   NAME
*       MUIM_Window_ScreenToFront
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_ScreenToFront);
*
*   FUNCTION
*       Move the screen containing the open native window to the front.
*
*   INPUTS
*       No arguments beyond MethodID.
*
*   RESULT
*       Zero when no native window exists; otherwise 1 after the Intuition call.
*
*   NOTES
*       This does not open the window or report the final stacking order. It
*       does not separately request keyboard activation.
*
*/

/****** Window.mui/MUIM_Window_ActionIconify ************
*
*   NAME
*       MUIM_Window_ActionIconify
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_ActionIconify);
*
*   FUNCTION
*       Set Application_Iconified to TRUE on this window's application.
*
*   INPUTS
*       No arguments beyond MethodID. The window must have a live connected
*       application.
*
*   RESULT
*       Always TRUE; the application setter result is ignored.
*
*   NOTES
*       This iconifies the application, not only this window. Iconified state
*       does not establish that an AppIcon was successfully created.
*
*/
IPTR Window__MUIM_ActionIconify(struct IClass *cl, Object *obj, Msg msg)
{
    set(_app(obj), MUIA_Application_Iconified, TRUE);

    return TRUE;
}


/* Automatic remember-on-exit updates memory only. Explicit snapshots also
 * update the application's preferences on disk. */
void ZuneRememberWindowPosition(Object *obj)
{
    struct MUI_WindowData *data = muiWindowData(obj);
    struct Window *w = data->wd_RenderInfo.mri_Window;
    struct windowpos pos = {0};
    if (!data->wd_ID || !w) return;
    pos.id = data->wd_ID;
    pos.x1 = w->LeftEdge; pos.y1 = w->TopEdge;
    pos.w1 = w->GZZWidth; pos.h1 = w->GZZHeight;
    set(_app(obj), MUIA_Application_SetWinPos, &pos);
}


/****** Window.mui/MUIM_Window_Snapshot ************
*
*   NAME
*       MUIM_Window_Snapshot
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_Snapshot, flags);
*
*   FUNCTION
*       Nonzero flags remember the window's current position and size under
*       its ID; zero forgets the remembered position. The change is made in
*       the application's window positions and in the saved preferences.
*
*   INPUTS
*       flags - LONG; nonzero to snapshot, zero to unsnapshot.
*
*   RESULT
*       FALSE without a window ID or a connected application, otherwise TRUE.
*
*/
IPTR Window__MUIM_Snapshot(struct IClass *cl, Object *obj,
    struct MUIP_Window_Snapshot *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    struct windowpos empty = {0};
    if (!data->wd_ID || !muiGlobalInfo(obj) || !_app(obj)) return FALSE;
    if (msg->flags)
        ZuneRememberWindowPosition(obj);
    else
    {
        empty.id = data->wd_ID;
        set(_app(obj), MUIA_Application_SetWinPos, &empty);
    }
#ifdef ZUNE68_GCC_NATIVE
    zune_window_save_snapshot(obj, data->wd_ID, msg->flags != 0);
#endif
    return TRUE;
}

/* Private MUIM_Window_UpdateMenu: when an effective menu exists, clear and
 * free native menus, build and lay out replacements and install them on the
 * live window. Requires open display resources and stable menu objects.
 * Return 1 even on absent menu or allocation failure; borrowed native
 * MenuItem pointers are invalidated by rebuilding.
 */
IPTR Window__MUIM_UpdateMenu(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    struct Menu *menu = NULL;
    APTR visinfo = NULL;
    struct Window *win = NULL;

    if (data->wd_Menustrip)     // only open windows can have a menustrip
    {
        if ((visinfo =
                GetVisualInfoA(data->wd_RenderInfo.mri_Screen, NULL)))
        {
            win = data->wd_RenderInfo.mri_Window;
            ClearMenuStrip(win);
            if (data->wd_Menu)
            {
                FreeMenus(data->wd_Menu);
                data->wd_Menu = NULL;
            }

            if ((menu = MakeMenus(visinfo, data->wd_Menustrip)))
            {
                data->wd_Menu = menu;
                SetMenuStrip(win, menu);
            }
            FreeVisualInfo(visinfo);
        }
    }

    return 1;
}

/* Private MUIM_Window_Refresh: invoke RefreshWindow and return 1. Requires
 * a live open window with setup root/resources; this wrapper does not guard
 * closed windows or validate the refresh result.
 */
IPTR Window__MUIM_Refresh(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    RefreshWindow(obj, data);

    return 1;
}

/* Forward the borrowed export message synchronously to the current root and
 * return zero regardless of its result. Require a non-NULL live root and
 * valid dataspace/message fields; do not retain the message or transfer
 * ownership.
 */
static IPTR Window__MUIM_Export(struct IClass *cl, Object *obj,
    struct MUIP_Export *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    DoMethodA(data->wd_RootObject, (Msg) msg);
    return 0;
}


/* Forward the borrowed import message synchronously to the current root and
 * return zero regardless of its result. Require a non-NULL live root and
 * valid dataspace/message fields. Child changes are not rolled back and no
 * ownership is transferred.
 */
static IPTR Window__MUIM_Import(struct IClass *cl, Object *obj,
    struct MUIP_Import *msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);
    DoMethodA(data->wd_RootObject, (Msg) msg);
    return 0;
}

/****** Window.mui/MUIM_Window_SetCycleChain ************
*
*   NAME
*       MUIM_Window_SetCycleChain
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Window_SetCycleChain, obj1, ...,
*           NULL);
*
*   FUNCTION
*       Replace the window's cycle chain with the listed objects in list
*       order, skipping repeated entries and objects not set up in this
*       window. Each listed object then gets MUIA_CycleChain TRUE.
*
*   INPUTS
*       obj1, ... - NULL-terminated list of Area objects. The message is
*       declared under MUI_OBSOLETE.
*
*   RESULT
*       FALSE on allocation failure, leaving the chain unchanged; otherwise
*       TRUE.
*
*   NOTES
*       The window keeps a copy of the order. An object registered later
*       through control-character handling joins the chain only if listed,
*       at its listed position.
*
*/

/* Dispatch supported Window and inherited object/user-data/lifecycle
 * methods, returning the selected handler result. Forward unknown methods
 * to Notify. Require a live object and correctly formed message for the
 * selected MethodID; header-only declarations do not add dispatcher
 * behavior.
 */
/* Private MUIM_Window_Refit, pushed by RecalcDisplay when the contents
 * outgrew the screen: close the window and open it again, which fits it
 * stage by stage as at the first opening. Nothing if the window was closed
 * (or opened again) since. */
static __attribute__((noinline)) IPTR Window__MUIM_Refit(struct IClass *cl,
    Object *obj, Msg msg)
{
    struct MUI_WindowData *data = INST_DATA(cl, obj);

    if ((data->wd_Flags & (MUIWF_REFITPENDING | MUIWF_OPENED))
        == (MUIWF_REFITPENDING | MUIWF_OPENED))
    {
        set(obj, MUIA_Window_Open, FALSE);
        /* at its default size, as a first opening (MUI 3.8 too) */
        data->wd_Width = data->wd_Height = 0;
        set(obj, MUIA_Window_Open, TRUE);
    }
    data->wd_Flags &= ~MUIWF_REFITPENDING;
    return 0;
}

/* The methods Window answers, by MethodID (unsigned, ascending: the
 * dispatcher halves the table to find one; the compiler checks the order).
 * OM_SET, which opens and closes the window, is tested before. */
static const struct { ULONG id; APTR method; } window_methods[] =
{
    { OM_NEW, (APTR)Window__OM_NEW },
    { OM_DISPOSE, (APTR)Window__OM_DISPOSE },
    { OM_GET, (APTR)Window__OM_GET },
    { MUIM_FindObject, (APTR)Window__MUIM_FindUData },
    { MUIM_Window_AddEventHandler, (APTR)Window__MUIM_AddEventHandler },
    { MUIM_Window_GetMenuCheck, (APTR)Window__MUIM_GetMenuCheck },
    { MUIM_Window_GetMenuState, (APTR)Window__MUIM_GetMenuState },
    { MUIM_Export, (APTR)Window__MUIM_Export },
    { MUIM_Window_ToBack, (APTR)Window__MUIM_Arrange },
    { MUIM_Window_SetMenuCheck, (APTR)Window__MUIM_SetMenuCheck },
    { MUIM_Window_ScreenToFront, (APTR)Window__MUIM_Arrange },
    { MUIM_Window_SetMenuState, (APTR)Window__MUIM_SetMenuState },
    { MUIM_Window_ActionIconify, (APTR)Window__MUIM_ActionIconify },
    { MUIM_Window_ToFront, (APTR)Window__MUIM_Arrange },
    { MUIM_Window_SetCycleChain, (APTR)Window__MUIM_SetCycleChain },
    { MUIM_Window_RemEventHandler, (APTR)Window__MUIM_RemEventHandler },
    { MUIM_Window_ScreenToBack, (APTR)Window__MUIM_Arrange },
    { MUIM_Window_Snapshot, (APTR)Window__MUIM_Snapshot },
    { MUIM_Window_Cleanup, (APTR)Window__MUIM_Cleanup },
    { MUIM_FindUData, (APTR)Window__MUIM_FindUData },
    { MUIM_Window_Setup, (APTR)Window__MUIM_Setup },
    { MUIM_SetUData, (APTR)Window__MUIM_SetUData },
    { MUIM_SetUDataOnce, (APTR)Window__MUIM_SetUDataOnce },
    { MUIM_Import, (APTR)Window__MUIM_Import },
    { MUIM_GetUData, (APTR)Window__MUIM_GetUData },
    { MUIM_ConnectParent, (APTR)Window__MUIM_ConnectParent },
    { MUIM_DisconnectParent, (APTR)Window__MUIM_DisconnectParent },
    { MUIM_Window_AddControlCharHandler, (APTR)Window__MUIM_AddControlCharHandler },
    { MUIM_Window_AllocGadgetID, (APTR)Window__MUIM_AllocGadgetID },
    { MUIM_Window_DrawBackground, (APTR)Window__MUIM_DrawBackground },
    { MUIM_Window_DragObject, (APTR)Window__MUIM_DragObject },
    { MUIM_Window_FreeGadgetID, (APTR)Window__MUIM_FreeGadgetID },
    { MUIM_Window_RecalcDisplay, (APTR)Window__MUIM_RecalcDisplay },
    { MUIM_Window_RemControlCharHandler, (APTR)Window__MUIM_RemControlCharHandler },
    { MUIM_Window_UpdateMenu, (APTR)Window__MUIM_UpdateMenu },
    { MUIM_Window_Refresh, (APTR)Window__MUIM_Refresh },
    { MUIM_Window_Refit, (APTR)Window__MUIM_Refit },
};
_Static_assert((ULONG)OM_NEW < (ULONG)OM_DISPOSE, "window_methods order");
_Static_assert((ULONG)OM_DISPOSE < (ULONG)OM_GET, "window_methods order");
_Static_assert((ULONG)OM_GET < (ULONG)MUIM_FindObject, "window_methods order");
_Static_assert((ULONG)MUIM_FindObject < (ULONG)MUIM_Window_AddEventHandler, "window_methods order");
_Static_assert((ULONG)MUIM_Window_AddEventHandler < (ULONG)MUIM_Window_GetMenuCheck, "window_methods order");
_Static_assert((ULONG)MUIM_Window_GetMenuCheck < (ULONG)MUIM_Window_GetMenuState, "window_methods order");
_Static_assert((ULONG)MUIM_Window_GetMenuState < (ULONG)MUIM_Export, "window_methods order");
_Static_assert((ULONG)MUIM_Export < (ULONG)MUIM_Window_ToBack, "window_methods order");
_Static_assert((ULONG)MUIM_Window_ToBack < (ULONG)MUIM_Window_SetMenuCheck, "window_methods order");
_Static_assert((ULONG)MUIM_Window_SetMenuCheck < (ULONG)MUIM_Window_ScreenToFront, "window_methods order");
_Static_assert((ULONG)MUIM_Window_ScreenToFront < (ULONG)MUIM_Window_SetMenuState, "window_methods order");
_Static_assert((ULONG)MUIM_Window_SetMenuState < (ULONG)MUIM_Window_ActionIconify, "window_methods order");
_Static_assert((ULONG)MUIM_Window_ActionIconify < (ULONG)MUIM_Window_ToFront, "window_methods order");
_Static_assert((ULONG)MUIM_Window_ToFront < (ULONG)MUIM_Window_SetCycleChain, "window_methods order");
_Static_assert((ULONG)MUIM_Window_SetCycleChain < (ULONG)MUIM_Window_RemEventHandler, "window_methods order");
_Static_assert((ULONG)MUIM_Window_RemEventHandler < (ULONG)MUIM_Window_ScreenToBack, "window_methods order");
_Static_assert((ULONG)MUIM_Window_ScreenToBack < (ULONG)MUIM_Window_Snapshot, "window_methods order");
_Static_assert((ULONG)MUIM_Window_Snapshot < (ULONG)MUIM_Window_Cleanup, "window_methods order");
_Static_assert((ULONG)MUIM_Window_Cleanup < (ULONG)MUIM_FindUData, "window_methods order");
_Static_assert((ULONG)MUIM_FindUData < (ULONG)MUIM_Window_Setup, "window_methods order");
_Static_assert((ULONG)MUIM_Window_Setup < (ULONG)MUIM_SetUData, "window_methods order");
_Static_assert((ULONG)MUIM_SetUData < (ULONG)MUIM_SetUDataOnce, "window_methods order");
_Static_assert((ULONG)MUIM_SetUDataOnce < (ULONG)MUIM_Import, "window_methods order");
_Static_assert((ULONG)MUIM_Import < (ULONG)MUIM_GetUData, "window_methods order");
_Static_assert((ULONG)MUIM_GetUData < (ULONG)MUIM_ConnectParent, "window_methods order");
_Static_assert((ULONG)MUIM_ConnectParent < (ULONG)MUIM_DisconnectParent, "window_methods order");
_Static_assert((ULONG)MUIM_DisconnectParent < (ULONG)MUIM_Window_AddControlCharHandler, "window_methods order");
_Static_assert((ULONG)MUIM_Window_AddControlCharHandler < (ULONG)MUIM_Window_AllocGadgetID, "window_methods order");
_Static_assert((ULONG)MUIM_Window_AllocGadgetID < (ULONG)MUIM_Window_DrawBackground, "window_methods order");
_Static_assert((ULONG)MUIM_Window_DrawBackground < (ULONG)MUIM_Window_DragObject, "window_methods order");
_Static_assert((ULONG)MUIM_Window_DragObject < (ULONG)MUIM_Window_FreeGadgetID, "window_methods order");
_Static_assert((ULONG)MUIM_Window_FreeGadgetID < (ULONG)MUIM_Window_RecalcDisplay, "window_methods order");
_Static_assert((ULONG)MUIM_Window_RecalcDisplay < (ULONG)MUIM_Window_RemControlCharHandler, "window_methods order");
_Static_assert((ULONG)MUIM_Window_RemControlCharHandler < (ULONG)MUIM_Window_UpdateMenu, "window_methods order");
_Static_assert((ULONG)MUIM_Window_UpdateMenu < (ULONG)MUIM_Window_Refresh, "window_methods order");
_Static_assert((ULONG)MUIM_Window_Refresh < (ULONG)MUIM_Window_Refit, "window_methods order");

/* The function of a method in window_methods, NULL if Window has none
 * (a function of its own: its search is off the stack when the method
 * runs) */
static __attribute__((noinline)) APTR WindowMethod(ULONG id)
{
    const ULONG *lo = &window_methods[0].id,
        *hi = &window_methods[sizeof(window_methods)
            / sizeof(window_methods[0]) - 1].id, *p;

    while (lo <= hi)
    {
        ULONG m;

        p = lo + (((hi - lo) >> 2) << 1);
        if ((m = *p) == id)
            return (APTR)p[1];
        if (m < id)
            lo = p + 2;
        else
            hi = p - 2;
    }
    return NULL;
}

BOOPSI_DISPATCHER(IPTR, Window_Dispatcher, cl, obj, msg)
{
    IPTR (*method)(struct IClass *, Object *, Msg);

    if (msg->MethodID == OM_SET)
#if defined(ZUNE68_GCC_NATIVE) && defined(__mc68000__)
        /* Opening a window draws all of it: see zune_deep_call() */
        return zune_deep_call(
            (IPTR (*)(struct IClass *, Object *, Msg))Window__OM_SET,
            cl, obj, msg);
#else
        return Window__OM_SET(cl, obj, (struct opSet *)msg);
#endif
    if ((method = WindowMethod(msg->MethodID)))
        return method(cl, obj, msg);
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUI_Window_desc =
{
    MUIC_Window,
    MUIC_Notify,
    sizeof(struct MUI_WindowData),
    (void *) Window_Dispatcher
};
