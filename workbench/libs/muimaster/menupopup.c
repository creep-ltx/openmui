/*
    Copyright  2002-2026, The AROS Development Team.
    All rights reserved.

*/

/* MUIM_Menustrip_Popup: a menu strip shown as a Zune-drawn pop-up menu at
 * an object, split out of classes/menuitem.c. In the native build it is
 * part of the menu module (modules.h, classes/zmenu.c), which draws the
 * menu; other builds include it in classes/menuitem.c. */

#include <exec/types.h>
#include <libraries/gadtools.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "menu.h"
#include "modules.h"

extern struct Library *MUIMasterBase;

struct PopupState
{
    struct ZMenu *menu;
    struct MenuItem selected;
    Object *choice;
    BOOL done;
};

/* A stack-local dispatcher target keeps modal input state out of the
 * application object's lifetime. It is removed before any notifications. */
BOOPSI_DISPATCHER(IPTR, PopupEvent, cl, obj, rawmsg)
{
    struct MUIP_HandleEvent *msg = (APTR)rawmsg;
    struct PopupState *state = (APTR)cl->cl_UserData;
    struct IntuiMessage *event = msg->imsg;
    if (!event) return 0;
    if (event->Class == IDCMP_MOUSEMOVE)
        zune_mouse_update(state->menu, 0);
    else if (event->Class == IDCMP_MOUSEBUTTONS &&
        (event->Code == SELECTUP || event->Code == MENUUP))
    {
        struct MenuItem *item = zune_leave_menu(state->menu);
        if (item)
        {
            state->selected = *item;
            state->choice = (Object *)GTMENUITEM_USERDATA(item);
        }
        state->done = TRUE;
    }
    else if (event->Class == IDCMP_RAWKEY && event->Code == 0x45)
        state->done = TRUE;
    return MUI_EventHandlerRC_Eat;
}
BOOPSI_DISPATCHER_END

IPTR zune_menustrip_popup(Object *obj, struct MUIP_Menustrip_Popup *msg)
{
    struct PopupState state = {0};
    struct IClass handler = {0};
    struct MUI_EventHandlerNode ehn = {0};
    struct ZuneWindowGuard guard = {0};
    Object *parent = msg->parent;
    Object *win, *app;
    struct NewMenu *newmenu = NULL;
    ULONG signals = 0;
    IPTR result = 0;
    if (!parent || !(_flags(parent) & MADF_CANDRAW)) return 0;
    win = _win(parent);
    app = _app(parent);
    /* Install the guard before the application callback, which can close
     * or dispose the parent window. No event handler is needed yet. */
    ZuneWatchWindow(win, &guard);
    DoMethod(obj, MUIM_Menustrip_WillOpen);
    if (guard.closed) return 0;
    ZuneUnwatchWindow(win, &guard);
    get(obj, MUIA_Menuitem_NewMenu, &newmenu);
    if (!newmenu) return 0;
    state.menu = zune_open_menu_at(_window(parent), newmenu,
        _window(parent)->LeftEdge + _left(parent) + msg->x,
        _window(parent)->TopEdge + _top(parent) + msg->y);
    if (!state.menu) return 0;
    handler.cl_Dispatcher.h_Entry = (HOOKFUNC)PopupEvent;
    handler.cl_UserData = (IPTR)&state;
    ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY;
    ehn.ehn_Priority = 127;
    ehn.ehn_Flags = MUI_EHF_GUIMODE;
    ehn.ehn_Object = parent;
    ehn.ehn_Class = &handler;
    DoMethod(win, MUIM_Window_AddEventHandler, &ehn);
    guard.handler = &ehn;
    ZuneWatchWindow(win, &guard);
    while (!state.done)
    {
        DoMethod(app, MUIM_Application_NewInput, &signals);
        if (guard.closed) break;
        if (!state.done)
        {
            struct MenuItem *selected = NULL;
            int picked = zune_poll_menu(state.menu, &selected);
            if (picked)
            {
                state.done = TRUE;
                if (selected)
                {
                    state.selected = *selected;
                    state.choice = (Object *)GTMENUITEM_USERDATA(selected);
                }
            }
            if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) break;
            if (!state.done) Delay(1);
        }
    }
    if (!guard.closed)
    {
        ZuneUnwatchWindow(win, &guard);
        DoMethod(win, MUIM_Window_RemEventHandler, &ehn);
    }
    zune_close_menu(state.menu);
    if (state.choice && !guard.closed)
    {
        get(state.choice, MUIA_UserData, &result);
        if (state.selected.Flags & CHECKIT)
            set(state.choice, MUIA_Menuitem_Checked,
                !!(state.selected.Flags & CHECKED));
        set(state.choice, MUIA_Menuitem_Trigger, &state.selected);
    }
    return result;
}

