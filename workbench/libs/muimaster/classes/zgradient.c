/* The gradient module (modules.h): gradient image specs drawn on a true
 * colour screen (imspec_gradientdraw.c). The library calls it through
 * modules.c, which fills with the start pen on other screens. */

#define zune_gradient_drawfast zmod_gradient_drawfast
#define zune_gradient_draw zmod_gradient_draw
#include "../imspec_gradientdraw.c"
#include <clib/alib_protos.h>
#include "mui.h"
#include "support.h"
#include "modules.h"

static const struct ZuneGradientModule gradient_module = {
    zmod_gradient_drawfast
};

BOOPSI_DISPATCHER(IPTR, Zgradient_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
    {
        /* cybergraphics.library may have been opened since the file
         * was loaded */
        ZUNE_MODULE_REFRESH();
        return (IPTR)&gradient_module;
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zgradient_desc =
{
    MUIC_Zgradient,
    ROOTCLASS,
    0,
    (void *) Zgradient_Dispatcher
};
