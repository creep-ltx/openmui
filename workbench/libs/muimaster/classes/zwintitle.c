/* The title gadget module (modules.h): the extra title bar gadgets
 * (Popup, Prefs, Snapshot, Iconify) and the Popup gadget's window menu
 * (wintitle.c). Off by default; the library calls it through modules.c
 * under the names modules.h gives them. */

#define zune_wintitle_create zmod_wintitle_create
#define zune_wintitle_menu zmod_wintitle_menu
#include "../wintitle.c"

static const struct ZuneWintitleModule wintitle_module = {
    zmod_wintitle_create, zmod_wintitle_menu
};

BOOPSI_DISPATCHER(IPTR, Zwintitle_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&wintitle_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zwintitle_desc =
{
    MUIC_Zwintitle,
    ROOTCLASS,
    0,
    (void *) Zwintitle_Dispatcher
};
