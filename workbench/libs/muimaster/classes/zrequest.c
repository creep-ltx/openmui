/* The request module (modules.h): MUI_AllocAslRequest(),
 * MUI_AslRequest() and MUI_FreeAslRequest() (requesters.c) and the
 * requester window of MUI_RequestA() (mui_request.c). The library's
 * entries call them through modules.c. */

#define ZUNE_REQUEST_MODULE
#define ZuneAllocAslRequest zmod_AllocAslRequest
#define ZuneAslRequest zmod_AslRequest
#define ZuneFreeAslRequest zmod_FreeAslRequest
#include "../requesters.c"
#define MUI_RequestA zmod_RequestA
#include "../mui_request.c"
#include <clib/alib_protos.h>
#include "mui.h"
#include "support.h"
#include "modules.h"

static const struct ZuneRequestModule request_module = {
    zmod_AllocAslRequest, zmod_AslRequest, zmod_FreeAslRequest,
    zmod_RequestA
};

BOOPSI_DISPATCHER(IPTR, Zrequest_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&request_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zrequest_desc =
{
    MUIC_Zrequest,
    ROOTCLASS,
    0,
    (void *) Zrequest_Dispatcher
};
