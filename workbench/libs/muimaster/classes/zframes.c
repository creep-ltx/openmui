/* The round frame module (modules.h): frame types 5, 7, 8, A and B
 * (frame_round.c), which no default setting uses, and Area's background
 * under them (arearound.c); and the built-in images no window shows by
 * default (imspec_rare.c: drawer, disks, tape buttons). The library takes the drawing functions from
 * its table (zune_round_frames_get()). */

#define ZUNE_FRAMES_MODULE
#define zune_area_round_background zmod_area_round_background
#include "../frame_round.c"
#include "../arearound.c"
#include "../imspec_rare.c"
#include <clib/alib_protos.h>
#include "support.h"

static const struct ZuneFramesModule frames_module = {
    zune_round_frames, zmod_area_round_background, zune_rare_images
};

BOOPSI_DISPATCHER(IPTR, Zframes_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&frames_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zframes_desc =
{
    MUIC_Zframes,
    ROOTCLASS,
    0,
    (void *) Zframes_Dispatcher
};
