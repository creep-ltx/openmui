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
    struct ColumnInfo *ci = data->ci, *end = ci + data->edit_column;
    if (!data->edit_object) return;
    for (; ci < end; ci++)
        if (!ci->hidden && !ci->layout_hidden)
            left += (data->format_layout ? ci->layout_width : ci->entries_width)
                + ci->delta + (ci->bar ? BAR_WIDTH : 0);
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
    editor = (Object *)DoMethod(obj, MUIM_List_CreateEditObject, row, column, data->entries[row].data);
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
    data->edit_entry = data->entries[row].data;
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
    /* data in a register of its own: GCC kept the instance offset live
     * to address data->columns from obj (one register more) */
    __asm__ ("" : "+a" (data));
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


/* The sort mark of a SORTABLE column's title (MUI 5 List1 demo): an
 * outlined triangle at the right end of the sort column's title cell
 * (starting at x1, row y of height h), pointing up for ascending, down
 * for descending, in the text pen. */
void zune_list_draw_sort_mark(Object *obj, struct MUI_ListData *data,
    LONG x1, LONG y, LONG h)
{
    LONG th = (_font(obj)->tf_YSize + 1) / 2;
    LONG left = MIN(x1 + ColumnWidth(data, data->sort_column),
        _mright(data->area)) - (th * 2 - 1) - 2;
    LONG base, tip;
    struct RastPort *rp;

    if (th < 3 || left < x1)
        return;
    /* the base line, then to the tip and back: the tip down for
     * descending */
    base = y + (h - th) / 2, tip = base + th - 1;
    if (!data->ci[data->sort_column].descending)
        tip = base, base += th - 1;
    rp = _rp(obj);
    SetAPen(rp, muiRenderInfo(obj)->mri_Pens[MPEN_TEXT]);
    Move(rp, left, base);
    Draw(rp, left + th * 2 - 2, base);
    Draw(rp, left + th - 1, tip);
    Draw(rp, left, base);
}


/* MUIA_List_ColumnOrder (MUI 5): the columns' contents in another order */
BOOL zune_list_column_order(struct MUI_ListData *data, const UBYTE *order)
{
    struct ColumnInfo *columns;
    if (!order || data->columns > 256) return FALSE;
    for (LONG i = 0; i < data->columns; i++)
    {
        if (order[i] >= data->columns) return FALSE;
        for (LONG j = 0; j < i; j++)
            if (order[i] == order[j]) return FALSE;
    }
    columns = AllocVec(data->columns * sizeof(*columns), MEMF_ANY);
    if (!columns) return FALSE;
    data->mapped_strings = List_MappedArea(data);  /* in the format block */
    memcpy(columns, data->ci, data->columns * sizeof(*columns));
    for (LONG i = 0; i < data->columns; i++)
        for (LONG j = 0; j < data->columns; j++)
            if (columns[j].order_id == order[i])
            {
                /* The format describes physical column widths. Reorder
                 * the logical contents and their display properties. */
                data->ci[i].colno = columns[j].colno;
                data->ci[i].order_id = columns[j].order_id;
                data->ci[i].preparse = columns[j].preparse;
                data->ci[i].hidden = columns[j].hidden;
                data->ci[i].is_object = columns[j].is_object;
                data->ci[i].sortable = columns[j].sortable;
                data->ci[i].descending = columns[j].descending;
                data->ci[i].elide = columns[j].elide;
                break;
            }
    FreeVec(columns);
    return TRUE;
}

/* ISOBJECT cells (MUI 5) borrow application objects. Balance temporary
 * setup and parent connections before the entry's destructor can release
 * them. This also permits an object to be reused in several rows without
 * duplicate cleanup/disposal ownership. */
static ULONG BeginCellObject(Object *list, Object *cell, struct MUI_MinMax *mm)
{
    ULONG state = 4;
    if (!cell) return 0;
    if (_parent(cell) && _parent(cell) != list) return 0;
    if (!_parent(cell))
    { DoMethod(cell, MUIM_ConnectParent, list); state |= 1; }
    if (!(_flags(cell) & MADF_SETUP))
    {
        if (!DoSetupMethod(cell, muiRenderInfo(list)))
        { if (state & 1) DoMethod(cell, MUIM_DisconnectParent); return 0; }
        state |= 2;
    }
    struct MUIP_AskMinMax ask = {MUIM_AskMinMax, mm};
    DoMethodA(cell, (Msg)&ask);
    if (ask.MinMaxInfo)
    {
        __area_finish_minmax(cell, ask.MinMaxInfo);
        *mm = *ask.MinMaxInfo;
    }
    return state;
}
static void EndCellObject(Object *cell, ULONG state)
{
    if (state & 2) DoMethod(cell, MUIM_Cleanup);
    if (state & 1) DoMethod(cell, MUIM_DisconnectParent);
}

/* An ISOBJECT cell's size (column j of entry, whose strings are made):
 * 1 when the row height or the column width grew */
LONG zune_list_cell_dims(Object *obj, struct MUI_ListData *data,
    struct ListEntry *entry, LONG j)
{
    Object *cell = (Object *)data->strings[j];
    struct MUI_MinMax mm = {0};
    ULONG state = BeginCellObject(obj, cell, &mm);
    LONG ret = 0;
    if (state)
    {
        LONG height = mm.DefHeight + data->prefs_linespacing;
        if (height > entry->height) { entry->height = height; ret = 1; }
        if (mm.DefWidth > data->ci[j].entries_width)
        { data->ci[j].entries_width = mm.DefWidth; ret = 1; }
        EndCellObject(cell, state);
    }
    return ret;
}

void zune_list_draw_cell(Object *list, Object *cell, LONG left, LONG top,
    LONG width, LONG height)
{
    struct MUI_MinMax mm = {0};
    ULONG state = BeginCellObject(list, cell, &mm);
    if (!state) return;
    BOOL shown = !!(_flags(cell) & MADF_CANDRAW);
    LONG w = MIN(width, mm.MaxWidth), h = MIN(height, mm.MaxHeight);
    if (w > 0 && h > 0)
    {
        APTR clip = MUI_AddClipping(muiRenderInfo(list), left, top, width, height);
        MUI_LayoutObj(cell, left, top + (height - h) / 2, w, h, 0);
        if (!shown) DoShowMethod(cell);
        MUI_Redraw(cell, MADF_DRAWOBJECT | MADF_DRAWFRAME);
        if (!shown) DoHideMethod(cell);
        if (clip != (APTR)-1) MUI_RemoveClipping(muiRenderInfo(list), clip);
    }
    EndCellObject(cell, state);
}

/* MUI 3.9's horizontal scroller below the entry area. A list has one only
 * in a program that gets MUI 4/5 or asks for it with the MUI 4 attribute
 * MUIA_List_HScrollerVisibility (list.c, MUIM_Setup); list.c calls these
 * only when it has one. */

/* The columns' total width, which the entries need: the last column
 * ends with its text, so its spacing does not count (the scroller's knob
 * matches MUI 3.9 on xSysInfo's register pages) */
static LONG List_ContentWidth(struct MUI_ListData *data)
{
    zune_list_layout_columns(data);
    LONG width = 0, last = -1;
    for (LONG j = 0; j < data->columns; j++)
        if (!ColumnHidden(data, j))
        {
            width += ColumnWidth(data, j) + data->ci[j].delta +
                (data->ci[j].bar ? BAR_WIDTH : 0);
            last = j;
        }
    if (last >= 0) width -= data->ci[last].delta;
    return width;
}

/* Whether the entry area, laid out at its full height, needs the
 * horizontal scroller: MUI 3.9 then takes the scroller's height off the
 * framed area and puts the scroller below it, as wide as the frame */
static BOOL List_NeedHoriz(struct MUI_ListData *data)
{
    if (!data->horiz || !(_flags(data->area) & MADF_SETUP))
        return FALSE;
    if (data->hscroll_visibility == MUIV_List_HScrollerVisibility_Never) return FALSE;
    return (data->hscroll_visibility == MUIV_List_HScrollerVisibility_Always ||
            List_ContentWidth(data) > _mwidth(data->area))
        && _minheight(data->horiz) > 0
        && _height(data->area) > 2 * _minheight(data->horiz);
}

/* Give the scroller its range; the first pixel stays within it */
static void List_SetHorizRange(struct MUI_ListData *data)
{
    LONG content = List_ContentWidth(data), visible = _mwidth(data->area);

    if (data->horiz_first > content - visible)
        data->horiz_first = content - visible;
    if (data->horiz_first < 0)
        data->horiz_first = 0;
    SetAttrs(data->horiz, MUIA_NoNotify, TRUE,
        MUIA_Prop_Entries, content,
        MUIA_Prop_Visible, visible,
        MUIA_Prop_First, data->horiz_first, TAG_DONE);
}

/* The entry area was laid out at (left, top, width, height) in the
 * group; shorten it for the scroller below when the columns need one,
 * or take the scroller out of the layout. Runs inside a layout pass,
 * where the list is hidden, so the shown flag can change hands directly. */
void zune_list_layout_horiz(Object *obj, struct MUI_ListData *data,
    LONG left, LONG top, LONG width, LONG height)
{
    BOOL need = List_NeedHoriz(data);

    if (!data->horiz)
        return;
    if (need)
    {
        LONG hh = _minheight(data->horiz);

        MUI_Layout(data->area, left, top, width, height - hh, 0);
        _flags(data->horiz) |= MADF_SHOWME;
        MUI_Layout(data->horiz, left, top + height - hh, width, hh, 0);
        data->horiz_shown = TRUE;
        List_SetHorizRange(data);
    }
    else
    {
        if (_flags(data->horiz) & MADF_CANDRAW)
            DoHideMethod(data->horiz);
        _flags(data->horiz) &= ~MADF_SHOWME;
        data->horiz_shown = FALSE;
        data->horiz_first = 0;
    }
}

/* After the columns changed width: show or hide the scroller through a
 * relayout, or just update its range (list.c's List_CheckHoriz has
 * checked that there is one, set up, and that the list is not quiet) */
void zune_list_check_horiz(Object *obj, struct MUI_ListData *data)
{
    if (List_NeedHoriz(data) != data->horiz_shown)
    {
        if (_flags(obj) & MADF_CANDRAW)
        {
            DoMethod(obj, MUIM_Group_InitChange);
            DoMethod(obj, MUIM_Group_ExitChange);
        }
    }
    else if (data->horiz_shown)
        List_SetHorizRange(data);
}

/* MUI 3.9's title buttons above the entry area (programs that get MUI
 * 4/5): each column, with its share of the spacing and its bar, gets a
 * bevel of shine above and left, shadow below and right, with background
 * corners where shine and shadow meet. Returns the strip's clip, in
 * which list.c draws the title row and which it removes. */
APTR zune_list_draw_title_strip(Object *obj, struct MUI_ListData *data)
{
    struct RastPort *rp = _rp(obj);
    WORD strip = _font(obj)->tf_YSize + 6;  /* List_TitleStrip() */
    LONG top = _top(data->area) - strip, bottom = _top(data->area) - 1;
    LONG left = _left(data->area), right = _right(data->area);
    LONG x0 = left - data->horiz_first;
    LONG x1 = _mleft(data->area) - data->horiz_first;
    ULONG col;
    APTR clip;

    SetAPen(rp, _pens(obj)[MPEN_BACKGROUND]);
    RectFill(rp, left, top, right, bottom);
    /* scrolled buttons start left of the strip */
    clip = MUI_AddClipping(muiRenderInfo(obj), left, top, right - left + 1,
        strip);
    for (col = 0; col < data->columns && x0 <= right; col++)
    {
        ULONG halfdelta = data->ci[col].delta / 2;
        if (ColumnHidden(data, col)) continue;
        LONG end;

        x1 += ColumnWidth(data, col) + halfdelta;
        /* a column ends where its bar is or would be; an empty button
         * fills the row after the last column */
        end = x1;
        if (end > right || (col + 1 == data->columns && end + 2 > right))
            end = right;
        SetAPen(rp, _pens(obj)[MPEN_SHINE]);
        Move(rp, x0, bottom - 1);
        Draw(rp, x0, top);
        Draw(rp, end - 1, top);
        SetAPen(rp, _pens(obj)[MPEN_SHADOW]);
        Move(rp, x0 + 1, bottom);
        Draw(rp, end, bottom);
        Draw(rp, end, top + 1);
        x0 = end + 1;
        x1 += (data->ci[col].bar ? BAR_WIDTH : 0)
            + data->ci[col].delta - halfdelta;
    }
    if (x0 + 1 < right)
    {
        SetAPen(rp, _pens(obj)[MPEN_SHINE]);
        Move(rp, x0, bottom - 1);
        Draw(rp, x0, top);
        Draw(rp, right - 1, top);
        SetAPen(rp, _pens(obj)[MPEN_SHADOW]);
        Move(rp, x0 + 1, bottom);
        Draw(rp, right, bottom);
        Draw(rp, right, top + 1);
    }
    return clip;
}

/* MUIM_List_SortEntries (MUI 5): sorts a program's own NULL-terminated
 * array of entries by the list's compare hook */
static void ListSiftRaw(Object *obj, struct MUI_ListData *data, APTR *entries,
    LONG root, LONG count)
{
    APTR entry = entries[root];
    while (root < count / 2)
    {
        LONG child = root * 2 + 1;
        if (child + 1 < count && zune_list_compare(obj, data, entries[child], entries[child + 1]) < 0) child++;
        if (zune_list_compare(obj, data, entry, entries[child]) >= 0) break;
        entries[root] = entries[child]; root = child;
    }
    entries[root] = entry;
}

IPTR zune_list_sort_entries(struct IClass *cl, Object *obj, APTR *entries)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    LONG count = 0, i;
    if (!entries) return FALSE;
    while (entries[count]) count++;
    for (i = count / 2; i > 0; i--) ListSiftRaw(obj, data, entries, i - 1, count);
    for (i = count - 1; i > 0; i--)
    {
        APTR entry = entries[0]; entries[0] = entries[i]; entries[i] = entry;
        ListSiftRaw(obj, data, entries, 0, i);
    }
    return TRUE;
}


/* MUIA_List_HideColumn / ShowColumn / TitleArray (MUI 5); init: from
 * OM_NEW (the format is parsed, nothing is drawn yet; a TitleArray of
 * NULL is not passed) */
void zune_list_set_columns(Object *obj, struct MUI_ListData *data,
    struct TagItem *tag, BOOL init)
{
    if (tag->ti_Tag == MUIA_List_TitleArray)
    {
        data->title_array = (APTR)tag->ti_Data;
        data->title_columns = data->max_columns_explicit ? data->max_columns : data->columns;
        data->title = data->title_array ? (STRPTR)1 : NULL;
        if (!init)
            DoMethod(obj, MUIM_List_Redraw, MUIV_List_Redraw_All);
        return;
    }
    if (tag->ti_Data < data->columns)
    {
        for (LONG i = 0; i < data->columns; i++)
            if (data->ci[i].order_id == tag->ti_Data)
                data->ci[i].hidden = tag->ti_Tag == MUIA_List_HideColumn;
        if (init)
            return;
        if (_flags(obj) & MADF_CANDRAW)
        {
            DoMethod(obj, MUIM_Group_InitChange);
            DoMethod(obj, MUIM_Group_ExitChange);
        }
        data->update = UPDATEMODE_ALL;
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    }
}

/* A format column's MUI 4/5 options (ParseListFormat, args as ReadArgs
 * filled them from FORMAT_TEMPLATE); after MINWIDTH and MAXWIDTH */
void zune_list_format_mui45(struct ColumnInfo *ci, IPTR *args)
{
    if (args[ARG_FIXWIDTH])
    {
        ci->min_width = ci->max_width =
            MAX(0, MIN(32767, *(LONG *)args[ARG_FIXWIDTH]));
        ci->min_pixels = ci->max_pixels = TRUE;
        ci->fixed = TRUE;
        ci->delta = 0;
    }
    ci->is_object = !!args[ARG_ISOBJECT];
    if (args[ARG_ELIDE])
    {
        STRPTR elide = (STRPTR)args[ARG_ELIDE];
        ci->elide = !Stricmp(elide, "left") ? MUIV_Text_Shorten_ElideLeft :
            !Stricmp(elide, "center") ? MUIV_Text_Shorten_ElideCenter :
            !Stricmp(elide, "right") ? MUIV_Text_Shorten_ElideRight : MUIV_Text_Shorten_Cutoff;
    }
    ci->hidden = !!args[ARG_HIDDEN];
    ci->sortable = !!args[ARG_SORTABLE];
    ci->descending = args[ARG_ORDER] &&
        (!Stricmp((STRPTR)args[ARG_ORDER], "DESC") ||
         !Stricmp((STRPTR)args[ARG_ORDER], "DESCENDING"));
}
