/*
    Copyright  2002-2026, The AROS Development Team.
    All rights reserved.

*/

/* List's rarely used methods, split out of classes/list.c: in-place
 * editing (MUIM_List_Edit, MUIM_List_EndEdit, MUI 5) and drag and drop
 * (MUIM_DragReport, MUIM_DragDrop, MUIM_CreateDragImage). In the native
 * build they are the list module (modules.h, classes/zlist.c); other builds
 * include this file in classes/list.c. */

#include <string.h>
#include <stdlib.h>

#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxmacros.h>
#include <graphics/view.h>
#include <devices/rawkeycodes.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/layers.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

/* #define MYDEBUG 1 */
#include "debug.h"
#include "mui.h"
#include "muimaster_intern.h"
#include "support.h"
#include "imspec.h"
#include "textengine.h"
#include "listimage.h"
#include "prefs.h"

#include "classes/list_private.h"
#include "frame.h"

extern struct Library *MUIMasterBase;

/****i* List.mui/MUIM_DragReport *********************************************
*
*   NAME
*       MUIM_DragReport
*
******************************************************************************
*
*/

IPTR zune_list_drag_report(struct IClass *cl, Object *obj,
    struct MUIP_DragReport *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    struct MUI_List_TestPos_Result pos;
    struct RastPort *rp = _rp(obj);
    LONG n, y;
    UWORD old_pattern;

    /* Choose new drop mark position */

    DoMethod(obj, MUIM_List_TestPos, msg->x, msg->y, (IPTR) &pos);
    if (pos.entry != -1)
    {
        n = pos.entry;
        if (pos.yoffset > 0)
            n++;
    }
    else if ((pos.flags & MUI_LPR_ABOVE) != 0)
        n = data->entries_first;
    else
    {
        n = MIN(data->entries_visible, data->entries_num)
            - data->entries_first;
    }

    /* Clear old drop mark */

    if ((data->flags & LIST_SHOWDROPMARKS) != 0)
    {
        y = data->entries_top_pixel + (n - data->entries_first)
            * data->entry_maxheight;
        if (y != data->drop_mark_y)
        {
            DoMethod(obj, MUIM_DrawBackground, _mleft(data->area),
                data->drop_mark_y, _mwidth(data->area), 1, 0, 0, 0);

            /* Draw new drop mark and store its position */

            /* Restore draw mode and background pen afterwards */
            UBYTE oldDrMd = rp->DrawMode, oldBPen = rp->BgPen;
            SetABPenDrMd(rp, _pens(obj)[MPEN_SHINE], _pens(obj)[MPEN_SHADOW],
                JAM2);
            old_pattern = rp->LinePtrn;
            SetDrPt(rp, 0xF0F0);
            Move(rp, _mleft(data->area), y);
            Draw(rp, _mright(data->area), y);
            SetDrPt(rp, old_pattern);
            SetBPen(rp, oldBPen);
            SetDrMd(rp, oldDrMd);
            data->drop_mark_y = y;
        }
    }

    return TRUE;
}


/****i* List.mui/MUIM_DragDrop ***********************************************
*
*   NAME
*       MUIM_DragDrop
*
******************************************************************************
*
*/

IPTR zune_list_drag_drop(struct IClass *cl, Object *obj,
    struct MUIP_DragDrop *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    struct MUI_List_TestPos_Result pos;
    LONG n;

    /* Find drop position */

    DoMethod(obj, MUIM_List_TestPos, msg->x, msg->y, (IPTR) &pos);
    if (pos.entry != -1)
    {
        /* Change drop position when coords move past centre of entry, not
         * entry boundary */

        n = pos.entry;
        if (pos.yoffset > 0)
            n++;
    }
    else if ((pos.flags & MUI_LPR_ABOVE) != 0)
        n = 0;
    else
        n = data->entries_num;

    data->drop_mark = n;

    if (data->flags & LIST_DRAGSORTABLE)
    {
        /* Ensure that dropped entry will be positioned between the two
         * entries that are above and below the drop mark, rather than
         * strictly at the numeric index shown */

        if (n > data->entries_active)
            n--;

        DoMethod(obj, MUIM_List_Move, MUIV_List_Move_Active, n);
        SET(obj, MUIA_List_Active, n);
    }

    return TRUE;
}


/****i* List.mui/MUIM_CreateDragImage ****************************************
*
*   NAME
*       MUIM_CreateDragImage
*
******************************************************************************
*
*/

IPTR zune_list_create_drag_image(struct IClass *cl, Object *obj,
    struct MUIP_CreateDragImage *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    BOOL success = TRUE;
    struct MUI_List_TestPos_Result pos;
    WORD width, height, left, top;
    struct MUI_DragImage *img = NULL;
    const struct ZuneFrameGfx *zframe;
    LONG depth;

    /* If entries aren't draggable, allow the list as a whole to be */
    if (data->drag_type == MUIV_Listview_DragType_None)
        return DoSuperMethodA(cl, obj, (Msg)msg);

    /* Get info on dragged entry */
    DoMethod(obj, MUIM_List_TestPos, _left(data->area) + msg->touchx,
        _top(data->area) + msg->touchy, (IPTR) &pos);
    if (pos.entry == -1)
        success = FALSE;

    if (success)
    {
        /* Get boundaries of entry */
        width = _mwidth(data->area);
        height = data->entry_maxheight;
        left = _mleft(data->area);
        top = _top(data->area) + msg->touchy
            - (pos.yoffset + data->entry_maxheight / 2);

        /* Allocate drag image structure */
        img = (struct MUI_DragImage *)
            AllocVec(sizeof(struct MUI_DragImage), MEMF_CLEAR);
        if (img == NULL)
            success = FALSE;
    }

    if (success)
    {
        /* Get drag frame */
        zframe = zune_zframe_get(obj,
            &muiGlobalInfo(obj)->mgi_Prefs->frames[MUIV_Frame_Drag]);

        /* Allocate drag image buffer */
        img->width = width + zframe->ileft + zframe->iright;
        img->height = height + zframe->itop + zframe->ibottom;
        depth = GetBitMapAttr(_screen(obj)->RastPort.BitMap, BMA_DEPTH);
        img->bm = AllocBitMap(img->width, img->height, depth, BMF_MINPLANES,
            _screen(obj)->RastPort.BitMap);

        if (img->bm != NULL)
        {
            /* Render entry */
            struct RastPort temprp;
            InitRastPort(&temprp);
            temprp.BitMap = img->bm;
            ClipBlit(_rp(obj), left, top, &temprp,
                zframe->ileft, zframe->itop, width, height,
                0xc0);

            /* Render frame */
            struct RastPort *rp_save = muiRenderInfo(obj)->mri_RastPort;
            muiRenderInfo(obj)->mri_RastPort = &temprp;
            zframe->draw(zframe->customframe, muiRenderInfo(obj), 0, 0,
                img->width, img->height, 0, 0, img->width, img->height);
            muiRenderInfo(obj)->mri_RastPort = rp_save;
        }

        /* Ensure drag point matches where user clicked */
        img->touchx = msg->touchx - zframe->ileft + _addleft(obj);
        img->touchy = -(pos.yoffset + data->entry_maxheight / 2)
            - zframe->itop;
        img->flags = 0;
    }

    return (IPTR) img;
}

void zune_list_layout_editor(Object *obj, struct MUI_ListData *data)
{
    LONG left = _mleft(data->area) - data->horiz_first;
    LONG top = data->entries_top_pixel + (data->edit_row - data->entries_first) * data->entry_maxheight;
    if (!data->edit_object) return;
    for (LONG column = 0; column < data->edit_column; column++)
        if (!ColumnHidden(data, column))
            left += ColumnWidth(data, column) + data->ci[column].delta +
                (data->ci[column].bar ? BAR_WIDTH : 0);
    LONG width = MIN(ColumnWidth(data, data->edit_column),
        _mright(data->area) - left + 1);
    MUI_LayoutObj(data->edit_object, left, top, MAX(1, width),
        MAX(data->entry_maxheight, _minheight(data->edit_object)), 0);
}

IPTR zune_list_edit(struct IClass *cl, Object *obj, LONG row, LONG column)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    Object *editor;
    struct MUI_MinMax minmax = {0};
    if (row == MUIV_List_Edit_Active) row = data->entries_active;
    if (!data->editable || !(_flags(obj) & MADF_CANDRAW) || row < 0 ||
        row >= data->entries_num || column < 0 || column >= data->columns) return FALSE;
    if (data->ci[column].hidden)
        set(obj, MUIA_List_ShowColumn, data->ci[column].order_id);
    if (data->edit_object && !zune_list_end_edit(cl, obj, MUIV_List_EndEdit_Done, FALSE)) return FALSE;
    editor = (Object *)DoMethod(obj, MUIM_List_CreateEditObject, row, column, data->entries[row]->data);
    if (!editor) return FALSE;
    DoMethod(editor, MUIM_ConnectParent, obj);
    if (!DoMethod(editor, MUIM_Setup, muiRenderInfo(obj)))
    {
        DoMethod(editor, MUIM_DisconnectParent); MUI_DisposeObject(editor);
        return FALSE;
    }
    struct MUIP_AskMinMax ask = {MUIM_AskMinMax, &minmax};
    DoMethodA(editor, (Msg)&ask);
    if (ask.MinMaxInfo) __area_finish_minmax(editor, ask.MinMaxInfo);
    data->edit_object = editor; data->edit_row = row; data->edit_column = column;
    data->edit_entry = data->entries[row]->data;
    data->edit_focus = (Object *)XGET(_win(obj), MUIA_Window_ActiveObject);
    DoMethod(obj, MUIM_List_Jump, row);
    zune_list_layout_editor(obj, data);
    DoShowMethod(editor); MUI_Redraw(editor, MADF_DRAWOBJECT);
    DoMethod(editor, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
        _app(obj), 5, MUIM_Application_PushMethod, obj, 2,
        MUIM_List_EndEdit, MUIV_List_EndEdit_Done);
    set(_win(obj), MUIA_Window_ActiveObject, editor);
    return TRUE;
}

IPTR zune_list_end_edit(struct IClass *cl, Object *obj, ULONG mode, BOOL force)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    Object *editor = data->edit_object;
    LONG row = data->edit_row, column = data->edit_column;
    if (!editor) return TRUE;
    if (data->ending_edit) return FALSE;
    data->ending_edit = TRUE;
    IPTR accepted = DoMethod(obj, MUIM_List_EditDone, row, column,
        mode == MUIV_List_EndEdit_Abort ? NULL : data->edit_entry, editor);
    if (!accepted && !force) { data->ending_edit = FALSE; return FALSE; }
    set(_win(obj), MUIA_Window_ActiveObject, data->edit_focus);
    data->edit_object = NULL;
    DoHideMethod(editor); DoMethod(editor, MUIM_Cleanup);
    DoMethod(editor, MUIM_DisconnectParent); MUI_DisposeObject(editor);
    data->ending_edit = FALSE;
    DoMethod(obj, MUIM_List_Redraw, MUIV_List_Redraw_All);
    if (!force && mode >= MUIV_List_EndEdit_Prev && mode <= MUIV_List_EndEdit_Down)
    {
        if (mode == MUIV_List_EndEdit_Prev) column--;
        if (mode == MUIV_List_EndEdit_Next) column++;
        if (mode == MUIV_List_EndEdit_Up) row--;
        if (mode == MUIV_List_EndEdit_Down) row++;
        if (column < 0) { column = data->columns - 1; row--; }
        if (column >= data->columns) { column = 0; row++; }
        zune_list_edit(cl, obj, row, column);
    }
    return TRUE;
}

