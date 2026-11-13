/* The drag and drop module (modules.h): the drag image a window moves
 * over the screen while an object is dragged (dragndrop.c), and the
 * window's handling of a drag (dragwin.c) and Area's drag methods
 * (areadrag.c). The library
 * calls it through modules.c under the names dragndrop.h gives them. */

#define CreateDragNDropA zmod_CreateDragNDropA
#define DeleteDragNDrop zmod_DeleteDragNDrop
#define PrepareDragNDrop zmod_PrepareDragNDrop
#define FinishDragNDrop zmod_FinishDragNDrop
#define DrawDragNDrop zmod_DrawDragNDrop
#define UndrawDragNDrop zmod_UndrawDragNDrop
#define CreateBitMapNodeA zmod_CreateBitMapNodeA
#define CreateBitMapNode zmod_CreateBitMapNode
#define DeleteBitMapNode zmod_DeleteBitMapNode
#define AttachBitMapNode zmod_AttachBitMapNode
#define DetachBitMapNode zmod_DetachBitMapNode
#define zune_drag_begin zmod_drag_begin
#define zune_drag_event zmod_drag_event
#define zune_area_drag zmod_area_drag
#define zune_area_drag_motion zmod_area_drag_motion
#include "../dragndrop.c"
#include "../dragwin.c"
#include "../areadrag.c"
#include <clib/alib_protos.h>
#include "mui.h"
#include "support.h"
#include "modules.h"

static const struct ZuneDragDropModule dragdrop_module = {
    zmod_CreateDragNDropA, zmod_DeleteDragNDrop, zmod_PrepareDragNDrop,
    zmod_FinishDragNDrop, zmod_DrawDragNDrop, zmod_UndrawDragNDrop,
    zmod_CreateBitMapNodeA, zmod_DeleteBitMapNode, zmod_AttachBitMapNode,
    zmod_DetachBitMapNode, zmod_drag_begin, zmod_drag_event, zmod_area_drag,
    zmod_area_drag_motion
};

BOOPSI_DISPATCHER(IPTR, Zdragdrop_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
    {
        /* cybergraphics.library may have been opened since the file
         * was loaded */
        ZUNE_MODULE_REFRESH();
        return (IPTR)&dragdrop_module;
    }
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zdragdrop_desc =
{
    MUIC_Zdragdrop,
    ROOTCLASS,
    0,
    (void *) Zdragdrop_Dispatcher
};
