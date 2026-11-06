/* The list module (modules.h): List's in-place editing (MUIM_List_Edit,
 * MUI 5) and its drag and drop methods (listx.c). The library calls them
 * through modules.c under the names classes/list_private.h gives them. */

#define zune_list_edit zmod_list_edit
#define zune_list_end_edit zmod_list_end_edit
#define zune_list_layout_editor zmod_list_layout_editor
#define zune_list_drag_report zmod_list_drag_report
#define zune_list_drag_drop zmod_list_drag_drop
#define zune_list_create_drag_image zmod_list_create_drag_image
#include "../listx.c"
#include "modules.h"

static const struct ZuneListModule list_module = {
    zmod_list_edit, zmod_list_end_edit, zmod_list_layout_editor,
    zmod_list_drag_report, zmod_list_drag_drop, zmod_list_create_drag_image
};

BOOPSI_DISPATCHER(IPTR, Zlist_Dispatcher, cl, obj, msg)
{
    if (msg->MethodID == MUIM_ZuneModule_Get)
        return (IPTR)&list_module;
    return DoSuperMethodA(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

const struct __MUIBuiltinClass _MUI_Zlist_desc =
{
    MUIC_Zlist,
    ROOTCLASS,
    0,
    (void *) Zlist_Dispatcher
};
