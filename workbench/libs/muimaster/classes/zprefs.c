/* The prefs module (modules.h): Configdata's setters (configset.c), used
 * only by the prefs program (SetString stays in the library). The
 * library calls them through modules.c under the name modules.h gives. */

#define zune_configdata_set zmod_configdata_set
#include "../configset.c"

static const struct ZunePrefsModule prefs_module = {
    zmod_configdata_set
};

BOOPSI_DISPATCHER(IPTR, Zprefs_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&prefs_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zprefs_desc =
{
    MUIC_Zprefs,
    ROOTCLASS,
    0,
    (void *) Zprefs_Dispatcher
};
