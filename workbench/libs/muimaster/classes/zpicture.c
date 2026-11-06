/* The picture module (modules.h): datatype pictures for brush and bitmap
 * image specs and menu images, and custom frames (the module half of
 * datatypescache.c, and frame_tile.c). The library calls them through modules.c under the
 * names datatypescache.h gives them. */

#define ZUNE_PICTURE_MODULE
#define dt_load_picture zmod_load_picture
#define dt_dispose_picture zmod_dispose_picture
#define dt_put_on_rastport zmod_put_on_rastport
#define dt_put_mim_on_rastport zmod_put_mim_on_rastport
#define dt_put_on_rastport_tiled zmod_put_on_rastport_tiled
#define load_custom_frame zmod_load_custom_frame
#define dispose_custom_frame zmod_dispose_custom_frame
#define dt_do_frame_rects zmod_do_frame_rects
#include "../datatypescache.c"
#include "../frame_tile.c"
#include <clib/alib_protos.h>
#include "mui.h"
#include "support.h"
#include "modules.h"

static const struct ZunePictureModule picture_module = {
    zmod_load_picture, zmod_dispose_picture, zmod_put_on_rastport,
    zmod_put_mim_on_rastport, zmod_put_on_rastport_tiled,
    zmod_load_custom_frame, zmod_dispose_custom_frame, zmod_do_frame_rects
};

BOOPSI_DISPATCHER(IPTR, Zpicture_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
    {
        /* the custom frames draw true colour pictures with
         * cybergraphics.library, which may have been opened since the
         * file was loaded */
        ZUNE_MODULE_REFRESH();
        return (IPTR)&picture_module;
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zpicture_desc =
{
    MUIC_Zpicture,
    ROOTCLASS,
    0,
    (void *) Zpicture_Dispatcher
};
