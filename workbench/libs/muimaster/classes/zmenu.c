/* The menu module (modules.h): Zune's own menus, drawn for context menus
 * and MUIM_Menustrip_Popup (menu.c). The library calls them through
 * modules.c under the names menu.h gives them. */

#define zune_open_menu zmod_open_menu
#define zune_open_menu_at zmod_open_menu_at
#define zune_mouse_update zmod_mouse_update
#define zune_poll_menu zmod_poll_menu
#define zune_leave_menu zmod_leave_menu
#define zune_close_menu zmod_close_menu
#define zune_get_menu_pointer zmod_get_menu_pointer
#define zune_menustrip_popup zmod_menustrip_popup
#define zune_area_menu zmod_area_menu
#include "../menu.c"
#include "../menupopup.c"
#include "../areamenu.c"
#include <clib/alib_protos.h>
#include "mui.h"
#include "support.h"
#include "modules.h"

#include <proto/graphics.h>
#include <proto/layers.h>

/* zune_popup_rollout (modules.h): MUI 3.8 rolls a cycle popup out from
 * the top, a band of rows a frame; each band drawn once, clipped to it */
static BOOL zmod_popup_rollout(struct Window *win, LONG frames,
    void (*draw)(Object *, APTR), Object *obj, APTR data)
{
    WORD shown = 0, upto, step;
    struct Rectangle rows;
    struct Region *band, *oldclip;

    rows.MinX = 0;
    rows.MaxX = win->Width - 1;
    for (step = 1; shown < win->Height; step++)
    {
        upto = win->Height * step / frames;
        if (!(band = NewRegion()))
            upto = win->Height;
        else
        {
            rows.MinY = shown;
            rows.MaxY = upto - 1;
            OrRectRegion(band, &rows);
            if (shown)
                WaitTOF();
            oldclip = InstallClipRegion(win->WLayer, band);
        }
        if (upto > shown)
            draw(obj, data);
        if (band)
        {
            InstallClipRegion(win->WLayer, oldclip);
            DisposeRegion(band);
        }
        shown = upto;
    }
    return TRUE;
}

static const struct ZuneMenuModule menu_module = {
    zmod_open_menu_at, zmod_mouse_update, zmod_poll_menu, zmod_leave_menu,
    zmod_close_menu, zmod_menustrip_popup, zmod_popup_rollout, zmod_area_menu
};

BOOPSI_DISPATCHER(IPTR, Zmenu_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&menu_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zmenu_desc =
{
    MUIC_Zmenu,
    ROOTCLASS,
    0,
    (void *) Zmenu_Dispatcher
};
