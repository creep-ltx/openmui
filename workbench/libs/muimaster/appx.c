/*
    Copyright (C) 2002-2026, The AROS Development Team.
    Copyright (C) 1999, David Le Corfec.
    All rights reserved.
*/

/* Application's rarely used parts, in the application module Zapp.mui
 * (classes/zapp.c, modules.h): ARexx commands, Workbench's icon and
 * menu item while iconified, Exchange's commands and the hotkey,
 * AppMessages, ShowHelp, AboutMUI, OpenConfigWindow, Execute, AROS's
 * version attributes. A
 * MUI 3 program that uses none of them never loads it. The library calls
 * these through modules.c under the names below. */

#include <stddef.h>
#include <string.h>
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
#include "classes/application_private.h"
#include "modules.h"


extern struct Library *MUIMasterBase;


/* MUI's standard ARexx commands. They take precedence over the
 * application's own commands of the same name, as in MUI 3.9. */
enum { REXX_QUIT, REXX_HIDE, REXX_DEACTIVATE, REXX_SHOW, REXX_ACTIVATE,
    REXX_INFO, REXX_HELP, REXX_BUILTINS };

static const struct
{
    CONST_STRPTR name;
    CONST_STRPTR template;
} rexx_builtins[REXX_BUILTINS] = {
    {"quit", "FORCE/S"}, {"hide", NULL}, {"deactivate", NULL},
    {"show", NULL}, {"activate", NULL}, {"info", "ITEM/A"},
    {"help", "FILE/A"}
};

/* MUI 3.8 and 3.9 reply -4 when the arguments do not match the template
 * (an INFO item it does not know too), MUI 3.8 -3 for a command it does
 * not know (measured with wasabi arexx). */
#define REXX_BAD_ARGS (-4)
#define REXX_UNKNOWN (-3)

static void rexx_help_section(BPTR fh, CONST_STRPTR title)
{
    FPrintf(fh, (STRPTR)"%s\n\nCommand      Template\n"
        "-------      --------\n", (IPTR)title);
}

static void rexx_help_line(BPTR fh, CONST_STRPTR name, CONST_STRPTR template)
{
    if (!template || template == MC_TEMPLATE_ID)
        template = "";
    FPrintf(fh, (STRPTR)"%-12s %s\n", (IPTR)name, (IPTR)template);
}

static LONG rexx_help(struct MUI_ApplicationData *data, CONST_STRPTR file)
{
    struct MUI_Command *cmd;
    BPTR fh = Open((STRPTR)file, MODE_NEWFILE);
    int i;

    if (!fh)
        return RETURN_ERROR;
    rexx_help_section(fh, "Standard-Commands:");
    for (i = 0; i < REXX_BUILTINS; i++)
        rexx_help_line(fh, rexx_builtins[i].name, rexx_builtins[i].template);
    if (data->app_Commands && data->app_Commands->mc_Name)
    {
        FPrintf(fh, (STRPTR)"\n\nCommands for application \"%s\":\n\n"
            "Command      Template\n-------      --------\n",
            (IPTR)(data->app_Title ? data->app_Title : (STRPTR)""));
        for (cmd = data->app_Commands; cmd->mc_Name; cmd++)
            rexx_help_line(fh, cmd->mc_Name, cmd->mc_Template);
    }
    Close(fh);
    return RETURN_OK;
}

static LONG rexx_info(struct MUI_ApplicationData *data, CONST_STRPTR item)
{
    static const char *const items[] = {
        "base", "title", "author", "copyright", "description", "version",
        "screen", NULL
    };
    CONST_STRPTR value = NULL;
    STRPTR copy;
    int i;

    for (i = 0; items[i] && Stricmp((STRPTR)item, (STRPTR)items[i]); i++)
        ;
    switch (i)
    {
    case 0: value = data->app_Base; break;
    case 1: value = data->app_Title; break;
    case 2: value = data->app_Author; break;
    case 3: value = data->app_Copyright; break;
    case 4: value = data->app_Description; break;
    case 5: value = data->app_Version; break;
    case 6: value = ""; break;
    default: return REXX_BAD_ARGS;
    }
    copy = StrDup(value ? value : (CONST_STRPTR)"?");
    if (!copy)
        return RETURN_FAIL;
    FreeVec(data->app_RexxString);
    data->app_RexxString = copy;
    return RETURN_OK;
}

static LONG rexx_builtin(struct MUI_ApplicationData *data, Object *obj,
    int which, IPTR *args)
{
    switch (which)
    {
    case REXX_QUIT:
        data->app_ForceQuit = args[0] ? TRUE : FALSE;
        if (!application_return_id(data, MUIV_Application_ReturnID_Quit))
            return RETURN_FAIL;
        break;
    case REXX_HIDE:
    case REXX_SHOW:
        set(obj, MUIA_Application_Iconified, which == REXX_HIDE);
        break;
    case REXX_DEACTIVATE:
    case REXX_ACTIVATE:
        set(obj, MUIA_Application_Active, which == REXX_ACTIVATE);
        break;
    case REXX_INFO:
        return rexx_info(data, (CONST_STRPTR)args[0]);
    case REXX_HELP:
        return rexx_help(data, (CONST_STRPTR)args[0]);
    }
    return RETURN_OK;
}

/*
 * MUIM_Application_InputBuffered : process all pending events
 */
void zune_app_rexx(struct MUI_ApplicationData *data, Object *obj,
    struct RexxMsg *rmsg)
{
    struct MUI_Command *cmd = data->app_Commands;
    struct RexxMsg *previous_msg = data->app_RexxMsg;
    STRPTR previous_string = data->app_RexxString;
    STRPTR command = (STRPTR)rmsg->rm_Args[0];
    CONST_STRPTR template = NULL;
    ULONG length = 0, parameters = 0;
    LONG result = RETURN_ERROR;
    int builtin;
    IPTR *args = NULL;
    STRPTR arguments = NULL;
    struct RDArgs *rdargs = NULL, *parsed = NULL;

    /* This module has its own copies of the library bases, taken when it
     * was loaded (classfile.h): rexxsyslib.library may have been opened
     * by the library after that (the first ARexx message), so the copy is
     * brought up to date here (CreateArgstring below) */
    if (!ZUNE_REXX())
        return;
    data->app_RexxMsg = rmsg;
    data->app_RexxString = NULL;
    rmsg->rm_Result2 = 0;
    if (!command)
        goto done;
    while (*command == ' ' || *command == '\t') command++;
    while (command[length] && command[length] != ' '
        && command[length] != '\t') length++;
    for (builtin = 0; builtin < REXX_BUILTINS; builtin++)
    {
        if (length == strlen(rexx_builtins[builtin].name)
            && Strnicmp(command, (STRPTR)rexx_builtins[builtin].name,
                length) == 0)
            break;
    }
    if (builtin < REXX_BUILTINS)
    {
        template = rexx_builtins[builtin].template;
        parameters = 1;
    }
    else
    {
        while (cmd && cmd->mc_Name)
        {
            if (length == strlen(cmd->mc_Name)
                && Strnicmp(command, cmd->mc_Name, length) == 0)
                break;
            cmd++;
        }
        if (!cmd || !cmd->mc_Name)
        {
            result = data->app_RexxHook
                ? (LONG)CallHookPkt(data->app_RexxHook, obj, rmsg)
                : REXX_UNKNOWN;
            goto done;
        }
        if ((STRPTR)cmd->mc_Template == MC_TEMPLATE_ID)
        {
            /* A plain return id; MUI ignores any arguments. */
            result = application_return_id(data, cmd->mc_Parameters)
                ? RETURN_OK : RETURN_FAIL;
            goto done;
        }
        if (!cmd->mc_Hook)
            goto done;
        template = cmd->mc_Template;
        parameters = cmd->mc_Parameters;
    }
    if (template)
    {
        STRPTR tail = command + length;
        if (parameters > (ULONG)-1 / sizeof(IPTR))
            goto done;
        args = AllocVec((parameters ? parameters : 1) * sizeof(IPTR),
            MEMF_ANY | MEMF_CLEAR);
        while (*tail == ' ' || *tail == '\t') tail++;
        length = strlen(tail);
        arguments = AllocVec(length + 2, MEMF_ANY);
        rdargs = AllocDosObject(DOS_RDARGS, NULL);
        if (!args || !arguments || !rdargs)
            goto done;
        CopyMem(tail, arguments, length);
        arguments[length] = '\n';
        arguments[length + 1] = 0;
        rdargs->RDA_Source.CS_Buffer = arguments;
        rdargs->RDA_Source.CS_Length = length + 1;
        rdargs->RDA_Source.CS_CurChr = 0;
        rdargs->RDA_Flags |= RDAF_NOPROMPT;
        parsed = ReadArgs((STRPTR)template, (LONG *)args, rdargs);
        if (!parsed)
        {
            result = REXX_BAD_ARGS;
            goto done;
        }
    }
    if (builtin < REXX_BUILTINS)
    {
        IPTR none = 0;
        result = rexx_builtin(data, obj, builtin, args ? args : &none);
    }
    else
        result = CallHookPkt(cmd->mc_Hook, obj, args);

done:
    if (parsed) FreeArgs(parsed);
    if (rdargs) FreeDosObject(DOS_RDARGS, rdargs);
    FreeVec(arguments);
    FreeVec(args);
    rmsg->rm_Result1 = result;
    if (result == 0 && (rmsg->rm_Action & RXFF_RESULT)
        && data->app_RexxString)
    {
        rmsg->rm_Result2 = (IPTR)CreateArgstring(data->app_RexxString,
            strlen(data->app_RexxString));
        if (!rmsg->rm_Result2)
            rmsg->rm_Result1 = RETURN_FAIL;
    }
    FreeVec(data->app_RexxString);
    data->app_RexxString = previous_string;
    data->app_RexxMsg = previous_msg;
}


static IPTR Application__MUIM_AboutMUI(struct IClass *cl, Object *obj,
    struct MUIP_Application_AboutMUI *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);

    if (!data->app_AboutWin)
    {
        data->app_AboutWin = AboutmuiObject,
            msg->refwindow ? MUIA_Window_RefWindow : TAG_IGNORE,
            (IPTR)msg->refwindow, MUIA_Window_LeftEdge,
            MUIV_Window_LeftEdge_Centered, MUIA_Window_TopEdge,
            MUIV_Window_TopEdge_Centered, MUIA_Aboutmui_Application, (IPTR)obj,
            End;
    }

    if (data->app_AboutWin)
        set(data->app_AboutWin, MUIA_Window_Open, TRUE);

    return 0;
}






static IPTR Application__MUIM_OpenConfigWindow(struct IClass *cl,
    Object *obj, struct MUIP_Application_OpenConfigWindow *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    struct TagItem tags[] = {
        {SYS_Asynch, TRUE},
        {SYS_Input, 0},
        {SYS_Output, 0},
        {NP_StackSize, AROS_STACKSIZE},
        {TAG_DONE}
    };
    char cmd[255];
    CONST_STRPTR prefs = "sys:prefs/OpenMUI";
    BPTR lock = Lock((STRPTR)prefs, SHARED_LOCK);

    if (lock)
        UnLock(lock);
    else
        prefs = "sys:prefs/Zune";

    snprintf(cmd, 255, "%s %s %ld", prefs,
        data->app_Base ? data->app_Base : (STRPTR) "", (long)obj);

    if (SystemTagList(cmd, tags) == -1)
    {
        return 0;
    }

    return 1;
}

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




static IPTR Application__MUIM_ShowHelp(struct IClass *cl, Object *obj,
    struct MUIP_Application_ShowHelp *msg)
{
    struct MUI_ApplicationData *data = INST_DATA(cl, obj);
    struct Library *AmigaGuideBase;
    struct NewAmigaGuide nag;
    APTR context;
    Object *window = msg->window;
    STRPTR name = msg->name ? (STRPTR)msg->name : data->app_HelpFile;

    if (!name || !*name) return FALSE;
    AmigaGuideBase = OpenLibrary("amigaguide.library", 39);
    if (!AmigaGuideBase) return FALSE;
    memset(&nag, 0, sizeof(nag));
    if (window == (Object *)-1)
    {
        struct MinList *children = NULL;
        Object *state;
        children = app_windows(data);
        window = NULL;
        if (children)
        {
            state = (Object *)children->mlh_Head;
            while ((window = NextObject(&state)))
                if (XGET(window, MUIA_Window_Open)) break;
        }
    }
    if (window)
        get(window, MUIA_Window_Screen, &nag.nag_Screen);
    nag.nag_Name = name;
    nag.nag_Node = msg->node;
    nag.nag_Line = msg->line;
    set(obj, MUIA_Application_Sleep, TRUE);
    context = OpenAmigaGuideA(&nag, NULL);
    if (context) CloseAmigaGuide(context);
    set(obj, MUIA_Application_Sleep, FALSE);
    CloseLibrary(AmigaGuideBase);
    return context != NULL;
}

/* MUIA_Application_Iconified changed: Workbench's icon and Tools menu
 * item, as the settings say (System page), or away with them */
void zune_app_iconify(struct MUI_ApplicationData *data, Object *obj,
    BOOL on)
{
    if (on)
    {
        struct ZunePrefsNew *prefs = data->app_GlobalInfo.mgi_Prefs;
        STRPTR appname = data->app_Title ? data->app_Title
            : (STRPTR) "Unnamed";
        struct DiskObject *dobj =
            (struct DiskObject *)XGET(obj, MUIA_Application_DiskObject);
        /* the defaults: ENV(ARC):SYS/def_AppIcon, def_MUI, def_Zune,
         * the default tool icon */
        static CONST_STRPTR const defaults[] =
            { "ENV:SYS/def_MUI", "ENV:SYS/def_Zune" };
        int i;

        if (prefs->iconification_show_menu && ZUNE_WB())
            data->app_AppMenu = AddAppMenuItemA(0L, 0L, appname,
                data->app_AppPort, NULL);
        if (!prefs->iconification_show_icon)
            return;
        if (dobj == NULL && ZUNE_ICON())
        {
            dobj = GetDefDiskObject(WBAPPICON);
            for (i = 0; !dobj && i < 2; i++)
                dobj = GetDiskObject((STRPTR)defaults[i]);
            if (!dobj)
                dobj = GetDefDiskObject(WBTOOL);
            data->app_DefaultDiskObject = dobj;
        }
        if (dobj == NULL || !ZUNE_WB())
            return;
        dobj->do_CurrentX = NO_ICON_POSITION;
        dobj->do_CurrentY = NO_ICON_POSITION;
        data->app_AppIcon = AddAppIconA(0L, 0L, appname, data->app_AppPort,
            BNULL, dobj, NULL);
        return;
    }
    if (data->app_AppMenu)
        RemoveAppMenuItem(data->app_AppMenu);
    data->app_AppMenu = NULL;
    if (data->app_AppIcon)
        RemoveAppIcon(data->app_AppIcon);
    data->app_AppIcon = NULL;
    if (data->app_DefaultDiskObject)
        FreeDiskObject(data->app_DefaultDiskObject);
    data->app_DefaultDiskObject = NULL;
}

/* A commodities message (the library replies it): Exchange's commands
 * (Appear and Disappear are the application's own business) and the
 * settings' iconification hotkey, then the application's hook */
void zune_app_cxmsg(struct MUI_ApplicationData *data, Object *obj, APTR m)
{
    CxMsg *msg = m;
    ULONG id;

    /* the module's copy of CxBase, as in zune_app_rexx() */
    if (!ZUNE_CX())
        return;
    id = CxMsgID(msg);

    if (CxMsgType(msg) == CXM_COMMAND)
    {
        if (id == CXCMD_DISABLE || id == CXCMD_ENABLE)
            set(obj, MUIA_Application_Active, id == CXCMD_ENABLE);
        else if (id == CXCMD_KILL)
            SetSignal(SIGBREAKF_CTRL_C, SIGBREAKF_CTRL_C);
    }
    else if (CxMsgType(msg) == CXM_IEVENT && id == ICONIFY_HOTKEY_ID)
        set(obj, MUIA_Application_Iconified, !data->app_Iconified);
    if (data->app_BrokerHook)
        CallHookPkt(data->app_BrokerHook, obj, msg);
}

/* An AppMessage: the AppIcon or menu item shows the iconified application
 * again (replied first, before the icon goes: TRUE), icons dropped on the
 * AppIcon go to MUIA_Application_DropObject, on an AppWindow to its
 * window; delivered at once, the library replies */
BOOL zune_app_appmsg(struct MUI_ApplicationData *data, Object *obj,
    struct AppMessage *appmsg)
{
    if (((appmsg->am_Type == AMTYPE_APPICON && appmsg->am_NumArgs == 0
        && appmsg->am_ArgList == NULL) || appmsg->am_Type == AMTYPE_APPMENUITEM)
        && XGET(obj, MUIA_Application_Iconified) == TRUE)
    {
        ReplyMsg((struct Message *)appmsg);
        set(obj, MUIA_Application_Iconified, FALSE);
        return TRUE;
    }
    if (appmsg->am_Type == AMTYPE_APPICON && appmsg->am_NumArgs > 0
        && data->app_DropObject)
        set(data->app_DropObject, MUIA_AppMessage, (IPTR)appmsg);
    else if (appmsg->am_Type == AMTYPE_APPWINDOW)
        set((Object *)appmsg->am_UserData, MUIA_AppMessage, (IPTR)appmsg);
    return FALSE;
}

/* MUIA_Application_Version made of Version_Number, Version_Date and
 * Version_Extra (AROS's attributes): "$VER: <title> <number> (<date>)
 * [<extra>]"; no memory: none */
void zune_app_version(struct MUI_ApplicationData *data)
{
    CONST_STRPTR parts[4] = { data->app_Title, data->app_Version_Number,
        data->app_Version_Date, data->app_Version_Extra };
    static const char *const fmt[] = { "$VER: %s", " %s", " (%s)", " [%s]" };
    ULONG length = 10, i;
    STRPTR result;

    for (i = 0; i < 4; i++)
        if (parts[i])
            length += strlen(parts[i]) + 3;
    if (!(result = AllocVec(length, MEMF_ANY)))
        return;
    result[0] = 0;
    for (i = 0; i < 4; i++)
        if (parts[i])
            snprintf(result + strlen(result), length - strlen(result),
                fmt[i], parts[i]);
    data->app_Version = result;
    data->app_VersionAllocated = TRUE;
}


/* The methods the library's dispatcher sends here */
IPTR zune_app_method(struct IClass *cl, Object *obj, Msg msg)
{
    switch (msg->MethodID)
    {
    case MUIM_Application_AboutMUI:
        return Application__MUIM_AboutMUI(cl, obj, (APTR)msg);
    case MUIM_Application_ShowHelp:
        return Application__MUIM_ShowHelp(cl, obj, (APTR)msg);
    case MUIM_Application_OpenConfigWindow:
        return Application__MUIM_OpenConfigWindow(cl, obj, (APTR)msg);
    case MUIM_Application_Execute:
    case MUIM_Application_Run:
        return Application__MUIM_Execute(cl, obj, msg);
    }
    return 0;
}
