/* The round frame module (modules.h): frame types 5, 7, 8, A and B
 * (frame_round.c), which no default setting uses. The library takes the
 * drawing functions from its table (zune_round_frames_get()). */

#define ZUNE_FRAMES_MODULE
#include "../frame_round.c"
#include <clib/alib_protos.h>
#include "support.h"

static const struct ZuneFramesModule frames_module = {
    zune_round_frames
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
