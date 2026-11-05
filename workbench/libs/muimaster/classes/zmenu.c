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
#include "../menu.c"
#include <clib/alib_protos.h>
#include "mui.h"
#include "support.h"
#include "modules.h"

static const struct ZuneMenuModule menu_module = {
    zmod_open_menu_at, zmod_mouse_update, zmod_poll_menu, zmod_leave_menu,
    zmod_close_menu
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
