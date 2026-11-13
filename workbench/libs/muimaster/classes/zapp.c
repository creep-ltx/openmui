/* The application module (modules.h): Application's rarely used parts
 * (appx.c). The library calls them through modules.c under the names
 * modules.h gives them. */

#define zune_app_method zmod_app_method
#define zune_app_rexx zmod_app_rexx
#define zune_app_iconify zmod_app_iconify
#define zune_app_cxmsg zmod_app_cxmsg
#define zune_app_appmsg zmod_app_appmsg
#define zune_app_version zmod_app_version
#define zune_app_usedclasses zmod_app_usedclasses
#include "../appx.c"

static const struct ZuneAppModule app_module = {
    zmod_app_method, zmod_app_rexx, zmod_app_iconify, (APTR)zmod_app_cxmsg,
    zmod_app_appmsg, zmod_app_version, zmod_app_usedclasses
};

BOOPSI_DISPATCHER(IPTR, Zapp_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&app_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zapp_desc =
{
    MUIC_Zapp,
    ROOTCLASS,
    0,
    (void *) Zapp_Dispatcher
};
