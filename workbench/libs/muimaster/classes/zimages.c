/* The image module (modules.h): the rarely used vector images, MUII_Drawer
 * to MUII_TapeDown (imspec_vector_rare.c). The library takes their
 * drawing functions from its table (zune_rare_vectors_get()). */

#define ZUNE_IMAGES_MODULE
#include "../imspec_vector_rare.c"
#include <clib/alib_protos.h>

static const struct ZuneImagesModule images_module = {
    zune_rare_vectors
};

BOOPSI_DISPATCHER(IPTR, Zimages_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&images_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zimages_desc =
{
    MUIC_Zimages,
    ROOTCLASS,
    0,
    (void *) Zimages_Dispatcher
};
