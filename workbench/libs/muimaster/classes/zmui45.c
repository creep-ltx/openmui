/* The MUI 4/5 module (modules.h): attributes of the core classes that
 * only MUI 4/5 programs use (mui45.c). The library calls them through
 * modules.c under the names modules.h gives them. */

#define zune_area_mui45 zmod_area_mui45
#include "../mui45.c"

static const struct ZuneMui45Module mui45_module = {
    zmod_area_mui45
};

BOOPSI_DISPATCHER(IPTR, Zmui45_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&mui45_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zmui45_desc =
{
    MUIC_Zmui45,
    ROOTCLASS,
    0,
    (void *) Zmui45_Dispatcher
};
