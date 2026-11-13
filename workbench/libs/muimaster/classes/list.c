/*
    Copyright (C) 2002-2025, The AROS Development Team. All rights reserved.
*/

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
#include "modules.h"

extern struct Library *MUIMasterBase;

#include "list_private.h"

/* Keep natural text widths separate from the current allocation. Resizing
 * can shrink and grow columns without measuring every entry again. */
void zune_list_layout_columns(struct MUI_ListData *data)
{
    struct ColumnInfo *ci, *end, *last = NULL;
    LONG available, used = 0, extra;

    if (!data->format_layout || !data->area) return;
    available = MAX(0, _mwidth(data->area));
    end = data->ci + data->columns;
    for (ci = data->ci; ci < end; ci++)
    {
        LONG minimum, maximum;

        ci->layout_hidden = FALSE;
        ci->layout_width = 0;
        if (ci->hidden) continue;
        minimum = ci->min_width < 0 ? ci->entries_width :
            ci->min_pixels ? ci->min_width : available * ci->min_width / 100;
        maximum = ci->max_width < 0 ? ci->entries_width :
            ci->max_pixels ? ci->max_width : available * ci->max_width / 100;
        if (!ci->fixed) { minimum += 6; maximum += 6; }
        /* the widths are WORDs: 32767 pixels at most */
        if (minimum > 32767) minimum = 32767;
        ci->layout_max = MIN(MAX(minimum, maximum), 32767);
        if (last)
        {
            LONG gap = last->delta + (last->bar ? BAR_WIDTH : 0);
            if (used + gap + minimum > available)
            { ci->layout_hidden = TRUE; continue; }
            used += gap;
        }
        ci->layout_width = minimum;
        used += minimum;
        last = ci;
    }
    extra = available - used;
    /* Redistribute space left by columns which hit their maximum. */
    while (extra > 0)
    {
        LONG weights = 0, remaining = extra;
        for (ci = data->ci; ci < end; ci++)
            if (!ci->hidden && !ci->layout_hidden && ci->layout_width < ci->layout_max)
                weights += ci->weight;
        if (!weights) break;
        for (ci = data->ci; ci < end; ci++)
        {
            if (ci->hidden || ci->layout_hidden || ci->layout_width >= ci->layout_max || !ci->weight) continue;
            LONG share = MAX(1, remaining * ci->weight / weights);
            share = MIN(extra, MIN(share, ci->layout_max - ci->layout_width));
            ci->layout_width += share; extra -= share;
        }
        if (extra == remaining) break;
    }
}

static LONG ParseColumnWidth(CONST_STRPTR value, UBYTE *pixels)
{
    char *end;
    LONG width = strtol((const char *)value, &end, 10);
    *pixels = !Stricmp(end, "px");
    if (end == (const char *)value || (*end && !*pixels)) return -1;
    return MAX(-1, MIN(width, 32767));
}

/* In-place editing (MUIM_List_Edit, MUI 5) and the list's drag and drop
 * methods are listx.c: the list module (modules.h) in the native build,
 * included at the end of this file in others. Only an editable list, a
 * list with an editor open or a drag ever calls them. */
static inline IPTR ListEndEdit(struct IClass *cl, Object *obj, ULONG mode,
    BOOL force)
{
    return ((struct MUI_ListData *)INST_DATA(cl, obj))->edit_object ?
        zune_list_end_edit(cl, obj, mode, force) : TRUE;
}
static inline IPTR ListEdit(struct IClass *cl, Object *obj, LONG row,
    LONG column)
{
    return ((struct MUI_ListData *)INST_DATA(cl, obj))->editable ?
        zune_list_edit(cl, obj, row, column) : FALSE;
}
static inline void ListLayoutEditor(Object *obj, struct MUI_ListData *data)
{
    if (data->edit_object)
        zune_list_layout_editor(obj, data);
}

/****** List.mui/MUIA_List_Active ********************************************
*
*   NAME
*       MUIA_List_Active -- (V4) [ISG], LONG
*
*   FUNCTION
*       The index of the active entry. There can be at most one active entry
*       in a list. The active entry is highlighted visibly, except for
*       read-only lists (those whose Listview has MUIA_Listview_Input set to
*       FALSE). Selecting an entry with the mouse, or moving through the list
*       with keyboard controls will also change the active entry (again
*       excepting read-only lists).
*
*       When set programmatically through this attribute, some special values
*       can be used:
*
*           MUIV_List_Active_Off
*           MUIV_List_Active_Top
*           MUIV_List_Active_Bottom
*           MUIV_List_Active_Up
*           MUIV_List_Active_Down
*           MUIV_List_Active_PageUp
*           MUIV_List_Active_PageDown
*
*       When this attribute is read, either the index of the active entry or
*       the special value MUIV_List_Active_Off will be returned.
*
*       Setting this attribute to a new value will additionally have the same
*       effect as calling the MUIM_List_Jump method with the specified or
*       implied index.
*
*   NOTES
*       The concept of an active entry must not be confused with that of a
*       selected entry.
*
*   SEE ALSO
*       MUIM_List_Jump, MUIM_List_Select, MUIA_Listview_Input
*
******************************************************************************
*
*/

/****** List.mui/MUIA_List_AutoVisible ***************************************
*
*   NAME
*       MUIA_List_AutoVisible -- (V11) [ISG], BOOL
*
*   FUNCTION
*       When this attribute is set to true, the active entry will be in the
*       visible portion of the list whenever the list is unhidden.
*
*   SEE ALSO
*       MUIA_List_Active
*
******************************************************************************
*
*/

/****** List.mui/MUIA_List_CompareHook ***************************************
*
*   NAME
*       MUIA_List_CompareHook -- (V4) [IS.], struct Hook *
*
*   FUNCTION
*       The provided hook indicates the sort ordering of two list entries.
*       The hook receives list-entry data pointers as its second and third
*       arguments. The hook should return a negative value if the first entry
*       should be placed before the second entry, a positive value if the
*       first entry should be placed after the second entry, and zero if the
*       entries are equal.
*
*       In addition to being used internally for sorting operations, this hook
*       will be called when MUIM_List_Compare is externally invoked.
*
*       If this attribute is not specified or is set to NULL, all list entries
*       must be strings.
*
******************************************************************************
*
*/

/****** List.mui/MUIA_List_ConstructHook *************************************
*
*   NAME
*       MUIA_List_ConstructHook -- (V4) [IS.], struct Hook *
*
*   FUNCTION
*       The provided hook creates a list entry. Whenever a data item is added
*       to the list, it is the result of this hook that is really added.

*       The hook receives the list's memory pool as its second argument and
*       the data item for the entry as its third argument. The memory pool may
*       be used for allocating memory for the new entry if desired. The hook
*       should return the new data entry, or NULL if an error occurred.
*
*       If this attribute is not specified or is set to NULL, all list entries
*       must be strings (which must continue to exist for the lifetime of the
*       list).
*
*       If you want string entries to be duplicated within the list, so that
*       they do not have to be preserved externally, a built-in hook for this
*       purpose can be used by providing the special value
*       MUIV_List_ConstructHook_String.
*
*       Whenever this attribute is specified, a matching hook must be
*       specified for the MUIA_ListDestructHook attribute.
*
*   SEE ALSO
*       MUIA_List_DestructHook
*
******************************************************************************
*
*/

/****** List.mui/MUIA_List_DestructHook **************************************
*
*   NAME
*       MUIA_List_DestructHook -- (V4) [IS.], struct Hook *
*
*   FUNCTION
*       The provided hook destroys a list entry, and is called whenever a data
*       item is removed from the list. It can be used to deallocate any
*       resources for the entry that were allocated by the hook specified for
*       MUIA_List_ConstructHook.
*
*       The hook receives the list's memory pool as its second argument and
*       the list entry to be destroyed as its third argument. The hook should
*       have no return value.
*
*       If this attribute is not specified or is set to NULL, all list entries
*       must be strings (which must continue to exist for the lifetime of the
*       list).
*
*       If you want string entries to be duplicated within the list, so that
*       they do not have to be preserved externally, a built-in hook for this
*       purpose can be used by providing the special value
*       MUIV_List_ConstructHook_String.
*
*       This attribute must only be specified when a matching hook has been
*       specified for the MUIA_List_ConstructHook attribute. If you specified
*       MUIV_List_ConstructHook_String for MUIA_List_ConstructHook, you must
*       specify MUIV_List_DestructHook_String for this attribute.
*
*   SEE ALSO
*       MUIA_List_ConstructHook
*
******************************************************************************
*
*/

/****** List.mui/MUIA_List_DropMark ******************************************
*
*   NAME
*       MUIA_List_DropMark -- (V11) [..G], LONG
*
*   FUNCTION
*       Provides the index of the last position where a list entry was
*       successfully dropped. The initial value before any entry has been
*       dropped is undefined.
*
*   SEE ALSO
*       MUIA_List_DragSortable, MUIA_List_ShowDropMarks,
*       MUIA_Listview_Draggable
*
******************************************************************************
*
*/

/****** List.mui/MUIA_List_First *********************************************
*
*   NAME
*       MUIA_List_First -- (V4) [..G], LONG
*
*   FUNCTION
*       The index of the first entry that can be seen (assuming nothing
*       obscures the list) This value of this attribute is -1 when the
*       list's window is not open.
*
*   NOTES
*       Notification does not occur on this attribute in MUI.
*
*   SEE ALSO
*       MUIA_List_First, MUIA_List_Entries
*
******************************************************************************
*
*/

/****** List.mui/MUIA_List_MultiTestHook *************************************
*
*   NAME
*       MUIA_List_MultiTestHook -- (V4) [IS.], struct Hook *
*
*   FUNCTION
*       The provided hook indicates whether a particular list entry
*       may be multiselected. The hook receives the list-entry data pointer as
*       its third argument, and returns a Boolean value. If this attribute is
*       not specified or is set to NULL, all list entries are considered
*       multi-selectable.
*
*       Whenever an entry is about to be selected, this hook is called if
*       there are other entries already selected. If the hook returns TRUE,
*       the entry may be multi-selected; if the hook returns FALSE, the entry
*       remains unselected.
*
*       Additionally, if a non-multi-selectable entry has been selected (as
*       the only selected entry in the list), any attempt to select an
*       additional entry will fail.
*
******************************************************************************
*
*/

/****** List.mui/MUIA_List_ShowDropMarks *************************************
*
*   NAME
*       MUIA_List_ShowDropMarks -- (V11) [ISG], BOOL
*
*   FUNCTION
*       Specifies whether a visual indication will be shown of where a list
*       item will be inserted if dropped during a drag-and-drop operation.
*       Defaults to TRUE. Only has an affect when the list is a drop target
*       (e.g. when MUIA_List_DragSortable is TRUE).
*
*   SEE ALSO
*       MUIA_List_DragSortable, MUIA_List_DropMark, MUIA_Listview_Draggable
*
******************************************************************************
*
*/

/****** List.mui/MUIA_List_Title *********************************************
*
*   NAME
*       MUIA_List_Title -- (V6) [ISG], char *
*
*   FUNCTION
*       A heading for the list, placed above list entries. A value of NULL
*       means no title is used. A value of TRUE means that the custom
*       display hook provides a separate title for each column; the hook
*       must then provide column titles instead of normal column data when
*       the entry pointer provided is NULL.
*
*   NOTES
*       If a string is set for this attribute, it is not cached within the
*       object.
*
*   SEE ALSO
*       MUIA_List_DisplayHook
*
******************************************************************************
*
*/

/****** List.mui/MUIA_List_Visible *******************************************
*
*   NAME
*       MUIA_List_Visible -- (V4) [..G], LONG
*
*   FUNCTION
*       The number of entries that can be seen at once with the list's
*       current dimensions. This value of this attribute is -1 when the
*       list's window is not open.
*
*   NOTES
*       Notification does not occur on this attribute in MUI.
*
*   SEE ALSO
*       MUIA_List_First, MUIA_List_Entries
*
******************************************************************************
*
*/

/**************************************************************************
 Ensures that there can be at least the given amount of entries within
 the list. Returns 0 if not. It also holds the title, entries[ENTRY_TITLE]
 (cleared when the array is first made).
**************************************************************************/
static int SetListSize(struct MUI_ListData *data, LONG size)
{
    struct ListEntry *new_entries;
    LONG n;

    if (size + 1 <= data->entries_allocated)
        return 1;

    n = data->entries_allocated * 2 + 4;
    if (n < size + 1)
        n = size + 1 + 10;  /* 10 is just random */
    /* a count no memory can hold (an Insert's bogus count): the byte size
     * below would wrap to a small block and the copy run past it */
    if ((ULONG)n > 0x0FFFFFFF)
        return 0;

    new_entries = AllocVec(n * sizeof(struct ListEntry),
        data->entries ? 0 : MEMF_CLEAR);
    if (NULL == new_entries)
        return 0;
    if (data->entries)
    {
        CopyMem(data->entries - 1, new_entries,
            (data->entries_num + 1) * sizeof(struct ListEntry));
        FreeVec(data->entries - 1);
    }
    data->entries = new_entries + 1;
    data->entries_allocated = n;
    return 1;
}

/**************************************************************************
 Frees all memory allocated by ParseListFormat()
**************************************************************************/
static void FreeListFormat(struct MUI_ListData *data)
{
    if (data->ci)
    {
        for (LONG i = 0; i < data->columns; i++)
            FreeVec(data->ci[i].preparse);
        FreeVec(data->ci);      /* the whole block */
        data->ci = NULL;
    }
    data->mapped_strings = NULL;
    data->preparses = data->strings = NULL;
    data->columns = 0;
}

/* Strings in a row of MUIV_List_ConstructHook/DisplayHook_StringArray */
static LONG StringArrayColumns(struct MUI_ListData *data)
{
    return data->max_columns_explicit ? data->max_columns
        : MIN(data->columns_used, data->max_columns);
}

/* String col of a string array row, NULL past its end. Our own copies
 * (ConstructHook_StringArray) know their length, the format may have
 * grown since; a program's own array has StringArrayColumns(). */
static CONST_STRPTR StringArrayGet(struct MUI_ListData *data, APTR entry,
    LONG col)
{
    LONG n;

    if (!entry)
        return NULL;
    n = (IPTR)data->construct_hook == MUIV_List_ConstructHook_StringArray
        ? (LONG)((ULONG *)entry)[-1] : StringArrayColumns(data);
    return col < n ? ((CONST_STRPTR *)entry)[col] : NULL;
}

/* The format's block for n columns and reserved display slots: the
 * columns, preparses[] (reserved + 10), strings[] (reserved + 11: the
 * entry position at -1 and room for MPlayer's -9) and mapped_strings[]
 * (2 n). Cleared. */
static struct ColumnInfo *List_FormatBlock(LONG n, LONG reserved)
{
    return AllocVec(n * (sizeof(struct ColumnInfo) + 2 * sizeof(STRPTR))
        + (2 * reserved + 21) * sizeof(STRPTR), MEMF_CLEAR);
}

/**************************************************************************
 Parses the given format string and replaces the previous format with it.
 Return FALSE on failure; the previous format is then kept: a list left
 without one wrote its display strings through a NULL array at the next
 redraw.
**************************************************************************/
static BOOL ParseListFormat(struct MUI_ListData *data, STRPTR format)
{
    LONG n = 1, i, reserved, used, base;
    BOOL layout = FALSE, mapped = FALSE;
    struct ColumnInfo *ci, *ci2;
    STRPTR ptr, format_sep;
    char c;

    IPTR args[ARG_CNT];
    struct RDArgs *rdargs;

    if (!format)
        format = (STRPTR) "";

    /* Count the number of columns first */
    for (ptr = format; (c = *ptr++);)
        if (c == ',')
            n++;
    if (n > data->max_columns)
        return FALSE;

    /* The display arrays hold every column the format names (COL= may
     * point past the column count), all of MUIA_List_MaxColumns when a
     * program set it, and at least LIST_MIN_RESERVED: display hooks may
     * fill more columns than the format has (Zune's Dirlist always fills
     * six), so a format of one column must not get a one-slot array. */
    base = MAX(n, data->max_columns_explicit
        ? data->max_columns : MIN(LIST_MIN_RESERVED, data->max_columns));
    if (!(ci = List_FormatBlock(n, base)))
        return FALSE;

    // set defaults
    for (i = 0; i < n; i++)
    {
        ci[i].colno = ci[i].order_id = i;
        ci[i].weight = 100;
        ci[i].delta = 4;
        ci[i].min_width = -1;
        ci[i].max_width = -1;
    }

    if (*format && (format_sep = StrDup(format)) != 0)
    {
        for (i = 0; format_sep[i] != '\0'; i++)
        {
            if (format_sep[i] == ',')
                format_sep[i] = '\0';
        }

        if ((rdargs = AllocDosObject(DOS_RDARGS, NULL)) != 0)
        {
            ptr = format_sep;
            i = 0;
            do
            {
                /* ReadArgs() wants the line terminated by a newline; a
                 * column of just "BAR" (xSysInfo's lists) failed without
                 * it and lost its bar */
                char line[128];
                LONG len = strlen(ptr);

                if (len > (LONG)sizeof(line) - 2)
                    len = sizeof(line) - 2;
                CopyMem(ptr, line, len);
                line[len++] = '\n';
                line[len] = '\0';
                rdargs->RDA_Source.CS_Buffer = line;
                rdargs->RDA_Source.CS_Length = len;
                rdargs->RDA_Source.CS_CurChr = 0;
                rdargs->RDA_DAList = 0;
                rdargs->RDA_Buffer = NULL;
                rdargs->RDA_BufSiz = 0;
                rdargs->RDA_ExtHelp = NULL;
                rdargs->RDA_Flags = 0;

                memset(args, 0, sizeof args);
                if (ReadArgs(FORMAT_TEMPLATE, args, rdargs))
                {
                    struct ColumnInfo *cc = &ci[i];
                    /* a column number no format can show: its own */
                    if (args[ARG_COL])
                    {
                        LONG col = *(LONG *) args[ARG_COL];
                        if (col >= 0 && col < MAX(n, data->max_columns))
                            cc->colno = col;
                    }
                    if (args[ARG_WEIGHT])
                        cc->weight = MAX(0, MIN(10000, *(LONG *)args[ARG_WEIGHT]));
                    if (args[ARG_DELTA])
                        cc->delta = MAX(0, MIN(32767, *(LONG *)args[ARG_DELTA]));
                    if (args[ARG_MINWIDTH])
                        cc->min_width =
                            ParseColumnWidth((STRPTR)args[ARG_MINWIDTH], &cc->min_pixels);
                    if (args[ARG_MAXWIDTH])
                        cc->max_width =
                            ParseColumnWidth((STRPTR)args[ARG_MAXWIDTH], &cc->max_pixels);
                    /* FIXWIDTH, ISOBJECT, ELIDE, HIDDEN, SORTABLE, ORDER
                     * (MUI 4/5; MUI 3.8's format has none of them) */
                    if (args[ARG_FIXWIDTH] || args[ARG_ISOBJECT]
                        || args[ARG_ELIDE] || args[ARG_HIDDEN]
                        || args[ARG_SORTABLE] || args[ARG_ORDER])
                        zune_list_format_mui45(cc, args);
                    layout |= args[ARG_MINWIDTH] || args[ARG_MAXWIDTH] || args[ARG_FIXWIDTH];
                    cc->bar = args[ARG_BAR];
                    if (args[ARG_PREPARSE])
                        cc->preparse = StrDup((STRPTR) args[ARG_PREPARSE]);

                    FreeArgs(rdargs);
                }
                ptr += strlen(ptr) + 1;
                i++;
            }
            while (i < n);
            FreeDosObject(DOS_RDARGS, rdargs);
        }
        FreeVec(format_sep);
    }

    reserved = base;
    used = n;
    for (i = 0; i < n; i++)
    {
        if (ci[i].colno >= reserved)
            reserved = ci[i].colno + 1;
        if (ci[i].colno >= used)
            used = ci[i].colno + 1;
        if (ci[i].colno != i)
            mapped = TRUE;
    }
    if (reserved > base)
    {
        /* a COL= past the slots: a larger block (rare) */
        if (!(ci2 = List_FormatBlock(n, reserved)))
        {
            for (i = 0; i < n; i++)
                FreeVec(ci[i].preparse);
            FreeVec(ci);
            return FALSE;
        }
        CopyMem(ci, ci2, n * sizeof(struct ColumnInfo));
        FreeVec(ci);
        ci = ci2;
    }

    FreeListFormat(data);
    data->ci = ci;
    data->columns = n;
    data->columns_reserved = reserved;
    data->columns_used = used;
    data->format_layout = layout;
    data->preparses = (STRPTR *)(ci + n);
    /* Skip entry pos (-1) and enough indexes to get (-9). Based on MPlayer source codes. */
    data->strings = data->preparses + reserved + 10 + 10;
    if (mapped)
        data->mapped_strings = List_MappedArea(data);
    return TRUE;
}

static void MapColumns(struct MUI_ListData *data)
{
    if (!data->mapped_strings) return;
    for (LONG i = 0; i < data->columns; i++)
    {
        data->mapped_strings[i] = data->strings[data->ci[i].colno];
        data->mapped_strings[data->columns + i] = data->preparses[data->ci[i].colno];
    }
    memcpy(data->strings, data->mapped_strings, data->columns * sizeof(STRPTR));
    memcpy(data->preparses, data->mapped_strings + data->columns, data->columns * sizeof(STRPTR));
}

/**************************************************************************
 Call the MUIM_List_Display for the given entry. It fills out
 data->string and data->preparses
**************************************************************************/
static void DisplayEntry(struct IClass *cl, Object *obj, int entry_pos)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    APTR entry_data;
    int col;

    if (data->mapped_strings)
    {
        memset(data->preparses, 0, data->columns_reserved * sizeof(STRPTR));
        for (col = 0; col < data->columns; col++)
            data->preparses[data->ci[col].colno] = data->ci[col].preparse;
    }
    else
        for (col = 0; col < data->columns; col++)
            data->preparses[col] = data->ci[col].preparse;

    if (entry_pos == ENTRY_TITLE)
    {
        if (data->title_array)
        {
            LONG count = data->title_columns;
            for (col = 0; col < data->columns_reserved; col++)
                data->strings[col] = col < count ? (STRPTR)data->title_array[col] : NULL;
            MapColumns(data);
            return;
        }
        if ((data->columns == 1) && (data->title != (STRPTR) 1))
        {
            *data->strings = data->title;
            return;
        }
        entry_data = NULL;      /* it's a title request */
    }
    else
    {
        data->strings[-1] = (STRPTR)(IPTR)entry_pos;
        entry_data = data->entries[entry_pos].data;
    }

    /* Get the display formation */
    DoMethod(obj, MUIM_List_Display, (IPTR) entry_data,
        (IPTR) data->strings, entry_pos, (IPTR) data->preparses);
    MapColumns(data);
}


/* The common cell: a string without a preparse, escapes, newlines or
 * tabs, which the text engine would take as one plain chunk
 * (zune_text_init_plain): its length, -1 for any other */
static LONG List_PlainLen(CONST_STRPTR pre, CONST_STRPTR s)
{
    LONG n;

    if ((pre && *pre) || !s || !*s)
        return -1;
    for (n = 0; s[n]; n++)
        if (s[n] == '\33' || s[n] == '\n' || s[n] == '\t' || n == 32767)
            return -1;
    return n;
}

/* A plain cell's width as zune_text_get_bounds() measures it, when the
 * list's RastPort already has the list's font in the plain style (no
 * copy of it needed): -1 otherwise */
static LONG List_PlainWidth(Object *obj, CONST_STRPTR pre, CONST_STRPTR s)
{
    struct RastPort *rp;
    LONG len = List_PlainLen(pre, s);

    if (len < 0 || !muiRenderInfo(obj) || !(rp = muiRenderInfo(obj)->mri_RastPort)
        || rp->Font != _font(obj) || rp->AlgoStyle)
        return -1;
    return TextLength(rp, s, len);
}

/* Any other cell's text size through the text engine: its width in
 * *width, its height returned, -1 for none. Without an allocation for a
 * plain string; its ZText off CalcDimsOfEntry's frame. */
static __attribute__((noinline)) LONG List_TextSize(Object *obj,
    CONST_STRPTR pre, CONST_STRPTR s, LONG *width)
{
    ZText *text, local_text;
    ZTextLine local_line;
    ZTextChunk local_chunk;
    BOOL borrowed = zune_text_init_plain(&local_text, &local_line,
        &local_chunk, pre, s);
    LONG h;

    text = borrowed ? &local_text : zune_text_new(pre, s, ZTEXT_ARG_NONE, 0);
    if (!text)
        return -1;
    zune_text_get_bounds(text, obj);
    *width = text->width;
    h = text->height;
    if (!borrowed)
        zune_text_destroy(text);
    return h;
}

/**************************************************************************
 Determine the dims of a single entry and adapt the columninfo according
 to it. pos might be ENTRY_TITLE. Returns 0 if pos entry needs to
 be redrawn after this operation, 1 if all entries need to be redrawn.
**************************************************************************/
static int CalcDimsOfEntry(struct IClass *cl, Object *obj, int pos)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    struct ListEntry *entry;
    LONG j, height;
    int ret = 0;

    if (!(_flags(obj) & MADF_SETUP))
        return ret;

    DisplayEntry(cl, obj, pos);
    /* after the display hook, which might have changed the list */
    entry = &data->entries[pos];

    /* Set height to at least minheight */
    if (entry->height < data->entry_minheight)
        entry->height = data->entry_minheight;

    for (j = 0; j < data->columns; j++)
    {
        if (data->ci[j].is_object && pos != ENTRY_TITLE)
        {
            ret |= zune_list_cell_dims(obj, data, entry, j);
            continue;
        }
        /* this runs for every column of every inserted entry: a plain
         * string is measured at once */
        LONG width, th;
        if ((width = List_PlainWidth(obj, data->preparses[j],
            data->strings[j])) >= 0)
            th = _font(obj)->tf_YSize;
        else if ((th = List_TextSize(obj, data->preparses[j],
            data->strings[j], &width)) < 0)
            continue;

        /* Row pitch includes the Listview font leading preference.
         * Rows are drawn entry_maxheight apart: only a new maximum
         * (below) moves the other rows, not this entry's own height
         * growing from 0 as it is measured */
        height = th + data->prefs_linespacing;
        if (height > entry->height)
            entry->height = height;

        /* a SORTABLE column's title has room for the sort mark
         * (zune_list_draw_sort_mark: a triangle two half-lines wide
         * and a gap on each side), as on MUI 5 */
        if (pos == ENTRY_TITLE && data->ci[j].sortable)
            width += (_font(obj)->tf_YSize + 1) / 2 * 2 + 5;

        if (width > data->ci[j].entries_width)
        {
            /* This entry has a greater width for this column than any
             * other entry, so we store this value */
            data->ci[j].entries_width = width;
            /* column width changed, redraw all entries later */
            ret = 1;
        }
    }
    if (entry->height > data->entry_maxheight)
    {
        data->entry_maxheight = entry->height;
        /* maximum entry height changed, redraw all entries later */
        ret = 1;
    }

    return ret;
}

/**************************************************************************
 Determine the widths of the entries
**************************************************************************/
static void CalcWidths(struct IClass *cl, Object *obj)
{
    int i, j;
    struct MUI_ListData *data = INST_DATA(cl, obj);

    if (!(_flags(obj) & MADF_SETUP))
        return;

    for (j = 0; j < data->columns; j++)
        data->ci[j].entries_width = 0;

    data->entry_maxheight = 0;
    data->entries_totalheight = 0;
    data->entries_maxwidth = 0;

    for (i = (data->title ? ENTRY_TITLE : 0); i < data->entries_num; i++)
    {
        CalcDimsOfEntry(cl, obj, i);
        data->entries_totalheight += data->entries[i].height;
    }

    for (j = 0; j < data->columns; j++)
        if (!data->ci[j].hidden)
            data->entries_maxwidth += data->ci[j].entries_width
                + data->ci[j].delta + (data->ci[j].bar ? BAR_WIDTH : 0);

    /* Without entries, rows are one line of the list font, so
     * MUIA_List_Visible counts lines rather than pixels */
    if (!data->entry_maxheight)
        data->entry_maxheight =
            MAX(1, _font(obj)->tf_YSize + data->prefs_linespacing);
}

/**************************************************************************
 Calculates the number of visible entry lines. Returns 1 if it has
 changed
**************************************************************************/
static int CalcVertVisible(struct IClass *cl, Object *obj)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    int old_entries_visible = data->entries_visible;
    int old_entries_top_pixel = data->entries_top_pixel;

    LONG room = _mheight(data->area) - data->title_height;

    data->entries_top_pixel = _mtop(data->area) + data->title_height;
    if (ZUNE_APP_MUI38(obj) && room >= data->entry_maxheight)
    {
        /* MUI 3.8 shows only whole rows and centres them: the spare
         * pixels are shared above and below (demopix: the Floattext of
         * MUI-Demo's Groups window) */
        data->entries_visible = room / data->entry_maxheight;
        data->entries_top_pixel +=
            (room - data->entries_visible * data->entry_maxheight) / 2;
    }
    else
        /* Like MUI 3.9, count, draw and scroll by the rows that are at
         * least partly visible: the last one may be cut off at the bottom,
         * entries start at the top, spare space stays below them
         * (tests/native/listvisible.c) */
        data->entries_visible = (room + data->entry_maxheight - 1)
            / data->entry_maxheight;
    data->vertprop_visible = data->entries_visible;

    if (data->entries_visible != old_entries_visible)
    {
        superset(cl, obj, MUIA_List_Visible, data->entries_visible);
        superset(cl, obj, MUIA_List_VertProp_Visible, data->entries_visible);
    }

    return (old_entries_visible != data->entries_visible)
        || (old_entries_top_pixel != data->entries_top_pixel);
}

#define PROP_VERT_FIRST   1
#define PROP_HORIZ_FIRST  2
#define PROP_VERT_SYNC    3
#define PROP_VERT_SYNCFIRST 4

/* The units the scrollbar counts in: MUI 3.8 gives its Prop pixels
 * (entries times the row height), which Intuition's prop gadget rounds
 * into other knob sizes than rows do (demopix, tests/lean/probe/propvis.c:
 * 232 of 232 knobs the same); MUI 3.9 gives rows. */
static LONG List_PropScale(Object *obj, struct MUI_ListData *data)
{
    return (_flags(obj) & MADF_SETUP) && ZUNE_APP_MUI38(obj)
        && data->entry_maxheight > 1 ? data->entry_maxheight : 1;
}

static void List_SyncVertProp(Object *obj, struct MUI_ListData *data)
{
    LONG scale = List_PropScale(obj, data);

    if (data->vert)
        SetAttrs(data->vert, MUIA_NoNotify, TRUE,
            MUIA_Prop_DeltaFactor, scale,
            MUIA_Prop_Entries, data->vertprop_entries * scale,
            MUIA_Prop_Visible, data->vertprop_visible * scale,
            MUIA_Prop_First, data->vertprop_first * scale, TAG_DONE);
}

/* Not inlined: its tag list stays off List_Function's frame */
static __attribute__((noinline)) void List_SyncVertFirst(Object *obj,
    struct MUI_ListData *data)
{
    if (data->vert)
        SetAttrs(data->vert, MUIA_NoNotify, TRUE, MUIA_Prop_First,
            data->vertprop_first * List_PropScale(obj, data), TAG_DONE);
}

/* List's class: the hooks below are shared by every list and find the
 * list's data from the object (set at OM_NEW) */
static struct IClass *List_Class;

/* The scrollbars' notifications (MUIM_CallHook on the list) */
static IPTR List_Function(register struct Hook *hook __asm("a0"),
    register Object *obj __asm("a2"), register SIPTR *msg __asm("a1"))
{
    struct MUI_ListData *data = INST_DATA(List_Class, obj);
    SIPTR type = msg[0];
    SIPTR val = msg[1];

    switch (type)
    {
    case PROP_VERT_FIRST:
        {
            LONG scale = List_PropScale(obj, data);

            get(data->vert, MUIA_Prop_First, &val);
            if (scale > 1)
                val = (val + scale / 2) / scale;
            nnset(obj, MUIA_List_VertProp_First, val);
        }
        break;
    case PROP_VERT_SYNC:
        List_SyncVertProp(obj, data);
        break;
    case PROP_VERT_SYNCFIRST:
        /* A scroll: only the position changed. The units, the entries
         * and the visible rows were given with the layout (the units
         * change only there) or by their own notifications; the
         * scrollbar's Prop compared them and passed them over (opbench
         * "list scroll line": four tags through Scrollbar, Group, Area,
         * Notify and Prop on every step) */
        List_SyncVertFirst(obj, data);
        break;
    case PROP_HORIZ_FIRST:
        get(data->horiz, MUIA_Prop_First, &val);
        if (val != data->horiz_first && data->horiz_shown)
        {
            data->horiz_first = val;
            data->update = UPDATEMODE_ALL;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
        }
        break;
    }
    return 0;
}

static IPTR List_LayoutFunction(register struct Hook *hook __asm("a0"),
    register Object *obj __asm("a2"),
    register struct MUI_LayoutMsg *lm __asm("a1"));
static const struct Hook list_hook =
    { {NULL, NULL}, (HOOKFUNC)List_Function, NULL, NULL };
static const struct Hook list_layout_hook =
    { {NULL, NULL}, (HOOKFUNC)List_LayoutFunction, NULL, NULL };
/* After the columns changed width: the horizontal scroller (the
 * module's; a list has one only in a program that gets MUI 4/5 or asks
 * for it, MUIM_Setup) is checked when quiet ends; the module is called
 * only for the check itself */
static void List_CheckHoriz(struct IClass *cl, Object *obj)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);

    if (!data->horiz || !(_flags(obj) & MADF_SETUP))
        return;
    if (data->flags & LIST_QUIET)
    {
        data->flags |= LIST_HORIZCHECK;
        return;
    }
    data->flags &= ~LIST_HORIZCHECK;
    zune_list_check_horiz(obj, data);
}

/* At entry to this function, data->area is always set, but data->vert may
 * or may not be set */
/* The list's notifications on its three VertProp attributes, which keep
 * the scrollbar in step (on), or their removal. Pass prop object as
 * DestObj (based on code in NList); the hook gives the scrollbar all
 * three in its units, the position alone when only it changed */
static __attribute__((noinline)) void List_VertNotify(Object *obj,
    struct MUI_ListData *data, BOOL on)
{
    static const ULONG vertprop[3] = { MUIA_List_VertProp_First,
        MUIA_List_VertProp_Visible, MUIA_List_VertProp_Entries };

    for (LONG i = 0; i < 3; i++)
        if (on)
            DoMethod(obj, MUIM_Notify, vertprop[i], MUIV_EveryTime,
                (IPTR) obj, 3, MUIM_CallHook, (IPTR) &list_hook,
                i ? PROP_VERT_SYNC : PROP_VERT_SYNCFIRST);
        else
            DoMethod(obj, MUIM_KillNotifyObj, vertprop[i], (IPTR) obj);
}

static void List_HandleScrollerPos(struct IClass *cl, Object *obj)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    BOOL vert_not_used = FALSE;

    /* Disallow any changes after setup. This function should basically be
     * creation-time only */
    if (_flags(obj) & MADF_SETUP)
        return;

    /* Remove both objects */
    if (data->area_connected)
        DoMethod(obj, OM_REMMEMBER, data->area);
    if (data->vert_connected)
        DoMethod(obj, OM_REMMEMBER, data->vert);

    /* Add list and/or scroller: the scroller left of the list or right
     * of it (the default), or none */
    if (data->scroller_pos == MUIV_Listview_ScrollerPos_None)
        vert_not_used = TRUE;
    else if (!data->vert)
        data->vert = ScrollbarObject, MUIA_Group_Horiz, FALSE, End;
    if (data->scroller_pos == MUIV_Listview_ScrollerPos_Left)
        DoMethod(obj, OM_ADDMEMBER, data->vert);
    DoMethod(obj, OM_ADDMEMBER, data->area);
    if (!vert_not_used && data->scroller_pos != MUIV_Listview_ScrollerPos_Left)
        DoMethod(obj, OM_ADDMEMBER, data->vert);

    data->area_connected = TRUE;

    /* Handle case where it was decided that vert will not be used */
    if (vert_not_used)
    {
        if (data->vert)
        {
            if (data->vert_connected)
            {
                List_VertNotify(obj, data, FALSE);
                data->vert_connected = FALSE;
            }

            MUI_DisposeObject(data->vert);
            data->vert = NULL;
        }
    }

    /* If at this point data->vert is not null, it means vert is to be
     * connected */
    if (data->vert && !data->vert_connected)
    {
        List_SyncVertProp(obj, data);

        DoMethod(data->vert, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime,
            (IPTR) obj, 4, MUIM_CallHook, (IPTR) &list_hook, PROP_VERT_FIRST,
            MUIV_TriggerValue);

        List_VertNotify(obj, data, TRUE);

        data->vert_connected = TRUE;
    }
}

/* MUI 3.8 puts a List's inner spacing inside its frame: the caller's
 * MUIA_InnerLeft replaces the frame's (tests/lean/probe/vlinner.c:
 * InputListFrame + InnerLeft 0 adds 2 on the left, InnerLeft 2 adds 4).
 * Zune's frame is on the entry area, so the spacing given goes there and
 * the group around it keeps none. MUI 5 keeps it outside (as Zune did):
 * MUI 3.8 programs only. Not inlined: OM_NEW's frame stays. */
static __attribute__((noinline)) void List_InnerToArea(Object *obj,
    Object *area, struct TagItem *attrs)
{
    struct MUI_AreaData *g = muiAreaData(obj), *a = muiAreaData(area);
    struct TagItem *tag;

    if ((tag = FindTagItem(MUIA_InnerLeft, attrs)))
        a->mad_Flags |= MADF_INNERLEFT, a->mad_InnerLeft = tag->ti_Data;
    if ((tag = FindTagItem(MUIA_InnerRight, attrs)))
        a->mad_Flags |= MADF_INNERRIGHT, a->mad_InnerRight = tag->ti_Data;
    if ((tag = FindTagItem(MUIA_InnerTop, attrs)))
        a->mad_Flags |= MADF_INNERTOP, a->mad_InnerTop = tag->ti_Data;
    if ((tag = FindTagItem(MUIA_InnerBottom, attrs)))
        a->mad_Flags |= MADF_INNERBOTTOM, a->mad_InnerBottom = tag->ti_Data;
    g->mad_Flags |= MADF_INNERLEFT | MADF_INNERRIGHT | MADF_INNERTOP
        | MADF_INNERBOTTOM;
    g->mad_InnerLeft = g->mad_InnerRight = g->mad_InnerTop =
        g->mad_InnerBottom = 0;
}

/* The attributes that OM_NEW and OM_SET store alike (OM_SET then redraws
 * for Stripes and clears the selection for a MultiTestHook) */
static void List_SetPlain(struct MUI_ListData *data, struct TagItem *tag)
{
    IPTR v = tag->ti_Data;

    switch (tag->ti_Tag)
    {
    case MUIA_List_HScrollerVisibility: data->hscroll_visibility = v; break;
    case MUIA_List_SortColumn: data->sort_column = v; break;
    case MUIA_List_Stripes: data->stripes = !!v; break;
    case MUIA_List_Editable: data->editable = !!v; break;
    case MUIA_List_CompareHook:
        data->compare_hook = (struct Hook *)v;
        break;
    case MUIA_List_ConstructHook: data->construct_hook = (struct Hook *)v; break;
    case MUIA_List_DestructHook: data->destruct_hook = (struct Hook *)v; break;
    case MUIA_List_DisplayHook: data->display_hook = (struct Hook *)v; break;
    case MUIA_List_MultiTestHook: data->multi_test_hook = (struct Hook *)v; break;
    case MUIA_List_AutoVisible:
        _handle_bool_tag(data->flags, v, LIST_AUTOVISIBLE);
        break;
    case MUIA_List_ShowDropMarks:
        _handle_bool_tag(data->flags, v, LIST_SHOWDROPMARKS);
        break;
    case MUIA_List_DragSortable:
        _handle_bool_tag(data->flags, v, LIST_DRAGSORTABLE);
        break;
    case MUIA_Listview_Input: data->read_only = !v; break;
    case MUIA_Listview_MultiSelect: data->multiselect = v; break;
    case MUIA_Listview_DefClickColumn: data->def_click_column = v; break;
    }
}

/**************************************************************************
 OM_NEW
**************************************************************************/
IPTR List__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_ListData *data;
    struct TagItem *tag;
    struct TagItem *tags;
    APTR *array = NULL;
    LONG new_entries_active = MUIV_List_Active_Off;
    struct TagItem rectattrs[2] =
        {{TAG_IGNORE, TAG_IGNORE }, {TAG_DONE, TAG_DONE}};
    Object *area;

    /* search for MUIA_Frame as it has to be passed to rectangle object */
    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        if (tag->ti_Tag == MUIA_Frame)
        {
            rectattrs[0].ti_Tag = MUIA_Frame;
            rectattrs[0].ti_Data = tag->ti_Data;
            tag->ti_Tag = TAG_IGNORE;
            break;
        }
    }

    obj = (Object *) DoSuperNewTags(cl, obj, NULL,
        MUIA_Group_Horiz, TRUE,
        MUIA_InnerLeft, 0,
        MUIA_InnerRight, 0,
        MUIA_Group_Spacing, 0,
        MUIA_Font, MUIV_Font_List,
        MUIA_ShowSelState, FALSE,
        MUIA_InputMode, MUIV_InputMode_RelVerify,
        MUIA_Background, MUII_ListBack,
        TAG_MORE, (IPTR) msg->ops_AttrList,
        TAG_DONE);

    if (!obj)
        return FALSE;

    data = INST_DATA(cl, obj);

    data->columns = 1;
    /* no scroller on its own for a MUI 3 program: Listview sets one (MUI
     * 3.8 measured: a bare Floattext in the TipOfDay demo has none;
     * tests/lean/real-programs.md). MUI 5's List has its own scrollbar
     * (a bare Floattext 56x46 with a Scrollbar child, probe/bars.c) */
    data->scroller_pos = ZUNE_MUI38() ? MUIV_Listview_ScrollerPos_None
        : MUIV_Listview_ScrollerPos_Default;
    data->max_columns = GetTagData(MUIA_List_MaxColumns, 64, msg->ops_AttrList);
    data->max_columns_explicit = FindTagItem(MUIA_List_MaxColumns, msg->ops_AttrList) != NULL;
    if (data->max_columns < 1 || data->max_columns > 256)
    { CoerceMethod(cl, obj, OM_DISPOSE); return 0; }
    data->entries_active = MUIV_List_Active_Off;
    /* 1 KB puddles (MUI documents 2008): most lists hold a few entries
     * and kept a 2 KB puddle each. Programs that set
     * MUIA_List_PoolPuddleSize/PoolThreshSize get theirs. */
    data->intern_puddle_size = 1000;
    data->intern_thresh_size = 500;
    /* the instance data comes cleared: only what is not 0 is set */
    data->flags = LIST_SHOWDROPMARKS;

    data->entries_visible = data->vertprop_visible = -1;
    data->last_active = -1;

    data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    data->ehn.ehn_Flags = MUI_EHF_GUIMODE;
    data->ehn.ehn_Object = obj;
    data->ehn.ehn_Class = cl;

    data->mouse_x = MUI_MAXMAX;
    data->mouse_y = MUI_MAXMAX;

    /* HACK:
     * List is a group where part of area is rendered and part is filled
     * with other objects (inside of List dimensions). One such object is
     * up/down arrow. This object depends on RelVerify mode to control
     * behaviour. List also has the RelVerify mode. Area super class in case
     * of both of those objects adds an event handler node with the same
     * priority. Depending on the sort order, the event handler node of
     * "list" can eat a click event and up/down arrows stop working. The
     * hack is to decrease the priority of Area event handler for list to
     * always favor up/down arrow. There are other hacky ways of solving
     * this, but this seems least evil approach, as this hack is
     * encapsulated in the List class itself.
     */
    muiAreaData(obj)->mad_ehnPriority--; /* the node is made at setup */

    List_Class = cl;

    area = (Object *)GetTagData(MUIA_List_ListArea, (IPTR) 0,
        msg->ops_AttrList);

    if (!area)
    {
        area = RectangleObject, MUIA_FillArea, FALSE, TAG_MORE,
            (IPTR) rectattrs, End;
        if (area && ZUNE_MUI38())
            List_InnerToArea(obj, area, msg->ops_AttrList);
    }
    else
        data->area_replaced = TRUE;
    data->area = area;

    /* parse initial taglist */
    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_List_Active:
            new_entries_active = tag->ti_Data;
            break;

        case MUIA_List_Pool:
            data->pool = (APTR) tag->ti_Data;
            break;

        case MUIA_List_PoolPuddleSize:
            data->intern_puddle_size = tag->ti_Data;
            break;

        case MUIA_List_PoolThreshSize:
            data->intern_thresh_size = tag->ti_Data;
            break;

        case MUIA_List_SourceArray:
            array = (APTR *) tag->ti_Data;
            break;

        case MUIA_List_Format:
            data->format = (STRPTR) tag->ti_Data;
            break;

        case MUIA_List_Title:
            data->title = (STRPTR) tag->ti_Data;
            break;

        case MUIA_List_MinLineHeight:
            data->entry_minheight = MAX(0, MIN((LONG)tag->ti_Data, 32767));
            break;

        case MUIA_List_AdjustHeight:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_ADJUSTHEIGHT);
            break;

        case MUIA_List_AdjustWidth:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_ADJUSTWIDTH);
            break;

        case MUIA_Listview_ScrollerPos:
            data->scroller_pos = tag->ti_Data;
            break;

        case MUIA_Listview_DragType:
            data->drag_type = tag->ti_Data;
            if (data->drag_type != MUIV_Listview_DragType_None)
                superset(cl, obj, MUIA_Draggable, TRUE);
            break;

        default:
            List_SetPlain(data, tag);
            break;
        }
    }

    List_HandleScrollerPos(cl, obj);

    /* a superclass attribute set in OM_NEW goes to the superclass, not
     * through a subclass's OM_SET (its OM_NEW has not finished; MUI 3.8
     * measured with tests/lean/probe/newset.c: the subclass sees nothing) */
    superset(cl, obj, MUIA_Group_LayoutHook, (IPTR) &list_layout_hook);

    if (!data->pool)
    {
        /* No memory pool given, so we create our own */
        data->pool = data->intern_pool =
            CreatePool(0, data->intern_puddle_size,
            data->intern_thresh_size);
        if (!data->pool)
        {
            CoerceMethod(cl, obj, OM_DISPOSE);
            return 0;
        }
    }

    /* parse the list format */
    if (!ParseListFormat(data, data->format))
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
        if (tag->ti_Tag == MUIA_List_HideColumn || tag->ti_Tag == MUIA_List_ShowColumn)
            zune_list_set_columns(obj, data, tag, TRUE);

    /* This is necessary for at least the title */
    if (!SetListSize(data, 0))
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

    if ((tag = FindTagItem(MUIA_List_TitleArray, msg->ops_AttrList))
        && tag->ti_Data)
        zune_list_set_columns(obj, data, tag, TRUE);

    if (array)
    {
        int i;
        /* Count the number of elements */
        for (i = 0; array[i] != NULL; i++)
            ;
        /* Insert them */
        DoMethod(obj, MUIM_List_Insert, (IPTR) array, i,
            MUIV_List_Insert_Top);
    }

    /* Selected entry will be moved into visible area. The special values
     * other than Top and Bottom are below 0, so the first entry. */
    if (data->entries_num && new_entries_active != MUIV_List_Active_Off)
    {
        if (new_entries_active == MUIV_List_Active_Bottom
            || new_entries_active >= data->entries_num)
            new_entries_active = data->entries_num - 1;
        else if (new_entries_active < 0)
            new_entries_active = 0;
        data->entries_active = new_entries_active;
    }

    NewList((struct List *)&data->images);

    D(bug("List_New(0x%p)\n", obj));

    return (IPTR) obj;
}

/**************************************************************************
 OM_DISPOSE
**************************************************************************/
IPTR List__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->edit_object) ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);

    D(bug("List Dispose\n"));

    /* Call destruct method for every entry and free the entries manually
     * to avoid notification */
    while (data->confirm_entries_num)
        DoMethod(obj, MUIM_List_Destruct,
            (IPTR) data->entries[--data->confirm_entries_num].data,
            (IPTR) data->pool);

    if (data->intern_pool)
        DeletePool(data->intern_pool);
    if (data->entries)
        FreeVec(data->entries - 1);     /* the title is before the rows */

    FreeListFormat(data);

    return DoSuperMethodA(cl, obj, (Msg)msg);
}


/**************************************************************************
 OM_SET
**************************************************************************/
/* These tags describe the list itself, not its internal scrollbar/area.
 * Keep mixed inherited tag lists and explicit forwarding requests intact. */
static void List_DragEvents(Object *obj, struct MUI_ListData *data, BOOL drag);
static IPTR List_SuperSet(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct TagItem *state = msg->ops_AttrList, *tag;
    while ((tag = NextTagItem(&state)))
    {
        switch (tag->ti_Tag)
        {
        case MUIA_List_Active:
        case MUIA_List_First:
        case MUIA_List_Entries:
        case MUIA_List_Visible:
        case MUIA_List_VertProp_First:
        case MUIA_List_VertProp_Entries:
        case MUIA_List_VertProp_Visible:
        case MUIA_List_Quiet:
        case MUIA_NoNotify:
            break;
        default:
            return DoSuperMethodA(cl, obj, (Msg)msg);
        }
    }
    struct TagItem local[] = {
        {MUIA_Group_Forward, FALSE}, {TAG_MORE, (IPTR)msg->ops_AttrList}
    };
    struct opSet copy = *msg;
    copy.ops_AttrList = local;
    return DoSuperMethodA(cl, obj, (Msg)&copy);
}

/* A conservative raster-copy path. Images, custom backfills, disabled
 * overlays, virtual clipping and damaged/covered layers repaint normally. */
static BOOL List_CanScroll(Object *obj, struct MUI_ListData *data)
{
    struct MUI_RenderInfo *mri = muiRenderInfo(obj);
    struct Layer *layer;
    struct ClipRect *cr;
    if (!(_flags(obj) & MADF_CANDRAW) || !mri || !mri->mri_RastPort)
        return FALSE;
    layer = _rp(obj)->Layer;
    if (data->area_replaced || muiAreaData(obj)->mad_DisableCount ||
        (_flags(obj) & MADF_INVIRTUALGROUP) ||
        (muiAreaData(obj)->mad_Flags2 & MADF2_CUSTOMBACKFILL) ||
        !zune_imspec_is_solid(muiAreaData(obj)->mad_Background) ||
        !layer || layer->Scroll_X || layer->Scroll_Y ||
        (layer->Flags & LAYERREFRESH) || layer->ClipRegion ||
        mri->mri_BufferBM || !mri->mri_Window ||
        _rp(obj) != mri->mri_Window->RPort)
        return FALSE;
    /* Object cells also need their live geometry moved, not just pixels. */
    for (ULONG col = 0; col < data->columns; col++)
        if (data->ci[col].is_object) return FALSE;
    BOOL visible = TRUE;
    LockLayer(0, layer);
    for (cr = layer->ClipRect; cr; cr = cr->Next)
    {
        if (cr->obscured)
        {
            visible = FALSE;
            break;
        }
    }
    UnlockLayer(layer);
    return visible;
}

IPTR List__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    struct TagItem *tag;
    struct TagItem *tags;

    /* parse taglist */
    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        switch (tag->ti_Tag)
        {
        case MUIA_List_HScrollerVisibility:
        case MUIA_List_SortColumn:
        case MUIA_List_Editable:
        case MUIA_List_CompareHook:
        case MUIA_List_ConstructHook:
        case MUIA_List_DestructHook:
        case MUIA_List_DisplayHook:
        case MUIA_List_AutoVisible:
        case MUIA_List_ShowDropMarks:
        case MUIA_List_DragSortable:
        case MUIA_Listview_Input: /* private set */
        case MUIA_Listview_MultiSelect: /* private set */
        case MUIA_Listview_DefClickColumn:
            List_SetPlain(data, tag);
            data->flags &= ~LIST_DIMSVALID;   /* a hook: sizes measured again */
            break;
        case MUIA_List_Stripes:
            List_SetPlain(data, tag);
            data->update = UPDATEMODE_ALL;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;
        case MUIA_List_HideColumn:
        case MUIA_List_ShowColumn:
        case MUIA_List_TitleArray:
            data->flags &= ~LIST_DIMSVALID;
            zune_list_set_columns(obj, data, tag, FALSE);
            break;
        case MUIA_List_MultiTestHook:
            List_SetPlain(data, tag);
            if (data->multi_test_hook != NULL)
            {
                /* Clearing current selections is the easiest way to keep
                 * selections consistent with the new hook */
                DoMethod(obj, MUIM_List_Select, MUIV_List_Select_All,
                    MUIV_List_Select_Off, NULL);
            }
            break;

        case MUIA_List_ColumnOrder:
            ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);
            if (zune_list_column_order(data, (APTR)tag->ti_Data))
                DoMethod(obj, MUIM_List_Redraw, MUIV_List_Redraw_All);
            break;
        case MUIA_List_Title:
            data->title = (STRPTR) tag->ti_Data;
            DoMethod(obj, MUIM_List_Redraw, MUIV_List_Redraw_All);
            break;

        case MUIA_List_VertProp_First:
            data->vertprop_first = tag->ti_Data;
            if (data->entries_first != tag->ti_Data)
            {
                set(obj, MUIA_List_First, tag->ti_Data);
            }
            break;

        case MUIA_List_Format:
            ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);
            /* A format that cannot be used (more columns than
             * MUIA_List_MaxColumns, no memory) leaves the old one */
            if (ParseListFormat(data, (STRPTR) tag->ti_Data))
                data->format = (STRPTR) tag->ti_Data;
            DoMethod(obj, MUIM_List_Redraw, MUIV_List_Redraw_All);
            break;

        case MUIA_List_VertProp_Entries:
            data->vertprop_entries = tag->ti_Data;
            break;

        case MUIA_List_VertProp_Visible:
            data->vertprop_visible = tag->ti_Data;
            data->entries_visible = tag->ti_Data;
            break;

        case MUIA_List_Active:
            {
                LONG new_entries_active = tag->ti_Data;

                if ((data->entries_num)
                    && (new_entries_active != MUIV_List_Active_Off))
                {
                    /* inline: through List_ActiveValue() OM_SET's frame
                     * grew by 4 bytes, and OM_SET recurses */
                    switch (new_entries_active)
                    {
                    case MUIV_List_Active_Top:
                        new_entries_active = 0;
                        break;

                    case MUIV_List_Active_Bottom:
                        new_entries_active = data->entries_num - 1;
                        break;

                    case MUIV_List_Active_Up:
                        new_entries_active = data->entries_active - 1;
                        break;

                    case MUIV_List_Active_Down:
                        new_entries_active = data->entries_active + 1;
                        break;

                    /* MUI 3.9 pages by one row less than visible
                     * (tests/native/listprop.c) */
                    case MUIV_List_Active_PageUp:
                        new_entries_active = data->entries_active
                            - MAX(data->entries_visible - 1, 1);
                        break;

                    case MUIV_List_Active_PageDown:
                        new_entries_active = data->entries_active
                            + MAX(data->entries_visible - 1, 1);
                        break;
                    }

                    if (new_entries_active < 0)
                        new_entries_active = 0;
                    else if (new_entries_active >= data->entries_num)
                        new_entries_active = data->entries_num - 1;
                }
                else
                    new_entries_active = -1;

                if (data->entries_active != new_entries_active)
                {
                    LONG old = data->entries_active;
                    /* Update selection before moving existing rows. */
                    BOOL jump = new_entries_active != -1
                        && (_flags(obj) & MADF_SETUP)
                        && !(data->flags & LIST_QUIET) && !data->read_only
                        && data->entries_visible > 0
                        && (new_entries_active < data->entries_first
                            || new_entries_active >= data->entries_first
                                + data->entries_visible);
                    data->entries_active = new_entries_active;

                    /* SelectChange stuff */
                    if (new_entries_active != -1)
                    {
                        DoMethod(obj, MUIM_List_SelectChange,
                            new_entries_active, MUIV_List_Select_On, 0);
                        DoMethod(obj, MUIM_List_SelectChange,
                            new_entries_active, MUIV_List_Select_Active, 0);
                    }
                    else
                        DoMethod(obj, MUIM_List_SelectChange,
                            MUIV_List_Active_Off, MUIV_List_Select_Off, 0);

                    if (old >= 0 && old < data->entries_num)
                        data->entries[old].flags |= ENTRY_RENDER;
                    if (data->entries_active >= 0 && data->entries_active < data->entries_num)
                        data->entries[data->entries_active].flags |= ENTRY_RENDER;
                    if (jump)
                    {
                        LONG first = new_entries_active, previous = data->entries_first;
                        BOOL scroll = data->update == 0 &&
                            !(data->flags & LIST_CHANGED) && List_CanScroll(obj, data);

                        if (first >= previous)
                            first = MAX(first - (data->entries_visible - 1), 0);
                        /* Preserve First/VertProp notification order. */
                        data->flags |= LIST_QUIET;
                        set(obj, MUIA_List_First, first);
                        data->flags &= ~(LIST_QUIET | LIST_CHANGED);
                        set(obj, MUIA_List_VertProp_First, first);
                        data->update_pos = previous;
                        data->update = scroll ? (UPDATEMODE_ENTRY | UPDATEMODE_ALL) : UPDATEMODE_ALL;
                        MUI_Redraw(obj, MADF_DRAWUPDATE);
                    }
                    else if (!data->read_only)
                    {
                        /* Paint both cursor rows in one pass. Do not discard
                         * a pending full repaint accumulated in quiet mode. */
                        if (data->update != UPDATEMODE_ALL)
                            data->update = UPDATEMODE_NEEDED;
                        if (!(data->flags & LIST_QUIET))
                            MUI_Redraw(obj, MADF_DRAWUPDATE);
                        else
                            data->flags |= LIST_CHANGED;
                    }

                    /* Make new active entry visible (if there is one and
                       list is visible) */
                    if (new_entries_active != -1
                        && (_flags(obj) & MADF_SETUP))
                    {
                        DoMethod(obj, MUIM_List_Jump,
                            MUIV_List_Jump_Active);
                    }
                }
            }
            break;

        case MUIA_List_First:
            data->update_pos = data->entries_first;
            data->entries_first = tag->ti_Data;
            if (!(data->flags & LIST_QUIET))
            {
                data->update = data->update == 0 && List_CanScroll(obj, data)
                    ? (UPDATEMODE_ENTRY | UPDATEMODE_ALL) : UPDATEMODE_ALL;
                MUI_Redraw(obj, MADF_DRAWUPDATE);
                if (data->vertprop_first != tag->ti_Data)
                {
                    set(obj, MUIA_List_VertProp_First, tag->ti_Data);
                }
            }
            else
            {
                data->update = UPDATEMODE_ALL;
                data->flags |= LIST_CHANGED;
            }
            break;

        case MUIA_List_Visible:    /* get-only; set only to notify */
            break;

        case MUIA_List_Entries:
            if (data->confirm_entries_num == tag->ti_Data)
            {
                data->entries_num = tag->ti_Data;
                if (!(data->flags & LIST_QUIET))
                {
                    set(obj, MUIA_List_VertProp_Entries, data->entries_num);
                }
            }
            else
            {
                D(bug("Bug: confirm_entries != MUIA_List_Entries!\n"));
            }
            break;

        case MUIA_List_Quiet:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_QUIET);
            if (!(data->flags & LIST_QUIET))
            {
                if (data->flags & LIST_HORIZCHECK)
                    List_CheckHoriz(cl, obj);
                if (data->flags & LIST_CHANGED)
                {
                    MUI_Redraw(obj, MADF_DRAWUPDATE);
                    if (data->entries_num != XGET(obj, MUIA_List_VertProp_Entries))
                        set(obj, MUIA_List_VertProp_Entries, data->entries_num);
                    if (data->entries_first != XGET(obj, MUIA_List_VertProp_First))
                        set(obj, MUIA_List_VertProp_First, data->entries_first);
                }
                data->flags &= ~LIST_CHANGED;
            }
            break;

        case MUIA_Selected:
            /* Swallow this so the Area class doesn't redraw us */
            tag->ti_Tag = TAG_IGNORE;
            break;

        case MUIA_Disabled:
            if (_flags(obj) & MADF_SETUP)
            {
                /* Stop listening for events we only listen to when mouse
                   button is down: we will not be informed of the button
                   being released */
                List_DragEvents(obj, data, FALSE);
            }
            break;

        case MUIA_Listview_DoubleClick: /* private set */
            data->doubleclick = tag->ti_Data != 0;
            break;

        case MUIA_Listview_ScrollerPos: /* private set */
            data->scroller_pos = tag->ti_Data;
            List_HandleScrollerPos(cl, obj);
            break;

        case MUIA_Listview_DragType:
            data->drag_type = tag->ti_Data;
            set(obj, MUIA_Draggable,
                tag->ti_Data != MUIV_Listview_DragType_None);
            break;
        }
    }

    return List_SuperSet(cl, obj, msg);
}

/**************************************************************************
 OM_GET
**************************************************************************/
IPTR List__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
/* small macro to simplify return value storage */
#define STORE *(msg->opg_Storage)
    struct MUI_ListData *data = INST_DATA(cl, obj);

    switch (msg->opg_AttrID)
    {
    case MUIA_List_SortColumn: STORE = data->sort_column; return TRUE;
    case MUIA_List_Stripes: STORE = data->stripes; return TRUE;
    case MUIA_List_Editable: STORE = data->editable; return TRUE;
    case MUIA_List_TitleClick: STORE = data->title_click; return TRUE;
    case MUIA_List_Entries:
        STORE = data->entries_num;
        return 1;
    case MUIA_List_First:
        STORE = data->entries_first;
        return 1;
    case MUIA_List_Active:
        STORE = data->entries_active;
        return 1;
    case MUIA_List_InsertPosition:
        STORE = data->insert_position;
        return 1;
    case MUIA_List_ColumnOrder:
        for (LONG i = 0; i < data->columns; i++)
            ((UBYTE *)msg->opg_Storage)[i] = data->ci[i].order_id;
        return TRUE;
    case MUIA_List_TitleArray:
        STORE = (IPTR)data->title_array;
        return TRUE;
    case MUIA_List_Title:
        STORE = (IPTR) data->title;
        return 1;
    case MUIA_List_VertProp_Entries:
        STORE = data->vertprop_entries;
        return 1;
    case MUIA_List_VertProp_Visible:
    case MUIA_List_Visible:
        STORE = data->vertprop_visible;
        return 1;
    case MUIA_List_VertProp_First:
        STORE = data->vertprop_first;
        return 1;
    case MUIA_List_Format:
        STORE = (IPTR) data->format;
        return 1;
    case MUIA_List_AutoVisible:
        STORE = data->flags & LIST_AUTOVISIBLE;
        return 1;
    case MUIA_List_ShowDropMarks:
        STORE = data->flags & LIST_SHOWDROPMARKS;
        return 1;
    case MUIA_List_DragSortable:
        STORE = data->flags & LIST_DRAGSORTABLE;
        return 1;
    case MUIA_List_DropMark:
        STORE = data->drop_mark;
        return 1;
    case MUIA_Listview_ClickColumn:
        STORE = data->click_column;
        return 1;
    case MUIA_Listview_DoubleClick:
        STORE = data->doubleclick;
        return 1;
    case MUIA_Listview_SelectChange:
        STORE = FALSE;
        return 1;
    case MUIA_Listview_List:
        STORE = (IPTR)obj;
        return 1;
    case MUIA_Listview_DefClickColumn:
        STORE = data->def_click_column;
        return 1;
    case MUIA_Listview_DragType:
        STORE = data->drag_type;
        return 1;
    }

    if (DoSuperMethodA(cl, obj, (Msg) msg))
        return 1;
    return 0;
#undef STORE
}

/* The three highlight images, list_cursor, list_select and list_selcur,
 * as an array (they are next to each other in MUI_ListData) */
static inline struct MUI_ImageSpec_intern **List_Highlights(
    struct MUI_ListData *data)
{
    return &data->list_cursor;
}
_Static_assert(offsetof(struct MUI_ListData, list_selcur)
    == offsetof(struct MUI_ListData, list_cursor) + 2 * sizeof(APTR),
    "the highlight images are an array");

/**************************************************************************
 MUIM_Setup
**************************************************************************/
/* Clean up and disconnect a list image if it is set up or connected: an
 * image of a list that was never shown has neither, and MUIM_Cleanup on
 * it went through a NULL render info. */
static void ListImageCleanup(Object *img)
{
    if (_flags(img) & MADF_SETUP)
        DoMethod(img, MUIM_Cleanup);
    if (muiNotifyData(img)->mnd_GlobalInfo)
        DoMethod(img, MUIM_DisconnectParent);
}

IPTR List__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);

    /* The horizontal scroller (MUI 3.9) lives inside the entry area and is
     * shown by the layout when the columns need it. It is made at the
     * first setup, before the group sets up its children, not with the
     * list: a list in a window that is never opened then costs no
     * scroller (a group, a prop and two buttons, ~1.3 KB). MUI 3.8 has no
     * horizontal list scroller: behaving as MUI 3.8, a list gets one only
     * when the program asks for it (MUIA_List_HScrollerVisibility
     * Always, a MUI 4 attribute). */
    if (!data->horiz && !data->area_replaced
        && (!ZUNE_APP_MUI38(obj) || data->hscroll_visibility
            == MUIV_List_HScrollerVisibility_Always))
    {
        data->horiz = ScrollbarObject,
            MUIA_Group_Horiz, TRUE,
            MUIA_ShowMe, FALSE,
            End;
        if (data->horiz)
        {
            DoMethod(obj, OM_ADDMEMBER, (IPTR) data->horiz);
            DoMethod(data->horiz, MUIM_Notify, MUIA_Prop_First,
                MUIV_EveryTime, (IPTR) obj, 4, MUIM_CallHook,
                (IPTR) &list_hook, PROP_HORIZ_FIRST, MUIV_TriggerValue);
        }
    }

    if (!DoSuperMethodA(cl, obj, (Msg) msg))
        return 0;

    data->prefs_refresh = muiGlobalInfo(obj)->mgi_Prefs->list_refresh;
    data->prefs_linespacing =
        muiGlobalInfo(obj)->mgi_Prefs->list_linespacing;
    data->prefs_smoothed = muiGlobalInfo(obj)->mgi_Prefs->list_smoothed;
    data->prefs_smoothval = muiGlobalInfo(obj)->mgi_Prefs->list_smoothval;

    data->list_cursor =
        zune_imspec_setup(MUII_ListCursor, muiRenderInfo(obj));
    data->list_select =
        zune_imspec_setup(MUII_ListSelect, muiRenderInfo(obj));
    data->list_selcur =
        zune_imspec_setup(MUII_ListSelCur, muiRenderInfo(obj));

    data->prefs_multi = muiGlobalInfo(obj)->mgi_Prefs->list_multi;
    if (data->multiselect == MUIV_Listview_MultiSelect_Default)
    {
        if (data->prefs_multi == LISTVIEW_MULTI_SHIFTED)
            data->multiselect = MUIV_Listview_MultiSelect_Shifted;
        else
            data->multiselect = MUIV_Listview_MultiSelect_Always;
    }

    struct ListImage *li;
    ForeachNode((struct List *)&data->images, li) {
        if (_flags(li->obj) & MADF_SETUP)
            continue;   /* already set up by CreateImage */
        DoMethod(li->obj, MUIM_ConnectParent, (IPTR) obj);
        DoSetupMethod(li->obj, muiRenderInfo(obj));
    }

    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) &data->ehn);

    return 1;
}

/**************************************************************************
 MUIM_Cleanup
**************************************************************************/
IPTR List__MUIM_Cleanup(struct IClass *cl, Object *obj,
    struct MUIP_Cleanup *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->edit_object) ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);

    zune_imspec_cleanup(data->list_cursor);
    zune_imspec_cleanup(data->list_select);
    zune_imspec_cleanup(data->list_selcur);

    /* Undo Setup's image setup: images still listed at every window
     * close were set up again at the next open and leaked each time. */
    {
        struct ListImage *li;
        ForeachNode((struct List *)&data->images, li)
            ListImageCleanup(li->obj);
    }

    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) &data->ehn);
    data->mouse_click = 0;

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/**************************************************************************
 MUIM_AskMinMax
**************************************************************************/
/* MUI 3.9 (20.x) shows a list title as a row of title buttons above the
 * framed entry area, one per column, with the scroller beside both
 * (tests/native/barfmt.c). The strip is the font height plus a 1 pixel
 * bevel and 2 pixels of spacing above and below. */
static WORD List_TitleStrip(Object *obj, struct MUI_ListData *data)
{
    if (!data->title || data->area_replaced || !_font(obj))
        return 0;
    /* MUI 3.8 shows the title as the first row of the entry area
     * (demopix: MUI-Demo's Dirlist) */
    if ((_flags(obj) & MADF_SETUP) && ZUNE_APP_MUI38(obj))
        return 0;
    return _font(obj)->tf_YSize + 6;
}


/* The list group's layout: the entry area, beside the vertical scroller
 * and below the title buttons, with the horizontal scroller inside it */
static IPTR List_LayoutFunction(register struct Hook *hook __asm("a0"),
    register Object *obj __asm("a2"),
    register struct MUI_LayoutMsg *lm __asm("a1"))
{
    struct MUI_ListData *data = INST_DATA(List_Class, obj);
    Object *vert = data->vert && (_flags(data->vert) & MADF_SHOWME)
        ? data->vert : NULL;

    switch (lm->lm_Type)
    {
    case MUILM_MINMAX:
        {
            struct MUI_MinMax *mm = &lm->lm_MinMax;

            mm->MinWidth = _minwidth(data->area);
            mm->DefWidth = _defwidth(data->area);
            mm->MaxWidth = _maxwidth(data->area);
            mm->MinHeight = _minheight(data->area);
            mm->DefHeight = _defheight(data->area);
            mm->MaxHeight = _maxheight(data->area);
            if (data->area_replaced)
            {
                /* Listtree's NListtree area: sized as MUI 3.8's own
                 * Listtree, a List (tests/lean/probe/lvrows.c), not by
                 * NList's 240-pixel default */
                LONG row = MAX(_font(obj)->tf_YSize
                    + data->prefs_linespacing, data->entry_minheight);

                mm->MinWidth = _subwidth(data->area) + 40;
                mm->DefWidth = _subwidth(data->area) + 100;
                mm->MinHeight = _subheight(data->area) + 3 * row;
                mm->DefHeight = _subheight(data->area) + 8 * row;
            }
            if (vert)
            {
                mm->MinWidth += _minwidth(vert);
                mm->DefWidth += _minwidth(vert);
                mm->MaxWidth = MIN((LONG)mm->MaxWidth + _minwidth(vert),
                    MUI_MAXMAX);
                mm->MinHeight = MAX(mm->MinHeight, _minheight(vert));
                mm->DefHeight = MAX(mm->DefHeight, _defheight(vert));
                mm->MaxHeight = MIN(mm->MaxHeight, _maxheight(vert));
            }
            return 0;
        }
    case MUILM_LAYOUT:
        {
            LONG width = lm->lm_Layout.Width, height = lm->lm_Layout.Height;
            LONG vw = vert ? _minwidth(vert) : 0, left = 0;
            WORD strip = List_TitleStrip(obj, data);

            if (vert)
            {
                if (data->scroller_pos == MUIV_Listview_ScrollerPos_Left)
                    left = vw;
                MUI_Layout(vert, left ? 0 : width - vw, 0, vw, height, 0);
            }
            /* the title buttons take the top of the entry area's place */
            if (strip >= height)
                strip = 0;
            MUI_Layout(data->area, left, strip, width - vw, height - strip,
                0);
            if (data->horiz)
                zune_list_layout_horiz(obj, data, left, strip, width - vw,
                    height - strip);
            return TRUE;
        }
    }
    return 0;
}

/* The font's height and width and the line spacing setting, as one key */
static inline ULONG List_FontSize(struct TextFont *f, struct MUI_ListData *data)
{
    return ((ULONG)f->tf_YSize << 16) | ((ULONG)(UBYTE)f->tf_XSize << 8)
        | (UBYTE)data->prefs_linespacing;
}

/* Whether the entries' sizes from the last CalcWidths still hold: nothing
 * changed them (LIST_DIMSVALID), the font and the line spacing are the
 * same (a setup after a cleanup, an uniconify, keeps them), no column holds
 * objects (their
 * sizes come from their own setup), no images (CreateImage: set up again
 * with the window) */
static BOOL List_DimsValid(Object *obj, struct MUI_ListData *data)
{
    LONG j;

    if (!(data->flags & LIST_DIMSVALID) || data->dims_font != _font(obj)
        || data->dims_fontsize != List_FontSize(_font(obj), data)
        || !IsListEmpty((struct List *)&data->images))  /* set up again */
        return FALSE;
    for (j = 0; j < data->columns; j++)
        if (data->ci[j].is_object)
            return FALSE;
    return TRUE;
}

IPTR List__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    WORD strip = List_TitleStrip(obj, data);

    DoSuperMethodA(cl, obj, (Msg) msg);

    /* Every entry is measured again only when something that changes the
     * sizes came since the last time (List_Dispatcher clears the flag): a
     * relayout of an unchanged list measured all its entries */
    if (!List_DimsValid(obj, data))
    {
        CalcWidths(cl, obj);
        data->flags |= LIST_DIMSVALID;
        data->dims_font = _font(obj);
        data->dims_fontsize = List_FontSize(_font(obj), data);
    }

    if ((data->flags & LIST_ADJUSTWIDTH) && (data->entries_num > 0))
    {
        msg->MinMaxInfo->MinWidth += data->entries_maxwidth;
        msg->MinMaxInfo->DefWidth = msg->MinMaxInfo->MinWidth;
        msg->MinMaxInfo->MaxWidth = msg->MinMaxInfo->MinWidth;
    }
    else
    {
        /* MUI 3.8 (measured with thick and thin frames and in the
         * fitting stages): 40 pixels of content at least, 100 by
         * default, inside the frame. The frame belongs to the entry
         * area, whose MinMax the group already included. */
        msg->MinMaxInfo->MinWidth +=
            MAX(40 - _minwidth(data->area) + _subwidth(data->area), 0);
        msg->MinMaxInfo->DefWidth +=
            MAX(100 - _defwidth(data->area) + _subwidth(data->area), 0);
        msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    }

    if ((data->flags & LIST_ADJUSTHEIGHT) && (data->entries_num > 0))
    {
        msg->MinMaxInfo->MinHeight += data->entries_totalheight + strip;
        msg->MinMaxInfo->DefHeight = msg->MinMaxInfo->MinHeight;
        msg->MinMaxInfo->MaxHeight = msg->MinMaxInfo->MinHeight;
    }
    else
    {
        /* Three rows minimum and eight by default, from the font (or
         * MUIA_List_MinLineHeight when higher) rather than the entries.The group result already holds the scroller,
         * so the rows must not be stacked on top of it. */
        LONG row = MAX(_font(obj)->tf_YSize + data->prefs_linespacing,
            data->entry_minheight);

        /* the scroller beside the title strip counts against both */
        LONG frame = _subheight(obj) + _subheight(data->area) + strip;

        msg->MinMaxInfo->MinHeight = MAX(msg->MinMaxInfo->MinHeight,
            frame + 3 * row);
        msg->MinMaxInfo->DefHeight = MAX(msg->MinMaxInfo->DefHeight,
            frame + 8 * row);
        msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    }
    D(bug("List %p minheight=%d, line maxh=%d\n",
            obj, msg->MinMaxInfo->MinHeight, data->entry_maxheight));

    return TRUE;
}

/****i* List.mui/MUIM_Zune_Layout *************************************************
*
*   NAME
*       MUIM_Zune_Layout
*
******************************************************************************
*
*/

WORD List_EntryAreaWidth(struct IClass *cl, Object *obj)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);

    return _mwidth(data->area);
}

IPTR List__MUIM_Layout(struct IClass *cl, Object *obj,
    struct MUIP_Zune_Layout *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    ULONG rc = DoSuperMethodA(cl, obj, (Msg) msg);
    LONG new_entries_first = data->entries_first;

    zune_list_layout_columns(data);
    /* Calc the numbers of entries visible */
    CalcVertVisible(cl, obj);

    /* Ensure active entry is visible if requested */
    if (data->entries_active + 1 >=
        (data->entries_first + data->entries_visible)
        && (data->flags & LIST_AUTOVISIBLE) != 0)
        new_entries_first =
            data->entries_active - data->entries_visible + 1;

    /* Ensure there are no unnecessary empty lines */
    if ((new_entries_first + data->entries_visible >=
            data->entries_num)
        && (data->entries_visible <= data->entries_num))
        new_entries_first = data->entries_num - data->entries_visible;

    /* Always show the start of the list if it isn't long enough to fill the
       view */
    if (data->entries_num <= data->entries_visible)
        new_entries_first = 0;

    if (new_entries_first < 0)
        new_entries_first = 0;

    set(obj, new_entries_first != data->entries_first ?
        MUIA_List_First : TAG_IGNORE, new_entries_first);

    /* So the notify happens */
    set(obj, MUIA_List_VertProp_Visible, data->entries_visible);
    /* the scrollbar's units may have changed with the setup */
    List_SyncVertProp(obj, data);

    ListLayoutEditor(obj, data);
    return rc;
}


/**************************************************************************
 MUIM_Show
**************************************************************************/
IPTR List__MUIM_Show(struct IClass *cl, Object *obj,
    struct MUIP_Show *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    ULONG rc = DoSuperMethodA(cl, obj, (Msg) msg);

    for (LONG i = 0; i < 3; i++)
        zune_imspec_show(List_Highlights(data)[i], obj);
    if (data->edit_object) DoShowMethod(data->edit_object);
    return rc;
}


/**************************************************************************
 MUIM_Hide
**************************************************************************/
IPTR List__MUIM_Hide(struct IClass *cl, Object *obj,
    struct MUIP_Hide *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->edit_object) DoHideMethod(data->edit_object);

    for (LONG i = 0; i < 3; i++)
        zune_imspec_hide(List_Highlights(data)[i]);

    return DoSuperMethodA(cl, obj, (Msg) msg);
}


/* The entry area's clip. On OS 3.1 a clip costs about 1.5 ms
 * (InstallClipRegion twice), more than repainting two rows of text, so
 * a cursor move clips only when something drawn may leave the area.
 * MUI 3.8 does not clip there either. */
struct ListClip
{
    APTR handle;
    BOOL done;
};

static void List_NeedClip(Object *obj, struct MUI_ListData *data,
    struct ListClip *lc)
{
    if (lc && !lc->done)
    {
        lc->handle = MUI_AddClipping(muiRenderInfo(obj), _mleft(data->area),
            _mtop(data->area), _mwidth(data->area), _mheight(data->area));
        lc->done = TRUE;
    }
}

/* zune_text_get_bounds() of a plain text (zune_text_init_plain()) on the
 * list's RastPort, given the list's font in the plain style first, as
 * zune_text_draw() gives it */
static __attribute__((noinline)) void List_PlainBounds(Object *obj,
    ZText *text, ZTextChunk *chunk)
{
    struct RastPort *rp = _rp(obj);
    struct TextFont *font = _font(obj);

    if (rp->Font != font)
        SetFont(rp, font);
    if (rp->AlgoStyle != FS_NORMAL)
        SetSoftStyle(rp, FS_NORMAL, AskSoftStyle(rp));
    text->width = chunk->cwidth = TextLength(rp, chunk->str, chunk->clen);
    text->height = chunk->cheight = font->tf_YSize;
}

/**************************************************************************
 Draw an entry at entry_pos at the given row. To draw the title, set pos to
 ENTRY_TITLE. lc is the entry area's clip, NULL when one is installed.
**************************************************************************/
static __attribute__((noinline)) VOID List_DrawEntry(struct IClass *cl, Object *obj, int entry_pos,
    int y, struct ListClip *lc)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    Object *area = data->area;
    int row_height = data->entry_maxheight;
    int y_offset = y;
    int col, x1, x2;

    /* To be sure we don't draw anything if there is no title */
    if (entry_pos == ENTRY_TITLE) {
        if (!data->title)
            return;
        row_height = data->entries[ENTRY_TITLE].height;
    }

    DisplayEntry(cl, obj, entry_pos);
    x1 = _mleft(area) - data->horiz_first;

    for (col = 0; col < data->columns; col++) {
        ZText *text, local_text;
        ZTextLine local_line;
        ZTextChunk local_chunk;
        BOOL borrowed;
        if (ColumnHidden(data, col)) continue;
        x2 = x1 + ColumnWidth(data, col);
        if (data->ci[col].is_object && entry_pos != ENTRY_TITLE)
        {
            /* the cell clips only to its own rectangle */
            List_NeedClip(obj, data, lc);
            zune_list_draw_cell(obj, (Object *)data->strings[col], x1, y,
                ColumnWidth(data, col), row_height);
            x1 = x2 + data->ci[col].delta + (data->ci[col].bar ? BAR_WIDTH : 0);
            continue;
        }

        borrowed = zune_text_init_plain(&local_text, &local_line, &local_chunk,
            data->preparses[col], data->strings[col]);
        text = borrowed ? &local_text : zune_text_new(data->preparses[col],
            data->strings[col], ZTEXT_ARG_NONE, 0);
        if (text) {
            /*
             * We need the bounds so that embedded
             * images are (correctly) rendered and to
             * vertically align the entry. A plain string's are its
             * length on the RastPort it is drawn on, set up as
             * zune_text_draw() sets it: no RastPort copy.
             */
            if (borrowed && !data->ci[col].elide)
                List_PlainBounds(obj, &local_text, &local_chunk);
            else
                zune_text_get_bounds(text, obj);
            /* centred, rounding down like MUI 3.9, which puts the
             * leading above the text (tests/native/listvisible.c) */
            y_offset = y + (row_height - text->height + 1) / 2;
            if (y_offset < y)
                y_offset = y;
            /* Plain text within the area needs no clip. Its width is
             * TextLength's, as everywhere else in Zune; an italic font
             * may paint past it. */
            if (lc && !lc->done && !(borrowed
                && x1 >= _mleft(area)
                && x1 + text->width - 1 <= _mright(area)
                && y_offset >= _mtop(area)
                && y_offset + text->height - 1 <= _mbottom(area)
                && !(_font(obj)->tf_Style & FSF_ITALIC)))
                List_NeedClip(obj, data, lc);
            /* Note, this was MPEN_SHADOW before */
            SetAPen(_rp(obj), muiRenderInfo(obj)->mri_Pens[MPEN_TEXT]);

            /* an ELIDE column or a laid out format: cut at the column's
             * end; MUI 3.8 draws only the whole characters of a too long
             * entry that end before the area's last column (demopix: the
             * long lines of MUI-Demo's String window); text that fits goes
             * up to the edge (its Floattexts) */
            LONG right = x2;
            if (data->ci[col].elide || data->format_layout)
                text->truncate = TRUE, right = MIN(x2 - 1, _mright(area));
            else if (x1 + text->width - 1 > _mright(area)
                && ZUNE_APP_MUI38(obj))
                text->truncate = TRUE, right = _mright(area) - 1;
            if (data->ci[col].elide >= MUIV_Text_Shorten_Hide)
                zune_text_draw_shortened(text, obj, x1, right, y_offset,
                    data->ci[col].elide);
            else
                zune_text_draw(text, obj, x1, right, y_offset);
            if (!borrowed) zune_text_destroy(text);
        }
        if (entry_pos == ENTRY_TITLE && col == data->sort_column
            && data->ci[col].sortable)
            zune_list_draw_sort_mark(obj, data, x1, y, row_height);
        x1 = x2 + data->ci[col].delta + (data->ci[col].bar ? BAR_WIDTH : 0);
    }
}


/* The column bars from y to bottom, in the title (title TRUE) or beside
 * the entries: MUI 3.9 draws shadow, then shine (tests/native/barfmt.c).
 * A bar that needs the entry area's clip while none is installed (not
 * clipped) stops it: TRUE, and MUIM_Draw installs the clip and calls it
 * again (the bars before are drawn twice, the same pixels). The clip is
 * made in MUIM_Draw's frame, as before: MUI_AddClipping goes deep. */
static BOOL List_DrawBars(Object *obj, struct MUI_ListData *data, LONG y,
    LONG bottom, BOOL clipped, BOOL title)
{
    Object *area = data->area;
    struct RastPort *rp = _rp(obj);
    LONG x1 = _mleft(area) - data->horiz_first;

    for (ULONG col = 0; col < data->columns; col++)
    {
        ULONG halfdelta = data->ci[col].delta / 2;
        if (ColumnHidden(data, col)) continue;
        x1 += ColumnWidth(data, col) + halfdelta;

        if (x1 + (data->ci[col].bar ? BAR_WIDTH : 0) > _mright(area))
            break;

        if (data->ci[col].bar)
        {
            if (!clipped && (x1 < _mleft(area) || (!title && y > bottom)))
                return TRUE;
            SetAPen(rp, _pens(obj)[MPEN_SHADOW]);
            Move(rp, x1, y);
            Draw(rp, x1, bottom);
            SetAPen(rp, _pens(obj)[MPEN_SHINE]);
            Move(rp, x1 + 1, y);
            Draw(rp, x1 + 1, bottom);

            x1 += BAR_WIDTH;
        }
        x1 += data->ci[col].delta - halfdelta;
    }
    return FALSE;
}

/* The background of the entry area's rows from y on, h pixels high, in
 * step with the scrolled entries */
static __attribute__((noinline)) void List_Backfill(Object *obj,
    struct MUI_ListData *data, LONG y, LONG h)
{
    Object *area = data->area;

    DoMethod(obj, MUIM_DrawBackground, _mleft(area), y, _mwidth(area), h,
        0, y - _mtop(area) + data->entries_first * data->entry_maxheight, 0);
}

/**************************************************************************
 MUIM_Draw
**************************************************************************/
IPTR List__MUIM_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    Object *area = data->area;  /* made at OM_NEW, never changes */
    int entry_pos, y;
    struct ListClip clip = { (APTR)-1, FALSE };
    int start, end, exposed_start = 0, exposed_end = 0;
    BOOL scrolling = FALSE, full;
    BOOL scroll_caused_damage = FALSE;
    struct MUI_ImageSpec_intern *highlight;
    IPTR ret = (IPTR)0;
    LONG entries_height, entries_bottom;

    D(bug("[Zune:List] %s()\n", __func__);)

    if (data->flags & LIST_QUIET)
        return ret;

    D(
        bug("[Zune:List] %s: Rendering...\n", __func__);
        bug("[Zune:List] %s: update = %d\n", __func__, data->update);
    )

    ret = DoSuperMethodA(cl, obj, (Msg) msg);

    if (data->area_replaced)
        return ret;

    zune_list_layout_columns(data);

    /* Calculate the title height */
    if (List_TitleStrip(obj, data))
    {
        data->title_height = 0;
        if ((msg->flags & MADF_DRAWUPDATE) == 0
            || data->update == UPDATEMODE_ALL)
        {
            /* the module draws the buttons and leaves its clip for the
             * title row */
            APTR clip = zune_list_draw_title_strip(obj, data);
            List_DrawEntry(cl, obj, ENTRY_TITLE, _top(area)
                - (_font(obj)->tf_YSize + 6) + 2, NULL);  /* the strip */
            MUI_RemoveClipping(muiRenderInfo(obj), clip);
        }
    }
    else if (data->title)
    {
        /* MUI 3.8: a pixel below the title, no separator lines (demopix:
         * MUI-Demo's Dirlist) */
        data->title_height = data->entries[ENTRY_TITLE].height
            + (ZUNE_APP_MUI38(obj) ? 1 : 2);
    }
    else
    {
        data->title_height = 0;
    }

    zune_list_layout_columns(data);
    /* Calc the numbers of entries visible */
    CalcVertVisible(cl, obj);

    entries_height = _mheight(area);
    entries_bottom = _mbottom(area);

    if ((msg->flags & MADF_DRAWUPDATE) == 0 || data->update == UPDATEMODE_ALL)
        List_Backfill(obj, data, _mtop(area), entries_height);

    /* A full repaint or a scroll clips at once, as it nearly always
     * draws a cut-off last row: a clip installed later would wait for
     * the blitter to finish the rows queued so far. Other updates repaint
     * a few rows and clip only when one needs it (List_NeedClip). An
     * empty list without a title draws nothing in the entry area. A
     * scroll whose rows all end inside the area (MUI 3.8 shows whole rows
     * only) is such an update too: two InstallClipRegion calls less per
     * step (opbench "list scroll line"). */
    /* A full repaint whose rows all end inside the area (MUI 3.8's whole
     * rows) needs no clip at once either: a row or text that would leave
     * the area asks for it below (opbench "full redraw", "register page") */
    if (((msg->flags & MADF_DRAWUPDATE) == 0
            || (data->update & UPDATEMODE_ALL))
        && (data->entries_num != 0 || (data->title_height && data->title))
        && !(((msg->flags & MADF_DRAWUPDATE) == 0
                || data->update == UPDATEMODE_ALL
                || data->update == (UPDATEMODE_ENTRY | UPDATEMODE_ALL))
            && data->entries_top_pixel >= _mtop(area)
            && data->entries_top_pixel + data->entries_visible
                * data->entry_maxheight - 1 <= entries_bottom))
        List_NeedClip(obj, data, &clip);

    if ((msg->flags & MADF_DRAWUPDATE) == 0 || data->update == UPDATEMODE_ALL)
    {
        y = _mtop(area);
        /* Draw Title
         */
        if (data->title_height && data->title)
        {
            if (y + data->entries[ENTRY_TITLE].height + 1 > entries_bottom)
                List_NeedClip(obj, data, &clip);
            /* MUI 3.8: no separator lines below it. A list that does
             * not behave as MUI 3.8 has its title in the strip of
             * buttons above the area (List_TitleStrip()), never here, so
             * Zune's old separator (shadow and shine lines) is gone. */
            List_DrawEntry(cl, obj, ENTRY_TITLE, y, &clip);
        }
    }

    y = data->entries_top_pixel;

    start = data->entries_first;
    end = data->entries_first + data->entries_visible;

    if ((msg->flags & MADF_DRAWUPDATE) && data->update == (UPDATEMODE_ENTRY|UPDATEMODE_ALL))
    {
        int diffy = data->entries_first - data->update_pos;
        int top, bottom;
        if (diffy && abs(diffy) < data->entries_visible)
        {
            scroll_caused_damage =
                (_rp(obj)->Layer->Flags & LAYERREFRESH) ? FALSE : TRUE;

            /* the last row may be cut off by the area. ScrollRasterBF:
             * the rows the scroll uncovers get the layer's backfill (none
             * in "redraw without clear", the default), not a fill with the
             * background pen: they are painted below in any case (their
             * background, then the rows), so the fill was one blit for
             * nothing (opbench "list scroll line" -0.6 ms) */
            ScrollRasterBF(_rp(obj), 0, diffy * data->entry_maxheight,
                _mleft(area), y,
                _mright(area),
                MIN(y + data->entry_maxheight * data->entries_visible,
                    entries_bottom));

            scroll_caused_damage =
                scroll_caused_damage
                && (_rp(obj)->Layer->Flags & LAYERREFRESH);

            if (diffy > 0)
            {
                start = end - diffy;
                /* The former bottom row may have been clipped: its newly
                 * visible lower pixels cannot be recovered by the blit.
                 * Only when it was: MUI 3.8's whole rows, centred, are
                 * never cut off, and drawing it again cost a row of text
                 * and the entry clip per step (opbench "list scroll
                 * line", tests/native/syscount.h) */
                if (data->entries_top_pixel + data->entries_visible
                    * data->entry_maxheight - 1 > entries_bottom)
                    start--;
                y = data->entries_top_pixel +
                    (start - data->entries_first) * data->entry_maxheight;
            }
            else
                end = start - diffy;
        }

        top = y;
        bottom = y + (end - start) * data->entry_maxheight;
        if (bottom > entries_bottom)
            List_NeedClip(obj, data, &clip);

        List_Backfill(obj, data, top, bottom - top + 1);
        exposed_start = start;
        exposed_end = end;
        scrolling = TRUE;
        start = data->entries_first;
        end = start + data->entries_visible;
        y = data->entries_top_pixel;
    }

    /* Which rows: all (a full draw: their background is drawn), on a
     * scroll the uncovered ones, the changed ones and the highlighted
     * ones, else the entry update_pos or the rows marked to render (with
     * their background) */
    full = !(msg->flags & MADF_DRAWUPDATE) || data->update == UPDATEMODE_ALL;
    if (end > data->entries_num)
        end = data->entries_num;
    for (entry_pos = start; entry_pos < end; entry_pos++,
        y += data->entry_maxheight)
    {
        struct ListEntry *entry = &data->entries[entry_pos];
        BOOL back;

        if (full)
            back = FALSE;
        else if (scrolling)
        {
            back = (entry->flags & ENTRY_RENDER) != 0;
            if (!back && !(entry->flags & ENTRY_SELECTED)
                && entry_pos != data->entries_active
                && (entry_pos < exposed_start || entry_pos >= exposed_end))
                continue;
        }
        else if (data->update == UPDATEMODE_ENTRY)
        {
            if (entry_pos != data->update_pos)
                continue;
            back = TRUE;
        }
        else if (data->update == UPDATEMODE_NEEDED
            && (entry->flags & ENTRY_RENDER))
            back = TRUE;
        else
            continue;

        /* a cut-off row: its highlight and background too */
        if (y < _mtop(area)
            || y + data->entry_maxheight - 1 > entries_bottom)
            List_NeedClip(obj, data, &clip);

        /* Choose appropriate highlight image */
        if (entry_pos == data->entries_active && !data->read_only)
            highlight = (entry->flags & ENTRY_SELECTED)
                ? data->list_selcur : data->list_cursor;
        else if (entry->flags & ENTRY_SELECTED)
            highlight = data->list_select;
        else
            highlight = NULL;

        /* Draw highlight or background */
        if (highlight != NULL)
        {
            /* A masked highlight must not blend with pixels copied
             * from a different phase of the previous highlight. */
            if (scrolling && !zune_imspec_is_solid(highlight))
                List_Backfill(obj, data, y, data->entry_maxheight);
            zune_imspec_draw(highlight, muiRenderInfo(obj),
                _mleft(area), y, _mwidth(area),
                data->entry_maxheight,
                0, y - data->entries_top_pixel, 0);
        }
        else
        {
            if (back)
                List_Backfill(obj, data, y, data->entry_maxheight);
            if (data->stripes && (entry_pos & 1))
            {
                SetAPen(_rp(obj), _pens(obj)[MPEN_HALFSHINE]);
                RectFill(_rp(obj), _mleft(area), y,
                    _mright(area), y + data->entry_maxheight - 1);
            }
        }
        List_DrawEntry(cl, obj, entry_pos, y, &clip);
        data->entries[entry_pos].flags &= ~ENTRY_RENDER;
    }

    data->update = 0;

    if (scroll_caused_damage)
    {
        if (MUI_BeginRefresh(muiRenderInfo(obj), 0))
        {
            /* Theoretically it might happen that more damage is caused
               after ScrollRaster. By something else, like window movement
               in front of our window. Therefore refresh root object of
               window, not just this object */

            Object *o = NULL;

            get(_win(obj), MUIA_Window_RootObject, &o);
            MUI_Redraw(o, MADF_DRAWOBJECT | MADF_DRAWFRAME);

            MUI_EndRefresh(muiRenderInfo(obj), 0);
        }
    }

    y = _mtop(area);

    if (data->title_height && data->title)
    {
        while (List_DrawBars(obj, data, y,
            y + data->entries[ENTRY_TITLE].height - 1, clip.done, TRUE))
            List_NeedClip(obj, data, &clip);
        y += data->entries[ENTRY_TITLE].height + 1;
    }
    while (List_DrawBars(obj, data, y, entries_bottom, clip.done, FALSE))
        List_NeedClip(obj, data, &clip);

    if (clip.done)
        MUI_RemoveClipping(muiRenderInfo(obj), clip.handle);

    data->flags &= ~LIST_CHANGED;

    if (data->edit_object) MUI_Redraw(data->edit_object, MADF_DRAWOBJECT);
    return 0;
}

/****** List.mui/MUIM_List_Clear *********************************************
*
*   NAME
*       MUIM_List_Clear (V4)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_Clear);
*
*   FUNCTION
*       Removes all entries from the list.
*
******************************************************************************
*
*/

IPTR List__MUIM_Clear(struct IClass *cl, Object *obj,
    struct MUIP_List_Clear *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->edit_object) ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);

    while (data->confirm_entries_num)
    {
        DoMethod(obj, MUIM_List_Destruct,
            (IPTR) data->entries[--data->confirm_entries_num].data,
            (IPTR) data->pool);
        data->flags |= LIST_CHANGED;
    }
    /* Should never fail when shrinking */
    SetListSize(data, 0);

    if (data->confirm_entries_num != data->entries_num)
    {
        SetAttrs(obj, MUIA_List_Entries, 0, MUIA_List_First, 0,
            /* Notify only when no entry was active */
            data->entries_active !=
            MUIV_List_Active_Off ? MUIA_List_Active : TAG_DONE,
            MUIV_List_Active_Off, TAG_DONE);

        data->update = UPDATEMODE_ALL;
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    }

    return 0;
}

/****** List.mui/MUIM_List_Exchange ******************************************
*
*   NAME
*       MUIM_List_Exchange (V4)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_Exchange, LONG pos1, LONG pos2);
*
*   FUNCTION
*       Exchange two entries' positions.
*
*   INPUTS
*       pos1 - the current index of the first entry that should be moved, or
*           one of these special values:
*               MUIV_List_Exchange_Active: the active entry.
*               MUIV_List_Exchange_Top: the first entry.
*               MUIV_List_Exchange_Bottom: the last entry.
*       pos2 - the index of the entry that the first entry should be exchanged
*           with, or one of these special values:
*               MUIV_List_Exchange_Active: the active entry.
*               MUIV_List_Exchange_Top: the first entry.
*               MUIV_List_Exchange_Bottom: the last entry.
*               MUIV_List_Exchange_Next: the next entry after pos1.
*               MUIV_List_Exchange_Previous: the previous entry before pos1.
*
*   NOTES
*       This method will do nothing if either index is greater than the last
*       index in the list, or if MUIV_List_Exchange_Next or
*       MUIV_List_Exchange_Previous imply an index outside the list.
*
*   SEE ALSO
*       MUIM_List_Move
*
******************************************************************************
*
*/

IPTR List__MUIM_Exchange(struct IClass *cl, Object *obj,
    struct MUIP_List_Exchange *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->edit_object) ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);
    LONG pos1, pos2;

    switch (msg->pos1)
    {
    case MUIV_List_Exchange_Top:
        pos1 = 0;
        break;
    case MUIV_List_Exchange_Active:
        pos1 = data->entries_active;
        break;
    case MUIV_List_Exchange_Bottom:
        pos1 = data->entries_num - 1;
        break;
    default:
        pos1 = msg->pos1;
    }

    switch (msg->pos2)
    {
    case MUIV_List_Exchange_Top:
        pos2 = 0;
        break;
    case MUIV_List_Exchange_Active:
        pos2 = data->entries_active;
        break;
    case MUIV_List_Exchange_Bottom:
        pos2 = data->entries_num - 1;
        break;
    case MUIV_List_Exchange_Next:
        pos2 = pos1 + 1;
        break;
    case MUIV_List_Exchange_Previous:
        pos2 = pos1 - 1;
        break;
    default:
        pos2 = msg->pos2;
    }

    if (pos1 >= 0 && pos1 < data->entries_num && pos2 >= 0
        && pos2 < data->entries_num && pos1 != pos2)
    {
        ULONG *a = ROW_L(data->entries, pos1), *b = ROW_L(data->entries, pos2);
        ULONG t = a[0]; a[0] = b[0]; b[0] = t;
        t = a[1]; a[1] = b[1]; b[1] = t;

        data->update = UPDATEMODE_ENTRY;
        data->update_pos = pos1;
        MUI_Redraw(obj, MADF_DRAWUPDATE);

        data->update = UPDATEMODE_ENTRY;
        data->update_pos = pos2;
        MUI_Redraw(obj, MADF_DRAWUPDATE);

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/**************************************************************************
 MUIM_List_Redraw
**************************************************************************/
IPTR List__MUIM_Redraw(struct IClass *cl, Object *obj,
    struct MUIP_List_Redraw *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);

    D(bug("[Zune:List] %s()\n", __func__);)

    if (msg->pos == MUIV_List_Redraw_All)
    {
        CalcWidths(cl, obj);
        List_CheckHoriz(cl, obj);
        data->update = UPDATEMODE_ALL;
        if (!(data->flags & LIST_QUIET))
        {
            MUI_Redraw(obj, MADF_DRAWUPDATE);
        }
        else
            data->flags |= LIST_CHANGED;
    }
    else
    {
        LONG pos = -1;
        if (msg->pos == MUIV_List_Redraw_Active)
            pos = data->entries_active;
        else if (msg->pos == MUIV_List_Redraw_Entry)
        {
            LONG i;
            for (i = 0; i < data->entries_num; i++)
                if (data->entries[i].data == msg->entry)
                {
                    pos = i;
                    break;
                }
        }
        else
            pos = msg->pos;

        if (pos != -1)
        {
            data->entries[pos].flags |= ENTRY_RENDER;
            if (!(data->flags & LIST_QUIET))
            {
                if (CalcDimsOfEntry(cl, obj, pos))
                    data->update = UPDATEMODE_ALL;
                else
                {
                    data->update = UPDATEMODE_ENTRY;
                    data->update_pos = pos;
                }

                MUI_Redraw(obj, MADF_DRAWUPDATE);
            }
            else
            {
                if (CalcDimsOfEntry(cl, obj, pos))
                    data->update = UPDATEMODE_ALL;
                else if (!(data->update & UPDATEMODE_ALL))
                    data->update = UPDATEMODE_NEEDED;
                data->flags |= LIST_CHANGED;
            }
        }
    }

    return 0;
}

/****** List.mui/MUIM_List_Remove ********************************************
*
*   NAME
*       MUIM_List_Remove (V4)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_Remove, LONG pos);
*
*   FUNCTION
*       Removes entries from the list. If a destruct hook has been
*       installed, it will be called for the removed entry.
*
*   INPUTS
*       pos - the index of the entry to be removed. The following
*           special values can also be used:
*           MUIV_List_Remove_First: remove the first entry.
*           MUIV_List_Remove_Last: remove the last entry.
*           MUIV_List_Remove_Active: remove the active entry.
*           MUIV_List_Remove_Selected: remove all selected entries
*               (or the active entry if there are no selected entries).
*
*   NOTES
*       When the active entry is removed, the next entry becomes active
*       (if there is no entry below the active entry, the previous entry
*       becomes active instead).
*
*   SEE ALSO
*       MUIM_List_Insertsingle, MUIM_List_Insert, MUIA_List_DestructHook.
*
******************************************************************************
*
* It was not possible to use MUIM_List_NextSelected here because that method
* may skip entries if entries are removed during an iteration.
*
*/

IPTR List__MUIM_Remove(struct IClass *cl, Object *obj,
    struct MUIP_List_Remove *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->edit_object) ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);
    LONG pos, new_act, i;
    Tag active_tag = TAG_DONE;

    if (!data->entries_num)
        return 0;

    switch (msg->pos)
    {
    case MUIV_List_Remove_First:
    case MUIV_List_Remove_Selected:
        pos = 0;
        break;

    case MUIV_List_Remove_Active:
        pos = data->entries_active;
        break;

    case MUIV_List_Remove_Last:
        pos = data->entries_num - 1;
        break;

    default:
        pos = msg->pos;
        break;
    }

    if (pos < 0 || pos >= data->entries_num)
        return 0;

    new_act = data->entries_active;

    /* All selected entries in one pass: removing them one at a time moved
     * the rest of the list once per entry (a second for 1000 of 2000
     * entries on a 68000). Without a selected one, the active entry goes. */
    if (msg->pos == MUIV_List_Remove_Selected)
    {
        LONG n = data->confirm_entries_num, w;
        LONG active = data->entries_active;

        for (i = 0; i < n; i++)
            if (data->entries[i].flags & ENTRY_SELECTED)
                break;
        if (i < n)
        {
            for (w = i; i < n; i++)
            {
                if (!(data->entries[i].flags & ENTRY_SELECTED))
                {
                    data->entries[w++] = data->entries[i];
                    continue;
                }
                DoMethod(obj, MUIM_List_Destruct,
                    (IPTR) data->entries[i].data, (IPTR) data->pool);
                if (i < active)
                {
                    new_act--;
                    active_tag = MUIA_List_Active;
                }
                else if (i == active)
                    active_tag = MUIA_List_Active;
            }
            data->confirm_entries_num = w;
            goto removed;
        }
        if (data->confirm_entries_num != data->entries_num
            || data->entries_active == MUIV_List_Active_Off)
            goto removed;
        pos = data->entries_active;
    }

    DoMethod(obj, MUIM_List_Destruct, (IPTR) data->entries[pos].data,
        (IPTR) data->pool);
    memmove(&data->entries[pos], &data->entries[pos + 1],
        (data->entries_num - (pos + 1)) * sizeof(struct ListEntry));
    data->confirm_entries_num--;
    if (pos <= new_act)
    {
        if (pos < new_act)
            new_act--;
        active_tag = MUIA_List_Active;
    }

removed:
    /* Update entries count prior to range check */
    SetAttrs(obj, MUIA_List_Entries, data->confirm_entries_num, TAG_DONE);

    /* Ensure that the active element is in a valid range (it might become
     * MUIV_List_Active_Off (-1), but that's OK) */
    if (new_act >= data->entries_num)
        new_act = data->entries_num - 1;

    SetAttrs(obj,
        active_tag, new_act,   /* Inform only if necessary (for notify) */
        TAG_DONE);

    data->flags |= LIST_CHANGED;
    data->update = UPDATEMODE_ALL;
    if (!(data->flags & LIST_QUIET))
        MUI_Redraw(obj, MADF_DRAWUPDATE);

    return 0;
}

/****** List.mui/MUIM_List_Select ********************************************
*
*   NAME
*       MUIM_List_Select (V4)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_Select, LONG pos, LONG seltype, LONG *state);
*
*   FUNCTION
*       Selects or deselects entries in the list and/or enquires about their
*       current selection state. If a multiselection test hook has been
*       installed via MUIA_List_MultiTestHook, it will be called to validate
*       the requested selection.
*
*       This method may also be used to count the number of selected entries
*       (see below).
*
*   INPUTS
*       pos - the index of the entry to be selected. The following
*           special values can also be used:
*           MUIV_List_Select_Active: Select the active entry.
*           MUIV_List_Select_All: Select all entries.
*       seltype - the new selection state; one of the following:
*           MUIV_List_Select_Off: Select the entry.
*           MUIV_List_Select_On: Deselect the entry.
*           MUIV_List_Select_Toggle: Switch to the alternate selection state.
*           MUIV_List_Select_Ask: Do not change the selection state, just
*               retrieve the current state. If 'pos' is MUIV_List_Select_All,
*               the number of selected entries will be retrieved instead (V9).
*       state - a pointer in which to fill in the previous selection state
*           (either MUIV_List_Select_On or MUIV_List_Select_Off) or the
*           number of selected entries. May be NULL.
*
*   NOTES
*       If pos is MUIV_List_Select_All and seltype is not
*       MUIV_List_Select_Ask, the value filled in 'state' is undefined.
*
*   SEE ALSO
*       MUIA_List_Active, MUIA_List_MultiTestHook.
*
******************************************************************************
*
*/

IPTR List__MUIM_Select(struct IClass *cl, Object *obj,
    struct MUIP_List_Select *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    LONG pos, i, count, selcount = 0, state = 0;
    BOOL multi_allowed = TRUE, new_multi_allowed, new_select_state = FALSE;

    /* Establish the range of entries affected */
    switch (msg->pos)
    {
    case MUIV_List_Select_Active:
        pos = data->entries_active;
        if (pos == MUIV_List_Active_Off)
            count = 0;
        else
            count = 1;
        break;

    case MUIV_List_Select_All:
        pos = 0;
        count = data->entries_num;
        break;

    default:
        pos = msg->pos;
        count = 1;
        if (pos < 0 || pos >= data->entries_num)
            return 0;
        break;
    }

    if (msg->seltype != MUIV_List_Select_Ask && data->multi_test_hook != NULL)
    {
        /* Count selected entries and disallow selection of additional
           entries if there is a currently selected entry that is not
           multi-selectable */
        for (i = 0; i < data->entries_num && multi_allowed; i++)
        {
            if (data->entries[i].flags & ENTRY_SELECTED)
            {
                selcount++;
                if (data->multi_test_hook != NULL && selcount == 1)
                    multi_allowed = CallHookPkt(data->multi_test_hook, NULL,
                        data->entries[i].data);
            }
        }
    }

    /* Change or check state of each entry in the range */
    for (i = pos; i < pos + count; i++)
    {
        state = data->entries[i].flags & ENTRY_SELECTED;
        switch (msg->seltype)
        {
        case MUIV_List_Select_Off:
            new_select_state = FALSE;
            break;

        case MUIV_List_Select_On:
            new_select_state = TRUE;
            break;

        case MUIV_List_Select_Toggle:
            new_select_state = !state;
            break;

        default:
            if (data->entries[i].flags & ENTRY_SELECTED)
                selcount++;
            break;
        }

        if (msg->seltype != MUIV_List_Select_Ask)
        {
            if (new_select_state && !state)
            {
                /* Check if there is potential to select an additional entry */
                if (multi_allowed || selcount == 0)
                {
                    /* Check if the entry to be selected is multi-selectable */
                    if (data->multi_test_hook != NULL)
                        new_multi_allowed = CallHookPkt(data->multi_test_hook,
                            NULL, data->entries[i].data);
                    else
                        new_multi_allowed = TRUE;

                    /* Check if the entry to be selected can be selected at
                       the same time as the already selected entries */
                    if (new_multi_allowed || selcount == 0)
                    {
                        /* Select the entry and update the selection count
                           and flag */
                        data->entries[i].flags |= ENTRY_SELECTED;
                        selcount++;

                        multi_allowed = new_multi_allowed;
                    }
                }
            }
            else if (!new_select_state && state)
            {
                data->entries[i].flags &= ~ENTRY_SELECTED;
                    selcount--;
            }
        }
    }

    /* Report old state or number of selected entries */
    if (msg->info)
    {
        if (msg->pos == MUIV_List_Select_All
            && msg->seltype == MUIV_List_Select_Ask)
            *msg->info = selcount;
        else
            *msg->info = state;
    }

    /* Redraw unless it was just an enquiry */
    if (msg->seltype != MUIV_List_Select_Ask)
    {
        if (count > 1)
            data->update = UPDATEMODE_ALL;
        else
        {
            data->update = UPDATEMODE_ENTRY;
            data->update_pos = pos;
        }
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    }
   
    return 0;
}

/****** List.mui/MUIM_List_Insert ********************************************
*
*   NAME
*       MUIM_List_Insert (V4)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_Insert, APTR *entries, LONG count, LONG pos);
*
*   FUNCTION
*       Adds multiple entries to the list. If a construct hook has been
*       installed, the results of passing the entries to this hook will be
*       inserted.
*
*   INPUTS
*       entries - an array of entries to be inserted.
*       count - the number of entries to insert. A special value of -1 may be
*           used, indicating that the array of entries is NULL-terminated.
*       pos - the index at which to insert the new entries. The following
*           special values can also be used:
*           MUIV_List_Insert_Top: insert at index 0.
*           MUIV_List_Insert_Bottom: insert after all existing entries.
*           MUIV_List_Insert_Active: insert at the index of the active entry
*               (or at index 0 if there is no active entry).
*           MUIV_List_Insert_Sorted: keep the list sorted.
*
*   SEE ALSO
*       MUIM_List_InsertSingle, MUIM_List_Remove, MUIA_List_ConstructHook.
*
******************************************************************************
*
*/

/* MUIM_List_Compare of two entries' data, turned round for a DESCENDING
 * sort column; MUIM_List_SortEntries (listx.c) compares through it too */
LONG zune_list_compare(Object *obj, struct MUI_ListData *data, APTR a, APTR b)
{
    struct MUIP_List_Compare msg = {MUIM_List_Compare, a, b, {data->sort_column}, 0};
    LONG result = (LONG)DoMethodA(obj, (Msg)&msg);
    if (data->sort_column >= 0 && data->sort_column < data->columns &&
        data->ci[data->sort_column].descending)
        result = result < 0 ? 1 : result > 0 ? -1 : 0;
    return result;
}
#define CompareEntries(obj, data, a, b) \
    zune_list_compare((obj), (data), (a)->data, (b)->data)

/* Insert one appended entry into the sorted prefix, after equal keys. */
static LONG InsertSortedEntry(Object *obj, struct MUI_ListData *data, struct ListEntry *entries, LONG at)
{
    ULONG d, hf;
    LONG lo = 0, hi = at, mid;
    while (lo < hi)
    {
        mid = lo + (hi - lo) / 2;
        if (CompareEntries(obj, data, &entries[mid], &entries[at]) <= 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    d = ROW_L(entries, at)[0], hf = ROW_L(entries, at)[1];
    while (at > lo)
    {
        entries[at] = entries[at - 1];
        entries[at].flags |= ENTRY_RENDER;
        at--;
    }
    ROW_L(entries, lo)[0] = d;
    ROW_L(entries, lo)[1] = hf | ENTRY_RENDER;
    return lo;
}

IPTR List__MUIM_Insert(struct IClass *cl, Object *obj,
    struct MUIP_List_Insert *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->edit_object) ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);
    LONG pos, count, sort, active, until;
    BOOL adjusted = FALSE, failed = FALSE;
    APTR *toinsert;

    count = msg->count;
    sort = 0;

    if (count == -1)
    {
        /* Count the number of entries */
        for (count = 0; msg->entries[count] != NULL; count++)
            ;
    }

    if (count <= 0)
        return ~0;

    switch (msg->pos)
    {
    case MUIV_List_Insert_Top:
        pos = 0;
        break;

    case MUIV_List_Insert_Active:
        if (data->entries_active != -1)
            pos = data->entries_active;
        else
            pos = 0;
        break;

    case MUIV_List_Insert_Sorted:
        pos = data->entries_num;
        sort = 1;               /* we sort'em later */
        break;

    case MUIV_List_Insert_Bottom:
        pos = data->entries_num;
        break;

    default:
        if (msg->pos > data->entries_num)
            pos = data->entries_num;
        else if (msg->pos < 0)
            pos = 0;
        else
            pos = msg->pos;
        break;
    }
    data->insert_position = pos;

    if (!(SetListSize(data, data->entries_num + count)))
        return ~0;

    until = pos + count;
    toinsert = msg->entries;

    memmove(&data->entries[until], &data->entries[pos],
        (data->entries_num - pos) * sizeof(struct ListEntry));

    while (pos < until)
    {
        APTR entry = (APTR)DoMethod(obj, MUIM_List_Construct,
            (IPTR)*toinsert, (IPTR)data->pool);
        if (!entry)
        {
            /* The reserved gap extends past entries_num. Move only the
             * original tail, never a negative (entries_num - until) size. */
            memmove(&data->entries[pos], &data->entries[until],
                (data->entries_num - data->insert_position)
                * sizeof(struct ListEntry));
            count = pos - data->insert_position;
            if (!count) return ~0;
            failed = TRUE;
            break;
        }

        data->entries[pos].data = entry;
        data->entries[pos].height = 0;
        data->entries[pos].flags = ENTRY_RENDER;
        data->confirm_entries_num++;

        data->flags |= LIST_CHANGED;

        if (_flags(obj) & MADF_SETUP)
        {
            /* We have to calculate the width and height of the newly
             * inserted entry. This has to be done after inserting the
             * element into the list */
            if (CalcDimsOfEntry(cl, obj, pos))
                adjusted = TRUE;
        }

        toinsert++;
        pos++;
    }
    pos--;

    /* Recalculate the number of visible entries */
    if (_flags(obj) & MADF_SETUP)
    {
        /* its result unused, as everywhere: GCC then drops it (IPA-SRA)
         * and keeps the function's frame as small as before */
        LONG visible = data->entries_visible;

        CalcVertVisible(cl, obj);
        if (visible != data->entries_visible)
            adjusted = TRUE;
    }
    if (adjusted)
        List_CheckHoriz(cl, obj);

    if (data->entries_num != data->confirm_entries_num)
    {
        SetAttrs(obj,
            MUIA_List_Entries, data->confirm_entries_num,
            MUIA_List_Visible, data->entries_visible, TAG_DONE);
    }

    if (sort)
    {
        LONG i;
        if (count == 1)
        {
            /* Insert_Sorted assumes existing entries are in order. An
             * explicit Sort still rechecks keys changed by the caller. */
            i = InsertSortedEntry(obj, data, data->entries, data->insert_position);
            if (data->entries_active >= i &&
                data->entries_active < data->insert_position)
                data->entries_active++;
            data->insert_position = i;
            if (!(data->update & UPDATEMODE_ALL))
                data->update = UPDATEMODE_NEEDED;
            if (!(data->flags & LIST_QUIET))
                MUI_Redraw(obj, MADF_DRAWUPDATE);
        }
        else
        {
            /* the first inserted entry, marked, is found after the sort */
            data->entries[data->insert_position].flags |= ENTRY_INSERTED;
            DoMethod(obj, MUIM_List_Sort);
            for (i = 0; i < data->entries_num; i++)
                if (data->entries[i].flags & ENTRY_INSERTED)
                {
                    data->entries[i].flags &= ~ENTRY_INSERTED;
                    data->insert_position = i;
                    break;
                }
        }

        if ((adjusted) && (data->flags & LIST_QUIET))
            data->update = UPDATEMODE_ALL;
    }
    else if (!adjusted && !(data->flags & LIST_QUIET) && !data->update
        && (_flags(obj) & MADF_SETUP)
        && data->insert_position >= data->entries_first)
    {
        /* No row height, column width or visible count changed and the
         * rows above the insert stay: only the rows from the insert to
         * the bottom of the view move. An insert below the view changes
         * no row at all (its scroller is told by MUIA_List_Entries
         * above), as on MUI 3.8 (opbench "list insert 1": 33.6 -> 6.9
         * ms, MUI 3.8 10.3 ms). */
        LONG i, last = data->entries_first + data->entries_visible;

        if (last > data->entries_num)
            last = data->entries_num;
        if (data->insert_position <= data->entries_first)
        {
            /* every row in view moves: one background, as before */
            data->update = UPDATEMODE_ALL;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
        }
        else if (data->insert_position < last)
        {
            for (i = data->insert_position; i < last; i++)
                data->entries[i].flags |= ENTRY_RENDER;
            data->update = UPDATEMODE_NEEDED;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
        }
    }
    else
    {
        data->update = UPDATEMODE_ALL;
        if (!(data->flags & LIST_QUIET))
            MUI_Redraw(obj, MADF_DRAWUPDATE);
    }
    superset(cl, obj, MUIA_List_InsertPosition, data->insert_position);

    /* Update index of active entry */
    if (!sort && data->entries_active >= data->insert_position)
    {
        active = data->entries_active + count;
        SET(obj, MUIA_List_Active, active);
    }

    return failed ? ~0UL : (ULONG)pos;
}

/****** List.mui/MUIM_List_InsertSingle **************************************
*
*   NAME
*       MUIM_List_InsertSingle (V7)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_InsertSingle, APTR entry, LONG pos);
*
*   FUNCTION
*       Adds a single entry to the list. If a construct hook has been
*       installed, the result of passing the entry to this hook will be
*       inserted.
*
*   INPUTS
*       entry - the entry to be inserted.
*       pos - the index at which to insert the new entry. The following
*           special values can also be used:
*           MUIV_List_Insert_Top: insert at index 0.
*           MUIV_List_Insert_Bottom: insert after all existing entries.
*           MUIV_List_Insert_Active: insert at the index of the active entry
*               (or at index 0 if there is no active entry).
*           MUIV_List_Insert_Sorted: keep the list sorted.
*
*   SEE ALSO
*       MUIM_List_Insert, MUIM_List_Remove, MUIA_List_ConstructHook.
*
******************************************************************************
*
*/

IPTR List__MUIM_InsertSingle(struct IClass *cl, Object *obj,
    struct MUIP_List_InsertSingle *msg)
{
    return DoMethod(obj, MUIM_List_Insert, (IPTR) & msg->entry, 1,
        msg->pos);
}

/****** List.mui/MUIM_List_GetEntry ******************************************
*
*   NAME
*       MUIM_List_GetEntry (V4)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_GetEntry, LONG pos, APTR *entry);
*
*   FUNCTION
*       Retrieves an entry from the list. If the requested entry position is
*       invalid, the entry will be NULL.
*
*   INPUTS
*       pos - the index of the entry to get, or the special value
*           MUIV_List_GetEntry_Active to get the active entry.
*       entry - a pointer to a variable in which to store a pointer to the
*           entry data.
*
*   SEE ALSO
*       MUIM_List_Insert, MUIM_List_InsertSingle, MUIM_List_Remove.
*
******************************************************************************
*
*/

IPTR List__MUIM_GetEntry(struct IClass *cl, Object *obj,
    struct MUIP_List_GetEntry *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    int pos = msg->pos;

    if (pos == MUIV_List_GetEntry_Active)
        pos = data->entries_active;

    if (pos < 0 || pos >= data->entries_num)
    {
        *msg->entry = NULL;
        return 0;
    }
    *msg->entry = data->entries[pos].data;
    return (IPTR) *msg->entry;
}

/**************************************************************************
 MUIM_List_Construct
**************************************************************************/
IPTR List__MUIM_Construct(struct IClass *cl, Object *obj,
    struct MUIP_List_Construct *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);

    if (NULL == data->construct_hook)
        return (IPTR) msg->entry;
    if ((IPTR)data->construct_hook == MUIV_List_ConstructHook_StringArray)
    {
        CONST_STRPTR *src = msg->entry;
        ULONG bytes, *allocation;
        STRPTR *copy, dest;
        /* The program's array has a string per column: as many as
         * MUIA_List_MaxColumns if it set that, else the columns the
         * format shows. Reading 64 (the default maximum) ran past short
         * arrays into strlen() of whatever followed. */
        LONG n = StringArrayColumns(data);
        if (!src || n <= 0) return 0;
        bytes = 2 * sizeof(ULONG) + n * sizeof(STRPTR);
        for (LONG i = 0; i < n; i++)
            if (src[i])
            {
                ULONG len = strlen(src[i]) + 1;
                if (len > 0x7fffffffUL - bytes) return 0;
                bytes += len;
            }
        allocation = AllocPooled(msg->pool, bytes);
        if (!allocation) return 0;
        /* bytes, the number of strings, the strings */
        allocation[0] = bytes;
        allocation[1] = n;
        copy = (APTR)(allocation + 2);
        dest = (STRPTR)(copy + n);
        for (LONG i = 0; i < n; i++)
        {
            copy[i] = NULL;
            if (src[i])
            {
                ULONG len = strlen(src[i]) + 1;
                copy[i] = dest;
                memcpy(dest, src[i], len);
                dest += len;
            }
        }
        return (IPTR)copy;
    }
    if ((IPTR) data->construct_hook == MUIV_List_ConstructHook_String)
    {
        int len = msg->entry ? strlen((STRPTR) msg->entry) : 0;
        ULONG *mem = AllocPooled(msg->pool, len + 5);

        if (NULL == mem)
            return 0;
        mem[0] = len + 5;
        if (msg->entry != NULL)
            strcpy((STRPTR) (mem + 1), (STRPTR) msg->entry);
        else
            *(STRPTR) (mem + 1) = 0;
        return (IPTR) (mem + 1);
    }
    return CallHookPkt(data->construct_hook, msg->pool, msg->entry);
}

/**************************************************************************
 MUIM_List_Destruct
**************************************************************************/
IPTR List__MUIM_Destruct(struct IClass *cl, Object *obj,
    struct MUIP_List_Destruct *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);

    if (NULL == data->destruct_hook)
        return 0;

    if ((IPTR)data->destruct_hook == MUIV_List_DestructHook_String)
    {
        ULONG *mem = ((ULONG *) msg->entry) - 1;
        FreePooled(msg->pool, mem, mem[0]);
    }
    else if ((IPTR)data->destruct_hook == MUIV_List_DestructHook_StringArray)
    {
        ULONG *mem = ((ULONG *) msg->entry) - 2;  /* see Construct */
        FreePooled(msg->pool, mem, mem[0]);
    }
    else
    {
        CallHookPkt(data->destruct_hook, msg->pool, msg->entry);
    }
    return 0;
}

/****** List.mui/MUIM_List_Compare *******************************************
*
*   NAME
*       MUIM_List_Compare (V20)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_Compare, APTR entry1, APTR entry2,
*           LONG sort_type1, LONG sort_type2);
*
*   FUNCTION
*       Compare two list entries according to the current comparison hook
*       (MUIA_List_CompareHook).
*
*   INPUTS
*       entry1 - the first entry data.
*       entry2 - the second entry data.
*       sort_type1 - undocumented.
*       sort_type2 - undocumented.
*
*   SEE ALSO
*       MUIA_List_CompareHook, MUIM_List_Sort.
*
******************************************************************************
*
*/

IPTR List__MUIM_Compare(struct IClass *cl, Object *obj,
    struct MUIP_List_Compare *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);

    if ((IPTR)data->compare_hook == MUIV_List_CompareHook_StringArray)
    {
        LONG column = msg->column;
        if (column >= 0 && column < data->columns)
            column = data->ci[column].colno;
        if (column < 0 || column >= data->max_columns) column = 0;
        CONST_STRPTR a = StringArrayGet(data, msg->entry1, column);
        CONST_STRPTR b = StringArrayGet(data, msg->entry2, column);
        return Stricmp(a ? a : (CONST_STRPTR)"", b ? b : (CONST_STRPTR)"");
    }
    if (!data->compare_hook || (IPTR)data->compare_hook == MUIV_List_CompareHook_String)
        return Stricmp(msg->entry1, msg->entry2);
    return CallHookPkt(data->compare_hook, msg->entry2, msg->entry1);
}

/**************************************************************************
 MUIM_List_Display
**************************************************************************/
IPTR List__MUIM_Display(struct IClass *cl, Object *obj,
    struct MUIP_List_Display *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);

    if ((IPTR)data->display_hook == MUIV_List_DisplayHook_StringArray)
    {
        for (LONG i = 0; i < data->columns_reserved; i++)
            msg->array[i] = (STRPTR)StringArrayGet(data, msg->entry, i);
        return TRUE;
    }
    if (NULL == data->display_hook ||
        (IPTR)data->display_hook == MUIV_List_DisplayHook_String)
    {
        if (msg->entry)
            *msg->array = msg->entry;
        else
            *msg->array = 0;
        return 1;
    }

    *((ULONG *) (msg->array - 1)) = msg->entry_pos;
    return CallHookPkt(data->display_hook, msg->array, msg->entry);
}

/**************************************************************************
 MUIM_List_SelectChange
**************************************************************************/
IPTR List__MUIM_SelectChange(struct IClass *cl, Object *obj,
    struct MUIP_List_SelectChange *msg)
{
    return 1;
}

/****** List.mui/MUIM_List_CreateImage ***************************************
*
*   NAME
*       MUIM_List_CreateImage (V11)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_CreateImage, Object *area, ULONG flags);
*
*   FUNCTION
*       Creates an image to be inserted within list entries. An instance of
*       any Area subclass is passed in and a blackbox value is returned that
*       can be displayed by embedding its hexadecimal representation within
*       any of the list's display strings (provided either statically or by
*       the list's display hook. The format string to be used is
*       "\330[%p]".
*
*       It is recommended that lists that embed images are instances of a
*       custom subclass so that this method can be called in the list's
*       MUIM_Setup method, and MUIM_List_DeleteImage can be called in the
*       list's MUIM_Cleanup method. However, this is not necessary as long as
*       MUIM_List_CreateImage is called after the list has had its MUIM_Setup
*       called, and MUIM_List_DeleteImage is called after the list has had its
*       MUIM_Cleanup method called.
*
*   INPUTS
*       area - the area object that is used to generate the image.
*       flags - must be zero.
*
*   RESULT
*       A blackbox reference to the list image, or NULL on an error.
*
*   NOTES
*       If this method returns NULL, no special action needs to be taken, as
*       embedding this value in a list string will simply cause no image to
*       appear.
*
*       The object passed in becomes a child of the list, so it cannot be used
*       elsewhere simultaneously.
*
*   SEE ALSO
*       MUIM_List_DeleteImage, MUIA_List_DisplayHook.
*
******************************************************************************
*
* Connects an Area subclass object to the list, much like an object gets
* connected to a window. List calls Setup and AskMinMax on that object,
* keeps a reference to it (that reference will be returned).
* Text engine will dereference that pointer and draw the object with its
* default size.
*
*/

IPTR List__MUIM_CreateImage(struct IClass *cl, Object *obj,
    struct MUIP_List_CreateImage *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    struct ListImage *li;

    if (!msg->obj)
        return 0;

    li = AllocPooled(data->pool, sizeof(struct ListImage));
    if (!li)
        return 0;
    li->obj = msg->obj;

    AddTail((struct List *)&data->images, (struct Node *)li);

    /* If the list is already setup, connect the object */
    if ((_flags(obj) & MADF_SETUP)) {
        DoMethod(li->obj, MUIM_ConnectParent, (IPTR) obj);
        DoSetupMethod(li->obj, muiRenderInfo(obj));
    }
    return (IPTR) li;
}

/****** List.mui/MUIM_List_DeleteImage ***************************************
*
*   NAME
*       MUIM_List_DeleteImage (V11)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_DeleteImage, APTR image);
*
*   FUNCTION
*       Deletes an image created by MUIM_List_CreateImage.
*
*   INPUTS
*       image - a blackbox reference to the list image, or NULL.
*
*   SEE ALSO
*       MUIM_List_CreateImage, MUIA_List_DisplayHook.
*
******************************************************************************
*
*/

IPTR List__MUIM_DeleteImage(struct IClass *cl, Object *obj,
    struct MUIP_List_DeleteImage *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    struct ListImage *li = (struct ListImage *)msg->listimg;

    if (li)
    {
        ListImageCleanup(li->obj);
        Remove((struct Node *)li);
        FreePooled(data->pool, li, sizeof(struct ListImage));
    }

    return 0;
}

/****** List.mui/MUIM_List_Jump **********************************************
*
*   NAME
*       MUIM_List_Jump (V4)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_Jump, LONG pos);
*
*   FUNCTION
*       Scrolls the list so that a particular entry is visible.
*
*   INPUTS
*       pos - index of entry that should become visible, or one of these
*           special values:
*               MUIV_List_Jump_Active: show the active entry.
*               MUIV_List_Jump_Top: show the first entry.
*               MUIV_List_Jump_Bottom: show the last entry.
*               MUIV_List_Jump_Up: show the previous hidden entry.
*               MUIV_List_Jump_Down: show the next hidden entry.
*
******************************************************************************
*
*/

IPTR List__MUIM_Jump(struct IClass *cl, Object *obj,
    struct MUIP_List_Jump *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    LONG pos = msg->pos;

    switch (pos)
    {
    case MUIV_List_Jump_Top:
        pos = 0;
        break;

    case MUIV_List_Jump_Active:
        pos = data->entries_active;
        break;

    case MUIV_List_Jump_Bottom:
        pos = data->entries_num - 1;
        break;

    case MUIV_List_Jump_Down:
        pos = data->entries_first + data->entries_visible;
        break;

    case MUIV_List_Jump_Up:
        pos = data->entries_first - 1;
        break;
    }

    if (pos >= data->entries_num)
    {
        pos = data->entries_num - 1;
    }
    if (pos < 0)
        pos = 0;

    if (pos < data->entries_first)
    {
        set(obj, MUIA_List_First, pos);
    }
    else if (pos >= data->entries_first + data->entries_visible)
    {
        pos -= (data->entries_visible - 1);
        if (pos < 0)
            pos = 0;
        if (pos != data->entries_first)
        {
            set(obj, MUIA_List_First, pos);
        }
    }

    return TRUE;
}

/****** List.mui/MUIM_List_Sort **********************************************
*
*   NAME
*       MUIM_List_Sort (V4)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_Sort);
*
*   FUNCTION
*       Sort the list's entries according to the current comparison hook
*       (MUIA_List_CompareHook).
*
*   NOTES
*       Preserve the active entry while updating its index after sorting.
*
*   SEE ALSO
*       MUIA_List_CompareHook, MUIM_List_Compare.
*
******************************************************************************
*
*/

/* A fixed-stack heap handles arbitrary input without a scratch array. */

static void SiftEntries(Object *obj, struct MUI_ListData *data, struct ListEntry *entries,
    LONG root, LONG count)
{
    struct ListEntry *r = &entries[root], *c, *end = &entries[count],
        *half = &entries[count / 2];

    /* the entry sinks by swaps: nothing held across the compares but
     * where it is */
    while (r < half)
    {
        c = r + (r - entries) + 1;
        if (c + 1 < end && CompareEntries(obj, data, c, c + 1) < 0)
            c++;
        if (CompareEntries(obj, data, r, c) >= 0) break;
        ULONG *a = (ULONG *)r, *b = (ULONG *)c;
        ULONG t = a[0]; a[0] = b[0]; b[0] = t;
        t = a[1]; a[1] = b[1] | ENTRY_RENDER; b[1] = t;
        r = c;
    }
    r->flags |= ENTRY_RENDER;
}

IPTR List__MUIM_Sort(struct IClass *cl, Object *obj,
    struct MUIP_List_Sort *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->edit_object) ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);
    struct ListEntry *entries = data->entries;
    LONG count = data->entries_num, i, active = data->entries_active;

    if (count < 2) return 0;
    /* Verify order each time: callers may change comparison keys in place. */
    for (i = 1; i < count; i++)
        if (CompareEntries(obj, data, &entries[i - 1], &entries[i]) > 0) break;
    if (i == count) return 0;
    /* the active entry is marked and found again after the sort */
    if (active >= 0 && active < count)
        entries[active].flags |= ENTRY_ACTIVEMARK;
    else
        active = -1;

    if (i == count - 1)
    {
        InsertSortedEntry(obj, data, entries, i);
    }
    else
    {
        for (i = count / 2; i > 0; i--)
            SiftEntries(obj, data, entries, i - 1, count);
        for (i = count - 1; i > 0; i--)
        {
            ULONG d = ROW_L(entries, 0)[0], hf = ROW_L(entries, 0)[1];
            entries[0] = entries[i];
            ROW_L(entries, i)[0] = d;
            ROW_L(entries, i)[1] = hf | ENTRY_RENDER;
            SiftEntries(obj, data, entries, 0, i);
        }
    }
    if (active >= 0)
        for (i = 0; i < count; i++)
            if (entries[i].flags & ENTRY_ACTIVEMARK)
            {
                entries[i].flags &= ~ENTRY_ACTIVEMARK;
                data->entries_active = i;
                break;
            }
    data->flags |= LIST_CHANGED;
    if (!(data->update & UPDATEMODE_ALL))
        data->update = UPDATEMODE_NEEDED;
    if (!(data->flags & LIST_QUIET))
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    return 0;
}

/****** List.mui/MUIM_List_Move **********************************************
*
*   NAME
*       MUIM_List_Move (V9)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_Move, LONG from, LONG to);
*
*   FUNCTION
*       Move a list entry to a new position.
*
*   INPUTS
*       from - the current index of the entry that should be moved, or one of
*           these special values:
*               MUIV_List_Move_Active: the active entry.
*               MUIV_List_Move_Top: the first entry.
*               MUIV_List_Move_Bottom: the last entry.
*       to - the index of the entry's new position, or one of
*           these special values:
*               MUIV_List_Move_Active: the active entry.
*               MUIV_List_Move_Top: the first entry.
*               MUIV_List_Move_Bottom: the last entry.
*
*   NOTES
*       The active index does not change, so the active entry may do so.
*
*   SEE ALSO
*       MUIM_List_Exchange
*
******************************************************************************
*
*/

IPTR List__MUIM_Move(struct IClass *cl, Object *obj,
    struct MUIP_List_Move *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->edit_object) ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);

    LONG from, to;

    /* Normalise special 'from' values */
    switch (msg->from)
    {
    case MUIV_List_Move_Top:
        from = 0;
        break;
    case MUIV_List_Move_Active:
        from = data->entries_active;
        break;
    case MUIV_List_Move_Bottom:
        from = data->entries_num - 1;
        break;
    default:
        from = msg->from;
    }

    /* Normalise special 'to' values */
    switch (msg->to)
    {
    case MUIV_List_Move_Top:
        to = 0;
        break;
    case MUIV_List_Move_Active:
        to = data->entries_active;
        break;
    case MUIV_List_Move_Bottom:
        to = data->entries_num - 1;
        break;
    case MUIV_List_Move_Next:
        to = from + 1;
        break;
    case MUIV_List_Move_Previous:
        to = from - 1;
        break;
    default:
        to = msg->to;
    }

    /* Check that values are within valid bounds */
    if (from > data->entries_num - 1 || from < 0
        || to > data->entries_num - 1 || to < 0 || from == to)
        return (IPTR) FALSE;

    /* Shift all entries in the range between the 'from' and 'to'
     * positions. The active index stays (MUI 3), so the active entry
     * may change. */
    {
        ULONG *p = ROW_L(data->entries, from);
        ULONG d = p[0], hf = p[1];
        LONG n;

        if (from < to)
            for (n = to - from; n; n--, p += 2)
                p[0] = p[2], p[1] = p[3];
        else
            for (n = from - to; n; n--, p -= 2)
                p[0] = p[-2], p[1] = p[-1];
        p[0] = d;
        p[1] = hf;
    }

    /* Reflect list changes visually */
    data->flags |= LIST_CHANGED;
    data->update = UPDATEMODE_ALL;
    if (!(data->flags & LIST_QUIET))
        MUI_Redraw(obj, MADF_DRAWUPDATE);

    return TRUE;
}

/****** List.mui/MUIM_List_NextSelected **************************************
*
*   NAME
*       MUIM_List_NextSelected (V6)
*
*   SYNOPSIS
*       DoMethod(obj, MUIM_List_NextSelected, LONG *pos);
*
*   FUNCTION
*       Allows iteration through a list's selected entries by providing the
*       index of the next selected entry after the specified index.
*
*   INPUTS
*       pos - the address of a variable containing the index of the previous
*           selected entry. The variable must be initialised to the special
*           value MUIV_List_NextSelected_Start to find the first selected
*           entry. When this method returns, the variable will contain the
*           index of the next selected entry, or MUIV_List_NextSelected_End if
*           there are no more.
*
*   NOTES
*       If there are no selected entries but there is an active entry, the
*       index of the active entry will be stored (when
*       MUIV_List_NextSelected_Start is specified).
*
*       Some selected entries may be skipped if any entries are removed
*       between calls to this method during an iteration of a list.
*
*       MUIV_List_NextSelected_Start and MUIV_List_NextSelected_End may have
*       the same numeric value.
*
*   SEE ALSO
*       MUIM_List_Select, MUIM_List_Remove.
*
******************************************************************************
*
*/

IPTR List__MUIM_NextSelected(struct IClass *cl, Object *obj,
    struct MUIP_List_NextSelected *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    LONG pos, i;
    BOOL found = FALSE;

    /* Get the first entry to check */
    pos = *msg->pos;
    if (pos == MUIV_List_NextSelected_Start)
        pos = 0;
    else
        pos++;

    /* Find the next selected entry */
    for (i = pos; i < data->entries_num && !found; i++)
    {
        if (data->entries[i].flags & ENTRY_SELECTED)
        {
            pos = i;
            found = TRUE;
        }
    }

    /* Return index of selected or active entry, or indicate there are no
       more */
    if (!found)
    {
        if (*msg->pos == MUIV_List_NextSelected_Start
            && data->entries_active != MUIV_List_Active_Off)
            pos = data->entries_active;
        else
            pos = MUIV_List_NextSelected_End;
    }
    *msg->pos = pos;

    return TRUE;
}

/**************************************************************************
 MUIM_List_TestPos
**************************************************************************/
IPTR List__MUIM_TestPos(struct IClass *cl, Object *obj,
    struct MUIP_List_TestPos *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    Object *area = data->area;
    struct MUI_List_TestPos_Result *result = msg->res;
    LONG col = -1, row = -1;
    UWORD flags = 0, i;
    LONG mx;
    LONG ey;
    LONG entries_visible;

    if (data->entries_visible <= data->entries_num)
        entries_visible = data->entries_visible;
    else
        entries_visible = data->entries_num;

    if ((msg->x == MUI_MAXMAX) && (msg->y == MUI_MAXMAX))
    {
        mx = data->mouse_x;
        ey = data->mouse_y;
    }
    else
    {
        mx = msg->x;
        ey = msg->y;
    }

    if ((mx != MUI_MAXMAX) && (ey != MUI_MAXMAX))
    {
        mx -= _left(area);
        /* y coordinates transformed to the entries */
        ey -= data->entries_top_pixel;

        /* Now check if it was clicked on a title or on entries */
        if (ey < 0)
            flags |= MUI_LPR_ABOVE;
        else if (ey >= entries_visible * data->entry_maxheight)
            flags |= MUI_LPR_BELOW;
        else
        {
            /* Identify row */
            row = ey / data->entry_maxheight + data->entries_first;
            result->yoffset =
                ey % data->entry_maxheight - data->entry_maxheight / 2;
        }

        if (mx < 0)
            flags |= MUI_LPR_LEFT;
        else if (mx >= _width(area))
            flags |= MUI_LPR_RIGHT;
        else
        {
            /* Identify column */
            if (data->columns > 0)
            {
                LONG width_sum = 0;
                for (i = 0; i < data->columns; i++)
                {
                    if (ColumnHidden(data, i)) continue;
                    LONG offset = mx + data->horiz_first - width_sum;
                    width_sum +=
                        ColumnWidth(data, i) +
                        data->ci[i].delta +
                        (data->ci[i].bar ? BAR_WIDTH : 0);
                    D(bug("[List/MUIM_TestPos] i %d "
                        "width %d width_sum %d mx %d\n",
                        i, ColumnWidth(data, i), width_sum, mx));
                    if (mx + data->horiz_first < width_sum)
                    {
                        col = i;
                        result->xoffset = offset;
                        D(bug("[List/MUIM_TestPos] Column hit %d\n", col));
                        break;
                    }
                }
            }
        }
    }

    if (col < 0) result->xoffset = -1;
    result->entry = row;
    result->column = col;
    result->flags = flags;

    return TRUE;
}

/****i* List.mui/MUIM_DragQuery **********************************************
*
*   NAME
*       MUIM_DragQuery
*
******************************************************************************
*
*/

IPTR List__MUIM_DragQuery(struct IClass *cl, Object *obj,
    struct MUIP_DragQuery *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);

    if (msg->obj == obj && (data->flags & LIST_DRAGSORTABLE))
        return MUIV_DragQuery_Accept;
    else
        return MUIV_DragQuery_Refuse;
}


/****i* List.mui/MUIM_DragFinish *********************************************
*
*   NAME
*       MUIM_DragFinish
*
******************************************************************************
*
*/

IPTR List__MUIM_DragFinish(struct IClass *cl, Object *obj,
    struct MUIP_DragFinish *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);

    data->drop_mark_y = -1;

    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/* Listen for mouse moves, ticks and an inactive window while the button
 * is down (drag TRUE), or stop (FALSE) */
static void List_DragEvents(Object *obj, struct MUI_ListData *data, BOOL drag)
{
    const ULONG extra = IDCMP_MOUSEMOVE | IDCMP_INTUITICKS
        | IDCMP_INACTIVEWINDOW;

    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) &data->ehn);
    data->ehn.ehn_Events = drag ? data->ehn.ehn_Events | extra
        : data->ehn.ehn_Events & ~extra;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) &data->ehn);
}

static void DoWheelMove(struct IClass *cl, Object *obj, LONG wheely)
{
    LONG new, first, entries, visible;

    new = first = XGET(obj, MUIA_List_First);
    entries = XGET(obj, MUIA_List_Entries);
    visible = ((struct MUI_ListData *)INST_DATA(cl, obj))->entries_visible;

    new += wheely;

    if (new > entries - visible)
    {
        new = entries - visible;
    }

    if (new < 0)
    {
        new = 0;
    }

    if (new != first)
    {
        set(obj, MUIA_List_First, new);
    }
}

/**************************************************************************
 MUIM_HandleEvent
**************************************************************************/
IPTR List__MUIM_HandleEvent(struct IClass *cl, Object *obj,
    struct MUIP_HandleEvent *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    Object *area = data->area;
    struct IntuiMessage *imsg = msg->imsg;
    if (data->edit_object && imsg && imsg->Class == IDCMP_RAWKEY &&
        imsg->Code == 0x45)
    {
        DoMethod(_app(obj), MUIM_Application_PushMethod, obj, 2,
            MUIM_List_EndEdit, MUIV_List_EndEdit_Abort);
        return MUI_EventHandlerRC_Eat;
    }
    /* A key that is no MUI key reaches every list with a key handler
     * when the active object does not eat it (a key release in a string
     * gadget): only the mouse wheel does anything here then */
    if (msg->muikey == MUIKEY_NONE && imsg && imsg->Class == IDCMP_RAWKEY
        && imsg->Code != RAWKEY_NM_WHEEL_UP
        && imsg->Code != RAWKEY_NM_WHEEL_DOWN)
        return 0;

    struct MUI_List_TestPos_Result pos;
    LONG seltype = MUIV_List_Select_On, old_active, new_active, visible,
        first, last, i;
    IPTR result = 0;
    BOOL select = FALSE, clear = FALSE, range_select = FALSE, changing;
    WORD delta;
    typeof(msg->muikey) muikey = msg->muikey;
#define LIST_NO_JUMP 0x7fffffff
    LONG jump = LIST_NO_JUMP;   /* MUIM_List_Jump's position for a key */

    /* the list's own value (its getter's answer), no GetAttr per event */
    new_active = old_active = data->entries_active;
    visible = data->entries_visible;

    if (muikey != MUIKEY_NONE)
    {
        result = MUI_EventHandlerRC_Eat;

        /* Make keys behave differently in read-only mode */
        if (data->read_only)
        {
            switch (muikey)
            {
            case MUIKEY_TOP:
                muikey = MUIKEY_LINESTART;
                break;

            case MUIKEY_BOTTOM:
                muikey = MUIKEY_LINEEND;
                break;

            case MUIKEY_UP:
                muikey = MUIKEY_LEFT;
                break;

            case MUIKEY_DOWN:
            case MUIKEY_PRESS:
                muikey = MUIKEY_RIGHT;
                break;
            }
        }

        switch (muikey)
        {
        case MUIKEY_TOGGLE:
            if (data->multiselect != MUIV_Listview_MultiSelect_None
                && !data->read_only)
            {
                select = TRUE;
                data->click_column = data->def_click_column;
                new_active = MUIV_List_Active_Down;
            }
            else
            {
                jump = 0;
                muikey = MUIKEY_NONE;
            }
            break;

        case MUIKEY_TOP:
            new_active = MUIV_List_Active_Top;
            break;

        case MUIKEY_BOTTOM:
            new_active = MUIV_List_Active_Bottom;
            break;

        case MUIKEY_LEFT:
        case MUIKEY_WORDLEFT:
            jump = MUIV_List_Jump_Up;
            break;

        case MUIKEY_RIGHT:
        case MUIKEY_WORDRIGHT:
            jump = MUIV_List_Jump_Down;
            break;

        case MUIKEY_LINESTART:
            jump = MUIV_List_Jump_Top;
            break;

        case MUIKEY_LINEEND:
            jump = MUIV_List_Jump_Bottom;
            break;

        case MUIKEY_UP:
            new_active = MUIV_List_Active_Up;
            break;

        case MUIKEY_DOWN:
            new_active = MUIV_List_Active_Down;
            break;

        case MUIKEY_PRESS:
            data->click_column = data->def_click_column;
            superset(cl, obj, MUIA_Listview_ClickColumn,
                data->click_column);
            set(obj, MUIA_Listview_DoubleClick, TRUE);
            break;

        case MUIKEY_PAGEUP:
            if (data->read_only)
                DoWheelMove(cl, obj, -visible);
            else
                new_active = MUIV_List_Active_PageUp;
            break;

        case MUIKEY_PAGEDOWN:
            if (data->read_only)
                DoWheelMove(cl, obj, visible);
            else
                new_active = MUIV_List_Active_PageDown;
            break;

        default:
            result = 0;
        }
        if (jump != LIST_NO_JUMP)
            DoMethod(obj, MUIM_List_Jump, jump);
    }
    else if (imsg)
    {
        DoMethod(obj, MUIM_List_TestPos, imsg->MouseX, imsg->MouseY,
            (IPTR) &pos);

        switch (imsg->Class)
        {
        case IDCMP_MOUSEBUTTONS:
            if (imsg->Code == SELECTDOWN)
            {
                if (data->title && imsg->MouseY >= _mtop(obj) &&
                    imsg->MouseY < data->entries_top_pixel &&
                    imsg->MouseX >= _mleft(area) &&
                    imsg->MouseX <= _mright(area))
                {
                    if (pos.column >= 0 && data->ci[pos.column].sortable)
                    {
                        if (data->sort_column == pos.column)
                            data->ci[pos.column].descending = !data->ci[pos.column].descending;
                        set(obj, MUIA_List_SortColumn, pos.column);
                        DoMethod(obj, MUIM_List_Sort);
                    }
                    if (pos.column < 0) return 0;
                    data->title_click = pos.column;
                    superset(cl, obj, MUIA_List_TitleClick, data->title_click);
                    return MUI_EventHandlerRC_Eat;
                }
                if (_isinobject(area, imsg->MouseX,
                    imsg->MouseY)
                    && !(data->horiz_shown && _isinobject(data->horiz,
                        imsg->MouseX, imsg->MouseY)))
                {
                    data->mouse_click = MOUSE_CLICK_ENTRY;

                    if (!data->read_only && pos.entry != -1)
                    {
                        new_active = pos.entry;

                        clear = (data->multiselect
                            == MUIV_Listview_MultiSelect_Shifted
                            && (imsg->Qualifier
                            & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) == 0);
                        seltype = clear ? MUIV_List_Select_On
                            : MUIV_List_Select_Toggle;
                        select = data->multiselect
                            != MUIV_Listview_MultiSelect_None;

                        /* Handle MUIA_Listview_ClickColumn */
                        data->click_column = pos.column;
                        superset(cl, obj, MUIA_Listview_ClickColumn,
                            data->click_column);

                        /* Handle double clicking */
                        if (data->last_active == pos.entry
                            && DoubleClick(data->last_secs, data->last_mics,
                            imsg->Seconds, imsg->Micros))
                        {
                            set(obj, MUIA_Listview_DoubleClick, TRUE);
                            data->last_active = -1;
                            data->last_secs = data->last_mics = 0;
                        }
                        else
                        {
                            data->last_active = pos.entry;
                            data->last_secs = imsg->Seconds;
                            data->last_mics = imsg->Micros;

                            /* Look out for mouse movement, timer and inactive-window events while mouse button is down */
                            List_DragEvents(obj, data, TRUE);
                        }
                    }
                    result = MUI_EventHandlerRC_Eat;
                }
            }
            else
            {
                /* Do not check for _isinobject, so that button releases once mouse leaves the object are still
                   registered and the drag-selection is turned off */
                if (imsg->Code == SELECTUP && data->mouse_click)
                {
                    /* cache click position ... */
                    data->mouse_x = imsg->MouseX;
                    data->mouse_y = imsg->MouseY;

                    /* ... and activate the object */
                    set(_win(obj), MUIA_Window_ActiveObject, (IPTR)obj);
                    data->mouse_click = 0;
                }

                /* Restore normal event mask */
                List_DragEvents(obj, data, FALSE);
            }
            break;

        case IDCMP_MOUSEMOVE:
        case IDCMP_INTUITICKS:
            if (pos.flags & MUI_LPR_ABOVE)
                new_active = MUIV_List_Active_Up;
            else if (pos.flags & MUI_LPR_BELOW)
                new_active = MUIV_List_Active_Down;
            else
                new_active = pos.entry;

            select = new_active != old_active
                && data->multiselect != MUIV_Listview_MultiSelect_None;
            if (select)
            {
                seltype = data->seltype;
                range_select = new_active >= 0;
            }

            break;

        case IDCMP_INACTIVEWINDOW:
            /* Stop listening for events we only listen to when mouse button is
               down: we will not be informed of the button being released */
            List_DragEvents(obj, data, FALSE);
            break;

        case IDCMP_RAWKEY:
            /* Scroll wheel */
            if (data->vert && _isinobject(data->vert, imsg->MouseX,
                imsg->MouseY))
                delta = 1;
            else if (_isinobject(area, imsg->MouseX,
                imsg->MouseY))
                delta = 4;
            else
                delta = 0;

            if (delta != 0)
            {
                switch (imsg->Code)
                {
                case RAWKEY_NM_WHEEL_UP:
                    DoWheelMove(cl, obj, -delta);
                    result = MUI_EventHandlerRC_Eat;
                    break;

                case RAWKEY_NM_WHEEL_DOWN:
                    DoWheelMove(cl, obj, delta);
                    result = MUI_EventHandlerRC_Eat;
                    break;
                }
                /* only the wheel is eaten: other keys stay for the
                 * window's control chars and the active object */
            }
            break;
        }
    }

    /* Decide in advance if any selections may change */
    changing = clear || muikey == MUIKEY_TOGGLE || select;

    /* Change selected and active entries */
    if (clear)
    {
        DoMethod(obj, MUIM_List_Select, MUIV_List_Select_All,
            MUIV_List_Select_Off, NULL);
    }

    if (muikey == MUIKEY_TOGGLE)
    {
        DoMethod(obj, MUIM_List_Select, MUIV_List_Select_Active,
            MUIV_List_Select_Toggle, NULL);
        select = FALSE;
    }

    if (new_active != old_active)
        set(obj, MUIA_List_Active, new_active);

    if (select)
    {
        if (range_select)
        {
            if (old_active < new_active)
                first = old_active + 1, last = new_active;
            else
                first = new_active, last = old_active - 1;
            for (i = first; i <= last; i++)
                DoMethod(obj, MUIM_List_Select, i, seltype, NULL);
        }
        else
        {
            DoMethod(obj, MUIM_List_Select, MUIV_List_Select_Active,
                seltype, NULL);
            DoMethod(obj, MUIM_List_Select, MUIV_List_Select_Active,
                MUIV_List_Select_Ask, &data->seltype);
        }
    }

    if (changing)
        superset(cl, obj, MUIA_Listview_SelectChange, TRUE);

    return result;
}

/**************************************************************************
 Dispatcher
**************************************************************************/

/* The sizes CalcWidths measures (List__MUIM_AskMinMax) depend on the
 * entries, the format, the title, the hooks and the font. Every method
 * that can change one forgets them ("forget" below): entry changes, events
 * (a title click, a column dragged) and drops; a setup with another font
 * or line spacing (List_DimsValid);
 * List__OM_SET forgets them for the hooks and the columns, a format or a
 * title redraws everything (MUIM_List_Redraw). Order (Sort, Exchange, Move)
 * and drawing keep them. The check sits in the cases, not before the
 * switch: Sort and Insert send thousands of Compare, Construct and Display
 * calls through here (a call per method cost opbench's list sort 1000
 * +14%, a tag scan in every OM_SET list active +2%). */
/* The few methods that are no plain call, as functions of their own */
static IPTR List_FindAreaObject(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    IPTR found = data->edit_object ? DoMethodA(data->edit_object, msg) : 0;
    return found ? found : DoSuperMethodA(cl, obj, msg);
}
static IPTR List_SortEntriesMethod(struct IClass *cl, Object *obj,
    struct MUIP_List_SortEntries *msg)
{
    return zune_list_sort_entries(cl, obj, msg->entries);
}
static IPTR List_EditMethod(struct IClass *cl, Object *obj,
    struct MUIP_List_Edit *msg)
{
    return ListEdit(cl, obj, msg->row, msg->column);
}
static IPTR List_EndEditMethod(struct IClass *cl, Object *obj,
    struct MUIP_List_EndEdit *msg)
{
    return ListEndEdit(cl, obj, msg->mode, FALSE);
}
static IPTR List_ReturnFalse(void) { return FALSE; }
static IPTR List_ReturnTrue(void) { return TRUE; }

/* The methods List answers, by MethodID (unsigned, ascending: the search
 * below halves it). The functions are longword aligned
 * (-falign-functions=4): bit 0 set means the method may change the
 * entries' sizes ("forget" above). */
static const struct { ULONG id; APTR method; } list_methods[] =
{
    { OM_NEW, (APTR)List__OM_NEW },
    { OM_DISPOSE, (APTR)List__OM_DISPOSE },
    { OM_SET, (APTR)List__OM_SET },
    { OM_GET, (APTR)List__OM_GET },
    { MUIM_DragQuery, (APTR)List__MUIM_DragQuery },
    { MUIM_List_EndEdit, (APTR)List_EndEditMethod },
    { MUIM_List_DeleteImage, (APTR)List__MUIM_DeleteImage + 1 },
    { MUIM_List_CreateEditObject, (APTR)List_ReturnFalse },
    { MUIM_List_Compare, (APTR)List__MUIM_Compare },
    { MUIM_List_Sort, (APTR)List__MUIM_Sort },
    { MUIM_AskMinMax, (APTR)List__MUIM_AskMinMax },
    { MUIM_List_EditDone, (APTR)List_ReturnTrue },
    { MUIM_List_Exchange, (APTR)List__MUIM_Exchange },
    { MUIM_DragFinish, (APTR)List__MUIM_DragFinish + 1 },
    { MUIM_List_Select, (APTR)List__MUIM_Select },
    { MUIM_List_Display, (APTR)List__MUIM_Display },
    { MUIM_List_Move, (APTR)List__MUIM_Move },
    { MUIM_List_InsertSingle, (APTR)List__MUIM_InsertSingle + 1 },
    { MUIM_List_NextSelected, (APTR)List__MUIM_NextSelected },
    { MUIM_List_TestPos, (APTR)List__MUIM_TestPos },
    { MUIM_List_Remove, (APTR)List__MUIM_Remove + 1 },
    { MUIM_List_Insert, (APTR)List__MUIM_Insert + 1 },
    { MUIM_HandleEvent, (APTR)List__MUIM_HandleEvent + 1 },
    { MUIM_Draw, (APTR)List__MUIM_Draw },
    { MUIM_List_Redraw, (APTR)List__MUIM_Redraw + 1 },
    { MUIM_List_Destruct, (APTR)List__MUIM_Destruct },
    { MUIM_List_GetEntry, (APTR)List__MUIM_GetEntry },
    { MUIM_Setup, (APTR)List__MUIM_Setup },
    { MUIM_List_Edit, (APTR)List_EditMethod },
    { MUIM_List_CreateImage, (APTR)List__MUIM_CreateImage + 1 },
    { MUIM_List_SortEntries, (APTR)List_SortEntriesMethod },
    { MUIM_List_Clear, (APTR)List__MUIM_Clear + 1 },
    { MUIM_List_Jump, (APTR)List__MUIM_Jump },
    { MUIM_DragDrop, (APTR)zune_list_drag_drop + 1 },
    { MUIM_Show, (APTR)List__MUIM_Show },
    { MUIM_List_Construct, (APTR)List__MUIM_Construct },
    { MUIM_Cleanup, (APTR)List__MUIM_Cleanup },
    { MUIM_CreateDragImage, (APTR)zune_list_create_drag_image },
    { MUIM_DragReport, (APTR)zune_list_drag_report },
    { MUIM_Hide, (APTR)List__MUIM_Hide },
    { MUIM_Zune_Layout, (APTR)List__MUIM_Layout },
    { MUIM_FindAreaObject, (APTR)List_FindAreaObject },
    { MUIM_List_SelectChange, (APTR)List__MUIM_SelectChange },
};
/* the table's order, checked by the compiler */
_Static_assert((ULONG)OM_NEW < (ULONG)OM_DISPOSE, "list_methods order");
_Static_assert((ULONG)OM_DISPOSE < (ULONG)OM_SET, "list_methods order");
_Static_assert((ULONG)OM_SET < (ULONG)OM_GET, "list_methods order");
_Static_assert((ULONG)OM_GET < (ULONG)MUIM_DragQuery, "list_methods order");
_Static_assert((ULONG)MUIM_DragQuery < (ULONG)MUIM_List_EndEdit, "list_methods order");
_Static_assert((ULONG)MUIM_List_EndEdit < (ULONG)MUIM_List_DeleteImage, "list_methods order");
_Static_assert((ULONG)MUIM_List_DeleteImage < (ULONG)MUIM_List_CreateEditObject, "list_methods order");
_Static_assert((ULONG)MUIM_List_CreateEditObject < (ULONG)MUIM_List_Compare, "list_methods order");
_Static_assert((ULONG)MUIM_List_Compare < (ULONG)MUIM_List_Sort, "list_methods order");
_Static_assert((ULONG)MUIM_List_Sort < (ULONG)MUIM_AskMinMax, "list_methods order");
_Static_assert((ULONG)MUIM_AskMinMax < (ULONG)MUIM_List_EditDone, "list_methods order");
_Static_assert((ULONG)MUIM_List_EditDone < (ULONG)MUIM_List_Exchange, "list_methods order");
_Static_assert((ULONG)MUIM_List_Exchange < (ULONG)MUIM_DragFinish, "list_methods order");
_Static_assert((ULONG)MUIM_DragFinish < (ULONG)MUIM_List_Select, "list_methods order");
_Static_assert((ULONG)MUIM_List_Select < (ULONG)MUIM_List_Display, "list_methods order");
_Static_assert((ULONG)MUIM_List_Display < (ULONG)MUIM_List_Move, "list_methods order");
_Static_assert((ULONG)MUIM_List_Move < (ULONG)MUIM_List_InsertSingle, "list_methods order");
_Static_assert((ULONG)MUIM_List_InsertSingle < (ULONG)MUIM_List_NextSelected, "list_methods order");
_Static_assert((ULONG)MUIM_List_NextSelected < (ULONG)MUIM_List_TestPos, "list_methods order");
_Static_assert((ULONG)MUIM_List_TestPos < (ULONG)MUIM_List_Remove, "list_methods order");
_Static_assert((ULONG)MUIM_List_Remove < (ULONG)MUIM_List_Insert, "list_methods order");
_Static_assert((ULONG)MUIM_List_Insert < (ULONG)MUIM_HandleEvent, "list_methods order");
_Static_assert((ULONG)MUIM_HandleEvent < (ULONG)MUIM_Draw, "list_methods order");
_Static_assert((ULONG)MUIM_Draw < (ULONG)MUIM_List_Redraw, "list_methods order");
_Static_assert((ULONG)MUIM_List_Redraw < (ULONG)MUIM_List_Destruct, "list_methods order");
_Static_assert((ULONG)MUIM_List_Destruct < (ULONG)MUIM_List_GetEntry, "list_methods order");
_Static_assert((ULONG)MUIM_List_GetEntry < (ULONG)MUIM_Setup, "list_methods order");
_Static_assert((ULONG)MUIM_Setup < (ULONG)MUIM_List_Edit, "list_methods order");
_Static_assert((ULONG)MUIM_List_Edit < (ULONG)MUIM_List_CreateImage, "list_methods order");
_Static_assert((ULONG)MUIM_List_CreateImage < (ULONG)MUIM_List_SortEntries, "list_methods order");
_Static_assert((ULONG)MUIM_List_SortEntries < (ULONG)MUIM_List_Clear, "list_methods order");
_Static_assert((ULONG)MUIM_List_Clear < (ULONG)MUIM_List_Jump, "list_methods order");
_Static_assert((ULONG)MUIM_List_Jump < (ULONG)MUIM_DragDrop, "list_methods order");
_Static_assert((ULONG)MUIM_DragDrop < (ULONG)MUIM_Show, "list_methods order");
_Static_assert((ULONG)MUIM_Show < (ULONG)MUIM_List_Construct, "list_methods order");
_Static_assert((ULONG)MUIM_List_Construct < (ULONG)MUIM_Cleanup, "list_methods order");
_Static_assert((ULONG)MUIM_Cleanup < (ULONG)MUIM_CreateDragImage, "list_methods order");
_Static_assert((ULONG)MUIM_CreateDragImage < (ULONG)MUIM_DragReport, "list_methods order");
_Static_assert((ULONG)MUIM_DragReport < (ULONG)MUIM_Hide, "list_methods order");
_Static_assert((ULONG)MUIM_Hide < (ULONG)MUIM_Zune_Layout, "list_methods order");
_Static_assert((ULONG)MUIM_Zune_Layout < (ULONG)MUIM_FindAreaObject, "list_methods order");
_Static_assert((ULONG)MUIM_FindAreaObject < (ULONG)MUIM_List_SelectChange, "list_methods order");

BOOPSI_DISPATCHER(IPTR, List_Dispatcher, cl, obj, msg)
{
    ULONG id = msg->MethodID, m;
    const ULONG *lo = &list_methods[0].id,
        *hi = &list_methods[sizeof(list_methods) / sizeof(list_methods[0]) - 1].id,
        *p;

    /* the methods Sort and Insert send by the thousand first */
    if (id == MUIM_List_Compare)
        return List__MUIM_Compare(cl, obj, (APTR)msg);
    if (id == MUIM_List_Display)
        return List__MUIM_Display(cl, obj, (APTR)msg);
    if (id == MUIM_List_Construct)
        return List__MUIM_Construct(cl, obj, (APTR)msg);
    while (lo <= hi)
    {
        p = lo + (((hi - lo) >> 2) << 1);
        if ((m = *p) == id)
        {
            m = p[1];
            if (m & 1)
                ((struct MUI_ListData *)INST_DATA(cl, obj))->flags
                    &= ~LIST_DIMSVALID;
            return ((IPTR (*)(struct IClass *, Object *, Msg))(m & ~1UL))
                (cl, obj, msg);
        }
        if (m < id)
            lo = p + 2;
        else
            hi = p - 2;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}
BOOPSI_DISPATCHER_END

/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUI_List_desc =
{
    MUIC_List,
    MUIC_Group,
    sizeof(struct MUI_ListData),
    (void *) List_Dispatcher
};
