/*
    Copyright (C) 2002-2026, The AROS Development Team.
    All rights reserved.
*/

/* Area's context menu (MUIA_ContextMenu): the right button pressed over
 * the object builds and opens the menu, the pointer's moves follow it and
 * the release chooses. Only an object with a context menu gets here, and
 * the menu needs the menu module for its window anyway, so in the native
 * build this is part of it (modules.h, classes/zmenu.c): Area's event
 * handling calls zune_area_menu(). Other builds include it in
 * classes/area.c. */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "menu.h"
#include "modules.h"

extern struct Library *MUIMasterBase;

/* The object's window events: its handler node taken out (when it
 * listens to anything), given the new events and put back */
static void areamenu_events(Object *obj, struct MUI_AreaLive *live,
    ULONG events)
{
    if (live->ehn.ehn_Events)
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR)&live->ehn);
    live->ehn.ehn_Events = events;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR)&live->ehn);
}

/* imsg: MENUDOWN over an object with a context menu, MENUUP or a move
 * with the right button held while its menu is open. Returns
 * MUI_EventHandlerRC_Eat. The open menu is kept in the live block's
 * extension (made here: only objects with a context menu need it). */
IPTR zune_area_menu(Object *obj, struct IntuiMessage *imsg)
{
    struct MUI_AreaLive *live = muiAreaData(obj)->mad_Live;
    struct MUI_AreaLiveX *x = zune_area_livex(obj, live);

    if (!x)
        ;
    else if (imsg->Class == IDCMP_MOUSEMOVE)
        zune_mouse_update(x->ContextZMenu, 0);
    else if (imsg->Code == MENUDOWN)
    {
        Object *menuobj = (Object *)DoMethod(obj, MUIM_ContextMenuBuild,
            imsg->MouseX, imsg->MouseY);

        if (menuobj)
        {
            struct NewMenu *newmenu = NULL;

            /* The menu's items are not connected to their parents by
             * the family they are in */
            DoMethod(menuobj, MUIM_ConnectParent, (IPTR)obj);
            get(menuobj, MUIA_Menuitem_NewMenu, &newmenu);
            if (newmenu)
            {
                if (x->ContextZMenu)
                    zune_close_menu(x->ContextZMenu);
                x->ContextZMenu = zune_open_menu(_window(obj), newmenu);
            }
            areamenu_events(obj, live, live->ehn.ehn_Events
                | IDCMP_MOUSEMOVE);
        }
    }
    else
    {
        struct MenuItem *item = zune_leave_menu(x->ContextZMenu);

        areamenu_events(obj, live, IDCMP_MOUSEBUTTONS);
        if (item)
        {
            Object *itemobj = (Object *)GTMENUITEM_USERDATA(item);

            if (item->Flags & CHECKIT)
                set(itemobj, MUIA_Menuitem_Checked,
                    !!(item->Flags & CHECKED));
            set(itemobj, MUIA_Menuitem_Trigger, item);
            DoMethod(obj, MUIM_ContextMenuChoice, (IPTR)itemobj);
        }
        zune_close_menu(x->ContextZMenu);
        x->ContextZMenu = NULL;
    }
    return MUI_EventHandlerRC_Eat;
}
