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
    LONG available, used = 0, last = -1, extra;
    if (!data->format_layout || !data->area) return;
    available = MAX(0, _mwidth(data->area));
    for (LONG i = 0; i < data->columns; i++)
    {
        struct ColumnInfo *ci = &data->ci[i];
        ci->layout_hidden = FALSE;
        ci->layout_width = 0;
        if (ci->hidden) continue;
        LONG minimum = ci->min_width < 0 ? ci->entries_width :
            ci->min_pixels ? ci->min_width : available * ci->min_width / 100;
        LONG maximum = ci->max_width < 0 ? ci->entries_width :
            ci->max_pixels ? ci->max_width : available * ci->max_width / 100;
        if (ci->user_width < 0) { minimum += 6; maximum += 6; }
        ci->layout_max = MAX(minimum, maximum);
        LONG gap = last < 0 ? 0 : data->ci[last].delta +
            (data->ci[last].bar ? BAR_WIDTH : 0);
        if (last >= 0 && used + gap + minimum > available)
        { ci->layout_hidden = TRUE; continue; }
        ci->layout_width = minimum;
        used += gap + minimum;
        last = i;
    }
    extra = available - used;
    /* Redistribute space left by columns which hit their maximum. */
    while (extra > 0)
    {
        LONG weights = 0, remaining = extra;
        for (LONG i = 0; i < data->columns; i++)
            if (!ColumnHidden(data, i) && data->ci[i].layout_width < data->ci[i].layout_max)
                weights += data->ci[i].weight;
        if (!weights) break;
        for (LONG i = 0; i < data->columns; i++)
        {
            struct ColumnInfo *ci = &data->ci[i];
            if (ColumnHidden(data, i) || ci->layout_width >= ci->layout_max || !ci->weight) continue;
            LONG share = MAX(1, remaining * ci->weight / weights);
            share = MIN(extra, MIN(share, ci->layout_max - ci->layout_width));
            ci->layout_width += share; extra -= share;
        }
        if (extra == remaining) break;
    }
}

static LONG ParseColumnWidth(CONST_STRPTR value, BOOL *pixels)
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

static BOOL IncreaseColumns(struct MUI_ListData *data, int new_columns);

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
 Allocate a single list entry, does not initialize it (except the pointer)
**************************************************************************/
static struct ListEntry *AllocListEntry(struct MUI_ListData *data)
{
    struct ListEntry *le;
    /* use IncreaseColumns() to enlarge column entry array. Sized for
     * columns_allocated, not the current format: IncreaseColumns() copies
     * that many from every entry, also after a format with fewer columns
     * made this one. */
    IPTR size = sizeof(struct ListEntry)
        + sizeof(LONG) * (data->columns_allocated + 1);

    le = (struct ListEntry *) AllocVecPooled(data->pool, size);
    D(bug("List AllocListEntry %p, %ld bytes\n", le, size));
    if (le)
    {
        /* possible, that we have an external pool, which does not have
           MEMF_CLEAR set.. */
        memset(le, 0, size);
    }
    return le;
}

/**************************************************************************
 The title entry lives outside the pool: a list that never gets an entry
 then does not hold a whole pool puddle for it. Cleared.
**************************************************************************/
static struct ListEntry *AllocTitleEntry(int columns)
{
    return (struct ListEntry *) AllocVec(sizeof(struct ListEntry)
        + sizeof(LONG) * (columns + 1), MEMF_ANY | MEMF_CLEAR);
}

/**************************************************************************
 Deallocate a single list entry, does not deinitialize it
**************************************************************************/
static void FreeListEntry(struct MUI_ListData *data,
    struct ListEntry *entry)
{
    D(bug("FreeListEntry %p\n", entry));
    FreeVecPooled(data->pool, entry);
}

/**************************************************************************
 Ensures that there can be at least the given amount of entries within
 the list. Returns 0 if not. It also allocates the space for the title.
 It can be accessed with data->entries[ENTRY_TITLE]
**************************************************************************/
static int SetListSize(struct MUI_ListData *data, LONG size)
{
    struct ListEntry **new_entries;
    int new_entries_allocated;

    if (size + 1 <= data->entries_allocated)
        return 1;

    new_entries_allocated = data->entries_allocated * 2 + 4;
    if (new_entries_allocated < size + 1)
        new_entries_allocated = size + 1 + 10;  /* 10 is just random */

    D(bug("List %p : SetListSize allocating %ld bytes\n", data,
            new_entries_allocated * sizeof(struct ListEntry *)));
    new_entries =
        AllocVec(new_entries_allocated * sizeof(struct ListEntry *), 0);
    if (NULL == new_entries)
        return 0;
    if (data->entries)
    {
        CopyMem(data->entries - 1, new_entries,
            (data->entries_num + 1) * sizeof(struct ListEntry *));
        FreeVec(data->entries - 1);
    }
    data->entries = new_entries + 1;
    data->entries_allocated = new_entries_allocated;
    return 1;
}

/**************************************************************************
 Prepares the insertion of count entries at pos.
 This function doesn't care if there is enough space in the datastructure.
 SetListSize() must be used first.
 With current implementation, this call will never fail
**************************************************************************/
static int PrepareInsertListEntries(struct MUI_ListData *data, int pos,
    int count)
{
    memmove(&data->entries[pos + count], &data->entries[pos],
        (data->entries_num - pos) * sizeof(struct ListEntry *));
    return 1;
}

/**************************************************************************
 Removes count (already deinitalized) list entries starting az pos.
**************************************************************************/
static void RemoveListEntries(struct MUI_ListData *data, int pos, int count)
{
    // FIXME: segfault if entries_num = pos = count = 1
    memmove(&data->entries[pos], &data->entries[pos + count],
        (data->entries_num - (pos + count)) * sizeof(struct ListEntry *));
}

/**************************************************************************
 Frees all memory allocated by ParseListFormat()
**************************************************************************/
static void FreeListFormat(struct MUI_ListData *data)
{
    int i;

    if (data->ci)
    {
        for (i = 0; i < data->columns; i++)
        {
            FreeVec(data->ci[i].preparse);
            data->ci[i].preparse = NULL;
        }
        FreeVec(data->ci);
        data->ci = NULL;
    }
    FreeVec(data->mapped_strings);
    data->mapped_strings = NULL;
    FreeVec(data->preparses);
    data->preparses = NULL;
    if (data->strings_mem)
    {
        FreeVec(data->strings_mem);
        data->strings_mem = NULL;
        data->strings = NULL;
    }
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

/* A parsed format, kept while a new one is parsed (ParseListFormat) */
struct ListFormatParts
{
    LONG columns, columns_reserved, columns_used;
    struct ColumnInfo *ci;
    STRPTR *preparses, *strings_mem, *strings, *mapped_strings;
    BOOL format_layout;
};

static void TakeListFormat(struct MUI_ListData *data,
    struct ListFormatParts *f)
{
    f->columns = data->columns;
    f->columns_reserved = data->columns_reserved;
    f->columns_used = data->columns_used;
    f->ci = data->ci;
    f->preparses = data->preparses;
    f->strings_mem = data->strings_mem;
    f->strings = data->strings;
    f->mapped_strings = data->mapped_strings;
    f->format_layout = data->format_layout;
    data->columns = 0;
    data->ci = NULL;
    data->preparses = data->strings_mem = data->strings = NULL;
    data->mapped_strings = NULL;
}

static void PutListFormat(struct MUI_ListData *data,
    struct ListFormatParts *f)
{
    data->columns = f->columns;
    data->columns_reserved = f->columns_reserved;
    data->columns_used = f->columns_used;
    data->ci = f->ci;
    data->preparses = f->preparses;
    data->strings_mem = f->strings_mem;
    data->strings = f->strings;
    data->mapped_strings = f->mapped_strings;
    data->format_layout = f->format_layout;
}

static BOOL ParseListFormatNew(struct MUI_ListData *data, STRPTR format,
    BOOL initial);

/**************************************************************************
 Parses the given format string and replaces the previous format with it.
 Use initial=FALSE for format changes outside OM_NEW.
 Return FALSE on failure; the previous format is then kept: a list left
 without one wrote its display strings through a NULL array at the next
 redraw.
**************************************************************************/
static BOOL ParseListFormat(struct MUI_ListData *data, STRPTR format,
    BOOL initial)
{
    struct ListFormatParts old, fresh;

    TakeListFormat(data, &old);
    if (!ParseListFormatNew(data, format, initial))
    {
        FreeListFormat(data);   /* what was made of the new one */
        PutListFormat(data, &old);
        return FALSE;
    }
    TakeListFormat(data, &fresh);
    PutListFormat(data, &old);
    FreeListFormat(data);
    PutListFormat(data, &fresh);
    return TRUE;
}

static BOOL ParseListFormatNew(struct MUI_ListData *data, STRPTR format,
    BOOL initial)
{
    int new_columns, i;
    STRPTR ptr;
    STRPTR format_sep;
    char c;

    IPTR args[ARG_CNT];
    struct RDArgs *rdargs;

    if (!format)
        format = (STRPTR) "";

    ptr = format;

    data->format_layout = FALSE;

    new_columns = 1;

    /* Count the number of columns first */
    while ((c = *ptr++))
        if (c == ',')
            new_columns++;


    if (new_columns > data->max_columns) return FALSE;

    if (!(data->ci = AllocVec(new_columns * sizeof(struct ColumnInfo), MEMF_CLEAR)))
        return FALSE;
    /* Counted at once so that a failure below frees the preparse strings */
    data->columns = new_columns;

    // set defaults
    for (i = 0; i < new_columns; i++)
    {
        data->ci[i].colno = data->ci[i].order_id = i;
        data->ci[i].weight = 100;
        data->ci[i].delta = 4;
        data->ci[i].min_width = -1;
        data->ci[i].max_width = -1;
        data->ci[i].user_width = -1;
        data->ci[i].bar = FALSE;
        data->ci[i].preparse = NULL;
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
                    if (args[ARG_COL])
                        data->ci[i].colno = *(LONG *) args[ARG_COL];
                    if (args[ARG_WEIGHT])
                        data->ci[i].weight = MAX(0, MIN(10000, *(LONG *)args[ARG_WEIGHT]));
                    if (args[ARG_DELTA])
                        data->ci[i].delta = MAX(0, MIN(32767, *(LONG *)args[ARG_DELTA]));
                    if (args[ARG_MINWIDTH])
                        data->ci[i].min_width =
                            ParseColumnWidth((STRPTR)args[ARG_MINWIDTH], &data->ci[i].min_pixels);
                    if (args[ARG_MAXWIDTH])
                        data->ci[i].max_width =
                            ParseColumnWidth((STRPTR)args[ARG_MAXWIDTH], &data->ci[i].max_pixels);
                    if (args[ARG_FIXWIDTH])
                    {
                        data->ci[i].min_width = data->ci[i].max_width =
                            MAX(0, MIN(32767, *(LONG *)args[ARG_FIXWIDTH]));
                        data->ci[i].min_pixels = data->ci[i].max_pixels = TRUE;
                        data->ci[i].user_width = data->ci[i].min_width;
                        data->ci[i].delta = 0;
                    }
                    data->format_layout |= args[ARG_MINWIDTH] || args[ARG_MAXWIDTH] || args[ARG_FIXWIDTH];
                    data->ci[i].is_object = !!args[ARG_ISOBJECT];
                    if (args[ARG_ELIDE])
                    {
                        STRPTR elide = (STRPTR)args[ARG_ELIDE];
                        data->ci[i].elide = !Stricmp(elide, "left") ? MUIV_Text_Shorten_ElideLeft :
                            !Stricmp(elide, "center") ? MUIV_Text_Shorten_ElideCenter :
                            !Stricmp(elide, "right") ? MUIV_Text_Shorten_ElideRight : MUIV_Text_Shorten_Cutoff;
                    }
                    data->ci[i].bar = args[ARG_BAR];
                    data->ci[i].hidden = !!args[ARG_HIDDEN];
                    data->ci[i].sortable = !!args[ARG_SORTABLE];
                    data->ci[i].descending = args[ARG_ORDER] &&
                        (!Stricmp((STRPTR)args[ARG_ORDER], "DESC") ||
                         !Stricmp((STRPTR)args[ARG_ORDER], "DESCENDING"));
                    if (args[ARG_PREPARSE])
                        data->ci[i].preparse =
                            StrDup((STRPTR) args[ARG_PREPARSE]);

                    FreeArgs(rdargs);
                }
                ptr += strlen(ptr) + 1;
                i++;
            }
            while (i < new_columns);
            FreeDosObject(DOS_RDARGS, rdargs);
        }
        FreeVec(format_sep);
    }

    for (i = 0; i < new_columns; i++)
    {
        D(bug("colno %d weight %d delta %d preparse %s\n",
                data->ci[i].colno, data->ci[i].weight, data->ci[i].delta,
                data->ci[i].preparse));
    }

    if (initial)
    {
        /* called from OM_NEW */
        data->columns_allocated = new_columns;
    }
    else if (data->columns_allocated < new_columns)
    {
        /* called by MUIA_List_Format */
        if (!IncreaseColumns(data, new_columns))
        {
            bug("[Zune:List] not enough memory for new columns!!\n");
            /* FIXME: failure can leave list entries with mixed allocation sizes. */
            return FALSE;
        }
        data->columns_allocated = new_columns;
    }
    /* The display arrays hold every column the format names (COL= may
     * point past the column count), all of MUIA_List_MaxColumns when a
     * program set it, and at least LIST_MIN_RESERVED: display hooks may
     * fill more columns than the format has (Zune's Dirlist always fills
     * six), so a format of one column must not get a one-slot array. */
    data->columns_reserved = MAX(new_columns, data->max_columns_explicit
        ? data->max_columns : MIN(LIST_MIN_RESERVED, data->max_columns));
    for (i = 0; i < new_columns; i++)
    {
        if (data->ci[i].colno < 0 ||
            data->ci[i].colno >= MAX(new_columns, data->max_columns))
            data->ci[i].colno = i;
        if (data->ci[i].colno >= data->columns_reserved)
            data->columns_reserved = data->ci[i].colno + 1;
    }
    data->columns_used = new_columns;
    for (i = 0; i < new_columns; i++)
        if (data->ci[i].colno >= data->columns_used)
            data->columns_used = data->ci[i].colno + 1;

    if (!(data->preparses =
            AllocVec((data->columns_reserved + 10) * sizeof(STRPTR), MEMF_CLEAR)))
        return FALSE;

    if (!(data->strings_mem = AllocVec((data->columns_reserved + 1 + 10)
        * sizeof(STRPTR), MEMF_CLEAR)))
                                  /* hold enough space also for the entry pos,
                                   * used by orginal MUI and also some
                                   * security space */
        return FALSE;
    data->strings=data->strings_mem;

    for (i = 0; i < new_columns; i++)
    {
        if (data->ci[i].colno != i && !data->mapped_strings)
        {
            data->mapped_strings = AllocVec(2 * new_columns * sizeof(STRPTR), MEMF_ANY);
            if (!data->mapped_strings) return FALSE;
        }
    }
    data->columns = new_columns;
    /* Skip entry pos (-1) and enough indexes to get (-9). Based on MPlayer source codes. */
    data->strings += 10;

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
        entry_data = data->entries[entry_pos]->data;
    }

    /* Get the display formation */
    DoMethod(obj, MUIM_List_Display, (IPTR) entry_data,
        (IPTR) data->strings, entry_pos, (IPTR) data->preparses);
    MapColumns(data);
}


/**************************************************************************
 Determine the dims of a single entry and adapt the columninfo according
 to it. pos might be ENTRY_TITLE. Returns 0 if pos entry needs to
 be redrawn after this operation, 1 if all entries need to be redrawn.
**************************************************************************/
static int CalcDimsOfEntry(struct IClass *cl, Object *obj, int pos)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    struct ListEntry *entry = data->entries[pos];
    int j;
    int ret = 0;

    if (!entry)
        return ret;

    if (!(_flags(obj) & MADF_SETUP))
        return ret;

    DisplayEntry(cl, obj, pos);

    /* Set height to at least minheight */
    if (data->entries[pos]->height < data->entry_minheight)
        data->entries[pos]->height = data->entry_minheight;

    for (j = 0; j < data->columns; j++)
    {
        if (data->ci[j].is_object && pos != ENTRY_TITLE)
        {
            ret |= zune_list_cell_dims(obj, data, entry, j);
            continue;
        }
        /* Plain strings without an allocation, as List_DrawEntry does:
         * this runs for every column of every inserted entry */
        ZText *text, local_text;
        ZTextLine local_line;
        ZTextChunk local_chunk;
        BOOL borrowed = zune_text_init_plain(&local_text, &local_line,
            &local_chunk, data->preparses[j], data->strings[j]);

        text = borrowed ? &local_text : zune_text_new(data->preparses[j],
            data->strings[j], ZTEXT_ARG_NONE, 0);
        if (text != NULL)
        {
            zune_text_get_bounds(text, obj);

            /* Row pitch includes the Listview font leading preference */
            if (text->height + data->prefs_linespacing
                > data->entries[pos]->height)
            {
                data->entries[pos]->height =
                    text->height + data->prefs_linespacing;
                /* entry height changed, redraw all entries later */
                ret = 1;
            }
            LONG width = text->width;

            /* a SORTABLE column's title has room for the sort mark
             * (zune_list_draw_sort_mark: a triangle two half-lines wide
             * and a gap on each side), as on MUI 5 */
            if (pos == ENTRY_TITLE && data->ci[j].sortable)
                width += (_font(obj)->tf_YSize + 1) / 2 * 2 + 5;
            data->entries[pos]->widths[j] = width;

            if (width > data->ci[j].entries_width)
            {
                /* This entry has a greater width for this column than any
                 * other entry, so we store this value
                 */
                data->ci[j].entries_width = width;
                /* column width changed, redraw all entries later */
                ret = 1;
            }

            if (!borrowed)
                zune_text_destroy(text);
        }
    }
    if (data->entries[pos]->height > data->entry_maxheight)
    {
        data->entry_maxheight = data->entries[pos]->height;
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
        data->entries_totalheight += data->entries[i]->height;
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

/**************************************************************************
 Resize arrays, so that there is enough space for new columns.
 The widths[] array must grow with increasing columns.
 columns_allocated must be checked, before calling this function.
 Space can only grow, not shrink.
 Return FALSE on error (no memory).
**************************************************************************/
static BOOL IncreaseColumns(struct MUI_ListData *data, int new_columns)
{
    int i = 0;
    IPTR newsize, oldsize;
    struct ListEntry *le;

    D(bug("IncreaseColumns: %d => %d columns\n", data->columns_allocated, new_columns));

    newsize = sizeof(struct ListEntry) + sizeof(LONG) * (new_columns             + 1);
    oldsize = sizeof(struct ListEntry) + sizeof(LONG) * (data->columns_allocated + 1);

    if (data->title)
    {
        i = -1;
    }

    for (; i < data->entries_num; i++)
    {
        D(bug("IncreaseColumns: i: %d, size: %d => %d\n", i, oldsize, newsize));
        if (i == ENTRY_TITLE)
            le = AllocTitleEntry(new_columns);
        else
            le = (struct ListEntry *) AllocVecPooled(data->pool, newsize);
        if (!le)
        {
            return FALSE;
        }
        if (i != ENTRY_TITLE)
            memset(le, 0, newsize);
        D(bug("IncreaseColumns: CopyMem(%p, %p, %d)\n", data->entries[i],
            le, oldsize));
        CopyMem(data->entries[i], le, oldsize);
        if (i == ENTRY_TITLE)
            FreeVec(data->entries[i]);
        else
            FreeVecPooled(data->pool, data->entries[i]);
        data->entries[i]=le;
    }

    return TRUE;
}

/**************************************************************************
 Default hook to compare two list entries. Works for strings only.
**************************************************************************/
AROS_UFH3S(int, default_compare_func,
    AROS_UFHA(struct Hook *, h, A0),
    AROS_UFHA(char *, s2, A2),
    AROS_UFHA(char *, s1, A1))
{
    AROS_USERFUNC_INIT

    return Stricmp(s1, s2);

    AROS_USERFUNC_EXIT
}

#define PROP_VERT_FIRST   1
#define PROP_HORIZ_FIRST  2
#define PROP_VERT_SYNC    3

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

static ULONG List_Function(struct Hook *hook, Object * obj, void **msg)
{
    struct MUI_ListData *data = (struct MUI_ListData *)hook->h_Data;
    SIPTR type = (SIPTR) msg[0];
    SIPTR val = (SIPTR) msg[1];

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

static ULONG List_LayoutFunction(struct Hook *hook, Object *obj,
    struct MUI_LayoutMsg *lm);
/* The horizontal scroller is the module's: a list has one only in a
 * program that gets MUI 4/5 or asks for it (MUIM_Setup) */
static inline void List_CheckHoriz(struct IClass *cl, Object *obj)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->horiz)
        zune_list_check_horiz(obj, data);
}

/* At entry to this function, data->area is always set, but data->vert may
 * or may not be set */
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

    /* Add list and/or scroller */
    switch (data->scroller_pos)
    {
    case MUIV_Listview_ScrollerPos_None:
        vert_not_used = TRUE;
        DoMethod(obj, OM_ADDMEMBER, data->area);
        break;
    case MUIV_Listview_ScrollerPos_Left:
        if (!data->vert)
            data->vert =ScrollbarObject, MUIA_Group_Horiz, FALSE, End;
        DoMethod(obj, OM_ADDMEMBER, data->vert);
        DoMethod(obj, OM_ADDMEMBER, data->area);
        break;
    default:
        if (!data->vert)
            data->vert = ScrollbarObject, MUIA_Group_Horiz, FALSE, End;
        DoMethod(obj, OM_ADDMEMBER, data->area);
        DoMethod(obj, OM_ADDMEMBER, data->vert);
        break;
    }

    data->area_connected = TRUE;

    /* Handle case where it was decided that vert will not be used */
    if (vert_not_used)
    {
        if (data->vert)
        {
            if (data->vert_connected)
            {
                DoMethod(obj, MUIM_KillNotifyObj, MUIA_List_VertProp_First,
                    (IPTR) obj);
                DoMethod(obj, MUIM_KillNotifyObj, MUIA_List_VertProp_Visible,
                    (IPTR) obj);
                DoMethod(obj, MUIM_KillNotifyObj, MUIA_List_VertProp_Entries,
                    (IPTR) obj);
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
            (IPTR) obj, 4, MUIM_CallHook, (IPTR) &data->hook, PROP_VERT_FIRST,
            MUIV_TriggerValue);

        /* Pass prop object as DestObj (based on code in NList); the hook
         * gives the scrollbar all three in its units */
        DoMethod(obj, MUIM_Notify, MUIA_List_VertProp_First, MUIV_EveryTime,
            (IPTR) obj, 3, MUIM_CallHook, (IPTR) &data->hook,
            PROP_VERT_SYNC);
        DoMethod(obj, MUIM_Notify, MUIA_List_VertProp_Visible, MUIV_EveryTime,
            (IPTR) obj, 3, MUIM_CallHook, (IPTR) &data->hook,
            PROP_VERT_SYNC);
        DoMethod(obj, MUIM_Notify, MUIA_List_VertProp_Entries, MUIV_EveryTime,
            (IPTR) obj, 3, MUIM_CallHook, (IPTR) &data->hook,
            PROP_VERT_SYNC);

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
    data->default_compare_hook.h_Entry = (HOOKFUNC) default_compare_func;
    data->default_compare_hook.h_SubEntry = 0;
    data->compare_hook = &(data->default_compare_hook);
    data->flags = LIST_SHOWDROPMARKS;
    data->area_replaced = FALSE;
    data->area_connected = FALSE;
    data->vert_connected = FALSE;

    data->entries_visible = data->vertprop_visible = -1;
    data->last_active = -1;
    data->title_click = 0;
    data->drop_mark = 0;

    data->ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    data->ehn.ehn_Priority = 0;
    data->ehn.ehn_Flags = MUI_EHF_GUIMODE;
    data->ehn.ehn_Object = obj;
    data->ehn.ehn_Class = cl;

    data->mouse_click = 0;
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

    data->hook.h_Entry = HookEntry;
    data->hook.h_SubEntry = (HOOKFUNC) List_Function;
    data->hook.h_Data = data;

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

        case MUIA_List_HScrollerVisibility:
            data->hscroll_visibility = tag->ti_Data;
            break;
        case MUIA_List_SortColumn:
            data->sort_column = tag->ti_Data;
            break;
        case MUIA_List_Stripes:
            data->stripes = !!tag->ti_Data;
            break;
        case MUIA_List_Editable:
            data->editable = !!tag->ti_Data;
            break;
        case MUIA_List_CompareHook:
            data->compare_hook = (struct Hook *)tag->ti_Data;
            if (data->compare_hook == NULL)
                data->compare_hook = &data->default_compare_hook;
            break;

        case MUIA_List_ConstructHook:
            data->construct_hook = (struct Hook *)tag->ti_Data;
            break;

        case MUIA_List_DestructHook:
            data->destruct_hook = (struct Hook *)tag->ti_Data;
            break;

        case MUIA_List_DisplayHook:
            data->display_hook = (struct Hook *)tag->ti_Data;
            break;

        case MUIA_List_MultiTestHook:
            data->multi_test_hook = (struct Hook *)tag->ti_Data;
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
            data->entry_minheight = tag->ti_Data;
            break;

        case MUIA_List_AdjustHeight:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_ADJUSTHEIGHT);
            break;

        case MUIA_List_AdjustWidth:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_ADJUSTWIDTH);
            break;

        case MUIA_List_AutoVisible:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_AUTOVISIBLE);
            break;

        case MUIA_List_ShowDropMarks:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_SHOWDROPMARKS);
            break;

        case MUIA_List_DragSortable:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_DRAGSORTABLE);
            break;

        case MUIA_Listview_ScrollerPos:
            data->scroller_pos = tag->ti_Data;
            break;

        case MUIA_Listview_Input:
            data->read_only = !tag->ti_Data;
            break;

        case MUIA_Listview_MultiSelect:
            data->multiselect = tag->ti_Data;
            break;

        case MUIA_Listview_DefClickColumn:
            data->def_click_column = tag->ti_Data;
            break;

        case MUIA_Listview_DragType:
            data->drag_type = tag->ti_Data;
            if (data->drag_type != MUIV_Listview_DragType_None)
                set(obj, MUIA_Draggable, TRUE);
            break;
        }
    }

    List_HandleScrollerPos(cl, obj);

    data->layout_hook.h_Entry = HookEntry;
    data->layout_hook.h_SubEntry = (HOOKFUNC) List_LayoutFunction;
    data->layout_hook.h_Data = data;
    set(obj, MUIA_Group_LayoutHook, (IPTR) &data->layout_hook);

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
    if (!ParseListFormat(data, data->format, TRUE))
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
        if ((tag->ti_Tag == MUIA_List_HideColumn || tag->ti_Tag == MUIA_List_ShowColumn)
            && tag->ti_Data < data->columns)
            for (LONG i = 0; i < data->columns; i++)
                    if (data->ci[i].order_id == tag->ti_Data)
                        data->ci[i].hidden = tag->ti_Tag == MUIA_List_HideColumn;

    /* This is necessary for at least the title */
    if (!SetListSize(data, 0))
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

    if (!(data->entries[ENTRY_TITLE] = AllocTitleEntry(data->columns_allocated)))
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }

    CONST_STRPTR *titles = (APTR)GetTagData(MUIA_List_TitleArray, 0, msg->ops_AttrList);
    if (titles)
    {
        ULONG count = data->max_columns_explicit ? data->max_columns : data->columns;
        data->title_array = titles;
        data->title_columns = count;
        data->title = (STRPTR)1;
    }

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

    if ((data->entries_num) && (new_entries_active != MUIV_List_Active_Off))
    {
        switch (new_entries_active)
        {
        case MUIV_List_Active_Top:
            new_entries_active = 0;
            break;

        case MUIV_List_Active_Bottom:
            new_entries_active = data->entries_num - 1;
            break;
        }

        if (new_entries_active < 0)
            new_entries_active = 0;
        else if (new_entries_active >= data->entries_num)
            new_entries_active = data->entries_num - 1;

        data->entries_active = new_entries_active;
        /* Selected entry will be moved into visible area */
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
    {
        struct ListEntry *lentry =
            data->entries[--data->confirm_entries_num];
        DoMethod(obj, MUIM_List_Destruct, (IPTR) lentry->data,
            (IPTR) data->pool);
        FreeListEntry(data, lentry);
    }

    if (data->intern_pool)
        DeletePool(data->intern_pool);
    if (data->entries)
    {
        FreeVec(data->entries[ENTRY_TITLE]); /* not pooled */
        FreeVec(data->entries - 1);
    }
            /* title is currently before all other elements */

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
#ifdef ZUNE68_GCC_NATIVE
        if (cr->obscured)
#else
        if (cr->lobs)
#endif
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
            data->hscroll_visibility = tag->ti_Data;
            break;
        case MUIA_List_SortColumn:
            data->sort_column = tag->ti_Data;
            break;
        case MUIA_List_Stripes:
            data->stripes = !!tag->ti_Data;
            data->update = UPDATEMODE_ALL;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            break;
        case MUIA_List_Editable:
            data->editable = !!tag->ti_Data;
            break;
        case MUIA_List_HideColumn:
        case MUIA_List_ShowColumn:
            if (tag->ti_Data < data->columns)
            {
                for (LONG i = 0; i < data->columns; i++)
                    if (data->ci[i].order_id == tag->ti_Data)
                        data->ci[i].hidden = tag->ti_Tag == MUIA_List_HideColumn;
                if (_flags(obj) & MADF_CANDRAW)
                {
                    DoMethod(obj, MUIM_Group_InitChange);
                    DoMethod(obj, MUIM_Group_ExitChange);
                }
                data->update = UPDATEMODE_ALL;
                MUI_Redraw(obj, MADF_DRAWUPDATE);
            }
            break;
        case MUIA_List_CompareHook:
            data->compare_hook = (struct Hook *)tag->ti_Data;
            if (data->compare_hook == NULL)
                data->compare_hook = &data->default_compare_hook;
            break;

        case MUIA_List_ConstructHook:
            data->construct_hook = (struct Hook *)tag->ti_Data;
            break;

        case MUIA_List_DestructHook:
            data->destruct_hook = (struct Hook *)tag->ti_Data;
            break;

        case MUIA_List_DisplayHook:
            data->display_hook = (struct Hook *)tag->ti_Data;
            break;

        case MUIA_List_MultiTestHook:
            data->multi_test_hook = (struct Hook *)tag->ti_Data;
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
        case MUIA_List_TitleArray:
            data->title_array = (APTR)tag->ti_Data;
            data->title_columns = data->max_columns_explicit ? data->max_columns : data->columns;
            data->title = data->title_array ? (STRPTR)1 : NULL;
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
            if (ParseListFormat(data, (STRPTR) tag->ti_Data, FALSE))
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
                        data->entries[old]->flags |= ENTRY_RENDER;
                    if (data->entries_active >= 0 && data->entries_active < data->entries_num)
                        data->entries[data->entries_active]->flags |= ENTRY_RENDER;
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

        case MUIA_List_AutoVisible:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_AUTOVISIBLE);
            break;

        case MUIA_List_ShowDropMarks:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_SHOWDROPMARKS);
            break;

        case MUIA_List_DragSortable:
            _handle_bool_tag(data->flags, tag->ti_Data, LIST_DRAGSORTABLE);
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

        case MUIA_Listview_Input: /* private set */
            data->read_only = !tag->ti_Data;
            break;

        case MUIA_Listview_MultiSelect: /* private set */
            data->multiselect = tag->ti_Data;
            break;

        case MUIA_Listview_DefClickColumn:
            data->def_click_column = tag->ti_Data;
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
                (IPTR) &data->hook, PROP_HORIZ_FIRST, MUIV_TriggerValue);
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
static ULONG List_LayoutFunction(struct Hook *hook, Object *obj,
    struct MUI_LayoutMsg *lm)
{
    struct MUI_ListData *data = (struct MUI_ListData *)hook->h_Data;
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

IPTR List__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    WORD strip = List_TitleStrip(obj, data);

    DoSuperMethodA(cl, obj, (Msg) msg);

    CalcWidths(cl, obj);

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

    zune_imspec_show(data->list_cursor, obj);
    zune_imspec_show(data->list_select, obj);
    zune_imspec_show(data->list_selcur, obj);
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

    zune_imspec_hide(data->list_cursor);
    zune_imspec_hide(data->list_select);
    zune_imspec_hide(data->list_selcur);

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

/**************************************************************************
 Draw an entry at entry_pos at the given row. To draw the title, set pos to
 ENTRY_TITLE. lc is the entry area's clip, NULL when one is installed.
**************************************************************************/
static __attribute__((noinline)) VOID List_DrawEntry(struct IClass *cl, Object *obj, int entry_pos,
    int y, struct ListClip *lc)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    struct ListEntry *entry = data->entries[entry_pos];
    int row_height = data->entry_maxheight;
    int y_offset = y;
    int col, x1, x2;

    /* To be sure we don't draw anything if there is no title */
    if (entry_pos == ENTRY_TITLE) {
        if (!data->title)
            return;
        row_height = entry ? entry->height : 0;
    }

    DisplayEntry(cl, obj, entry_pos);
    x1 = _mleft(data->area) - data->horiz_first;

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
             * vertically align the entry.
             */
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
                && x1 >= _mleft(data->area)
                && x1 + text->width - 1 <= _mright(data->area)
                && y_offset >= _mtop(data->area)
                && y_offset + text->height - 1 <= _mbottom(data->area)
                && !(_font(obj)->tf_Style & FSF_ITALIC)))
                List_NeedClip(obj, data, lc);
            /* Note, this was MPEN_SHADOW before */
            SetAPen(_rp(obj), muiRenderInfo(obj)->mri_Pens[MPEN_TEXT]);

            if (data->ci[col].elide)
            {
                text->truncate = TRUE;
                LONG right = MIN(x2 - 1, _mright(data->area));
                if (data->ci[col].elide >= MUIV_Text_Shorten_Hide)
                    zune_text_draw_shortened(text, obj, x1, right, y_offset, data->ci[col].elide);
                else zune_text_draw(text, obj, x1, right, y_offset);
            }
            else if (data->format_layout)
            {
                text->truncate = TRUE;
                zune_text_draw(text, obj, x1, MIN(x2 - 1, _mright(data->area)), y_offset);
            }
            else if (x1 + text->width - 1 > _mright(data->area)
                && ZUNE_APP_MUI38(obj))
            {
                /* MUI 3.8 draws only the whole characters of a too long
                 * entry that end before the area's last column (demopix:
                 * the long lines of MUI-Demo's String window); text that
                 * fits goes up to the edge (its Floattexts) */
                text->truncate = TRUE;
                zune_text_draw(text, obj, x1, _mright(data->area) - 1,
                    y_offset);
            }
            else zune_text_draw(text, obj, x1, x2, y_offset);
            if (!borrowed) zune_text_destroy(text);
        }
        if (entry_pos == ENTRY_TITLE && col == data->sort_column
            && data->ci[col].sortable)
            zune_list_draw_sort_mark(obj, data, x1, y, row_height);
        x1 = x2 + data->ci[col].delta + (data->ci[col].bar ? BAR_WIDTH : 0);
    }
}


/**************************************************************************
 MUIM_Draw
**************************************************************************/
IPTR List__MUIM_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    int entry_pos, y;
    struct ListClip clip = { (APTR)-1, FALSE };
    int start, end, exposed_start = 0, exposed_end = 0;
    BOOL scrolling = FALSE;
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
            List_DrawEntry(cl, obj, ENTRY_TITLE, _top(data->area)
                - List_TitleStrip(obj, data) + 2, NULL);
            MUI_RemoveClipping(muiRenderInfo(obj), clip);
        }
    }
    else if (data->title)
    {
        /* MUI 3.8: a pixel below the title, no separator lines (demopix:
         * MUI-Demo's Dirlist) */
        data->title_height = data->entries[ENTRY_TITLE]->height
            + (ZUNE_APP_MUI38(obj) ? 1 : 2);
    }
    else
    {
        data->title_height = 0;
    }

    zune_list_layout_columns(data);
    /* Calc the numbers of entries visible */
    CalcVertVisible(cl, obj);

    entries_height = _mheight(data->area);
    entries_bottom = _mbottom(data->area);

    if ((msg->flags & MADF_DRAWUPDATE) == 0 || data->update == UPDATEMODE_ALL)
    {
        DoMethod(obj, MUIM_DrawBackground, _mleft(data->area),
            _mtop(data->area), _mwidth(data->area), entries_height,
            0, data->entries_first * data->entry_maxheight, 0);
    }

    /* A full repaint or a scroll clips at once, as it nearly always
     * draws a cut-off last row: a clip installed later would wait for
     * the blitter to finish the rows queued so far. Other updates repaint
     * a few rows and clip only when one needs it (List_NeedClip). An
     * empty list without a title draws nothing in the entry area. */
    if (((msg->flags & MADF_DRAWUPDATE) == 0
            || (data->update & UPDATEMODE_ALL))
        && (data->entries_num != 0 || (data->title_height && data->title)))
        List_NeedClip(obj, data, &clip);

    if ((msg->flags & MADF_DRAWUPDATE) == 0 || data->update == UPDATEMODE_ALL)
    {
        y = _mtop(data->area);
        /* Draw Title
         */
        if (data->title_height && data->title)
        {
            if (y + data->entries[ENTRY_TITLE]->height + 1 > entries_bottom)
                List_NeedClip(obj, data, &clip);
            List_DrawEntry(cl, obj, ENTRY_TITLE, y, &clip);
            y += data->entries[ENTRY_TITLE]->height;
            if (ZUNE_APP_MUI38(obj))
                goto title_done;
            SetAPen(_rp(obj), _pens(obj)[MPEN_SHADOW]);
            Move(_rp(obj), _mleft(data->area), y);
            Draw(_rp(obj), _mright(data->area), y);
            SetAPen(_rp(obj), _pens(obj)[MPEN_SHINE]);
            y++;
            Move(_rp(obj), _mleft(data->area), y);
            Draw(_rp(obj), _mright(data->area), y);
          title_done:;
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

            /* the last row may be cut off by the area */
            ScrollRaster(_rp(obj), 0, diffy * data->entry_maxheight,
                _mleft(data->area), y,
                _mright(data->area),
                MIN(y + data->entry_maxheight * data->entries_visible,
                    entries_bottom));

            scroll_caused_damage =
                scroll_caused_damage
                && (_rp(obj)->Layer->Flags & LAYERREFRESH);

            if (diffy > 0)
            {
                start = end - diffy;
                /* The former bottom row may have been clipped: its newly
                 * visible lower pixels cannot be recovered by the blit. */
                if ((entries_bottom - data->entries_top_pixel + 1) % data->entry_maxheight)
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

        DoMethod(obj, MUIM_DrawBackground, _mleft(data->area), top,
            _mwidth(data->area), bottom - top + 1, 0,
            top - _mtop(data->area) + data->entries_first
            * data->entry_maxheight, 0);
        exposed_start = start;
        exposed_end = end;
        scrolling = TRUE;
        start = data->entries_first;
        end = start + data->entries_visible;
        y = data->entries_top_pixel;
    }

    for (entry_pos = start;
        entry_pos < end && entry_pos < data->entries_num; entry_pos++)
    {
        struct ListEntry *entry = data->entries[entry_pos];

        if (!(msg->flags & MADF_DRAWUPDATE) ||
            ((msg->flags & MADF_DRAWUPDATE) && data->update == UPDATEMODE_ALL) ||
            (scrolling && ((entry_pos >= exposed_start && entry_pos < exposed_end)
                || (entry->flags & (ENTRY_RENDER | ENTRY_SELECTED))
                || entry_pos == data->entries_active)) ||
            ((msg->flags & MADF_DRAWUPDATE) && data->update == UPDATEMODE_ENTRY
                && data->update_pos == entry_pos) ||
            ((msg->flags & MADF_DRAWUPDATE) && data->update == UPDATEMODE_NEEDED
                && (entry->flags & ENTRY_RENDER)))
        {
            /* a cut-off row: its highlight and background too */
            if (y < _mtop(data->area)
                || y + data->entry_maxheight - 1 > entries_bottom)
                List_NeedClip(obj, data, &clip);

            /* Choose appropriate highlight image */

            if (entry_pos == data->entries_active
                && (entry->flags & ENTRY_SELECTED) && !data->read_only)
                highlight = data->list_selcur;
            else if (entry_pos == data->entries_active && !data->read_only)
                highlight = data->list_cursor;
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
                    DoMethod(obj, MUIM_DrawBackground, _mleft(data->area), y,
                        _mwidth(data->area), data->entry_maxheight, 0,
                        y - _mtop(data->area) +
                        data->entries_first * data->entry_maxheight, 0);
                zune_imspec_draw(highlight, muiRenderInfo(obj),
                    _mleft(data->area), y, _mwidth(data->area),
                    data->entry_maxheight,
                    0, y - data->entries_top_pixel, 0);
            }
            else if ((scrolling && (entry->flags & ENTRY_RENDER)) ||
            ((msg->flags & MADF_DRAWUPDATE) && data->update == UPDATEMODE_ENTRY
                && data->update_pos == entry_pos) ||
            ((msg->flags & MADF_DRAWUPDATE) && data->update == UPDATEMODE_NEEDED
                && (entry->flags & ENTRY_RENDER)))
            {
                DoMethod(obj, MUIM_DrawBackground, _mleft(data->area), y,
                    _mwidth(data->area), data->entry_maxheight, 0,
                    y - _mtop(data->area) +
                    data->entries_first * data->entry_maxheight, 0);
            }

            if (!highlight && data->stripes && (entry_pos & 1))
            {
                SetAPen(_rp(obj), _pens(obj)[MPEN_HALFSHINE]);
                RectFill(_rp(obj), _mleft(data->area), y,
                    _mright(data->area), y + data->entry_maxheight - 1);
            }
            List_DrawEntry(cl, obj, entry_pos, y, &clip);
            entry->flags &= ~ENTRY_RENDER;
        }
        y += data->entry_maxheight;
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

    LONG x1 = _mleft(data->area) - data->horiz_first;
    ULONG col;
    y = _mtop(data->area);

    if (data->title_height && data->title)
    {
        for (col = 0; col < data->columns; col++)
        {
            ULONG halfdelta = data->ci[col].delta / 2;
            if (ColumnHidden(data, col)) continue;
            x1 += ColumnWidth(data, col) + halfdelta;

            if (x1 + (data->ci[col].bar ? BAR_WIDTH : 0) > _mright(data->area))
                break;

            if (data->ci[col].bar)
            {
                if (x1 < _mleft(data->area))
                    List_NeedClip(obj, data, &clip);
                /* MUI 3.9: shadow, then shine (tests/native/barfmt.c) */
                SetAPen(_rp(obj), _pens(obj)[MPEN_SHADOW]);
                Move(_rp(obj), x1, y);
                Draw(_rp(obj), x1,
                    y + data->entries[ENTRY_TITLE]->height - 1);
                SetAPen(_rp(obj), _pens(obj)[MPEN_SHINE]);
                Move(_rp(obj), x1 + 1, y);
                Draw(_rp(obj), x1 + 1,
                    y + data->entries[ENTRY_TITLE]->height - 1);

                x1 += BAR_WIDTH;
            }
            x1 += data->ci[col].delta - halfdelta;
        }
        y += data->entries[ENTRY_TITLE]->height + 1;
    }

    x1 = _mleft(data->area) - data->horiz_first;

    for (col = 0; col < data->columns; col++)
    {
        ULONG halfdelta = data->ci[col].delta / 2;
        if (ColumnHidden(data, col)) continue;
        x1 += ColumnWidth(data, col) + halfdelta;

        if (x1 + (data->ci[col].bar ? BAR_WIDTH : 0) > _mright(data->area))
            break;

        if (data->ci[col].bar)
        {
            if (x1 < _mleft(data->area) || y > entries_bottom)
                List_NeedClip(obj, data, &clip);
            SetAPen(_rp(obj), _pens(obj)[MPEN_SHADOW]);
            Move(_rp(obj), x1, y);
            Draw(_rp(obj), x1, entries_bottom);
            SetAPen(_rp(obj), _pens(obj)[MPEN_SHINE]);
            Move(_rp(obj), x1 + 1, y);
            Draw(_rp(obj), x1 + 1, entries_bottom);

            x1 += BAR_WIDTH;
        }

        x1 += data->ci[col].delta - halfdelta;
    }

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
        struct ListEntry *lentry =
            data->entries[--data->confirm_entries_num];
        DoMethod(obj, MUIM_List_Destruct, (IPTR) lentry->data,
            (IPTR) data->pool);
        FreeListEntry(data, lentry);

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
        struct ListEntry *save = data->entries[pos1];
        data->entries[pos1] = data->entries[pos2];
        data->entries[pos2] = save;

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
                if (data->entries[i]->data == msg->entry)
                {
                    pos = i;
                    break;
                }
        }
        else
            pos = msg->pos;

        if (pos != -1)
        {
            data->entries[pos]->flags |= ENTRY_RENDER;
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
    LONG pos;
    LONG new_act;
    LONG i;
    BOOL found, done = FALSE;
    struct ListEntry *lentry;
    Tag active_tag = TAG_DONE;

    if (!data->entries_num)
        return 0;

    switch (msg->pos)
    {
    case MUIV_List_Remove_First:
        pos = 0;
        break;

    case MUIV_List_Remove_Active:
        pos = data->entries_active;
        break;

    case MUIV_List_Remove_Last:
        pos = data->entries_num - 1;
        break;

    case MUIV_List_Remove_Selected:
        pos = 0;
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
     * entries on a 68000). Without a selected one, the active entry goes
     * as before. */
    if (msg->pos == MUIV_List_Remove_Selected)
    {
        LONG n = data->confirm_entries_num, w = 0;
        LONG active = data->entries_active;

        for (i = 0; i < n; i++)
            if (data->entries[i]->flags & ENTRY_SELECTED)
                break;
        if (i < n)
        {
            for (w = i; i < n; i++)
            {
                lentry = data->entries[i];
                if (!(lentry->flags & ENTRY_SELECTED))
                {
                    data->entries[w++] = lentry;
                    continue;
                }
                DoMethod(obj, MUIM_List_Destruct, (IPTR) lentry->data,
                    (IPTR) data->pool);
                FreeListEntry(data, lentry);
                if (i < active)
                {
                    new_act--;
                    active_tag = MUIA_List_Active;
                }
                else if (i == active)
                    active_tag = MUIA_List_Active;
            }
            data->confirm_entries_num = w;
            done = TRUE;
        }
    }

    while (!done)
    {
        if (msg->pos == MUIV_List_Remove_Selected)
        {
            /* Find the next selected entry */
            for (found = FALSE, i = pos;
                i < data->confirm_entries_num && !found; i++)
            {
                if (data->entries[i]->flags & ENTRY_SELECTED)
                {
                    pos = i;
                    found = TRUE;
                }
            }

            if (!found)
            {
                done = TRUE;

                /* If there were no selected entries, remove the active one */
                if (data->confirm_entries_num == data->entries_num
                    && data->entries_active != MUIV_List_Active_Off)
                {
                    pos = data->entries_active;
                    found = TRUE;
                }
            }
        }
        else
        {
            done = TRUE;
            found = TRUE;
        }

        if (found)
        {
            lentry = data->entries[pos];
            DoMethod(obj, MUIM_List_Destruct, (IPTR) lentry->data,
                (IPTR) data->pool);
            FreeListEntry(data, lentry);
            RemoveListEntries(data, pos, 1);
            data->confirm_entries_num--;

            if (pos < new_act)
            {
                new_act--;
                active_tag = MUIA_List_Active;
            }
            else if (pos == new_act)
                active_tag = MUIA_List_Active;
        }
    }

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
            if (data->entries[i]->flags & ENTRY_SELECTED)
            {
                selcount++;
                if (data->multi_test_hook != NULL && selcount == 1)
                    multi_allowed = CallHookPkt(data->multi_test_hook, NULL,
                        data->entries[i]->data);
            }
        }
    }

    /* Change or check state of each entry in the range */
    for (i = pos; i < pos + count; i++)
    {
        state = data->entries[i]->flags & ENTRY_SELECTED;
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
            if (data->entries[i]->flags & ENTRY_SELECTED)
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
                            NULL, data->entries[i]->data);
                    else
                        new_multi_allowed = TRUE;

                    /* Check if the entry to be selected can be selected at
                       the same time as the already selected entries */
                    if (new_multi_allowed || selcount == 0)
                    {
                        /* Select the entry and update the selection count
                           and flag */
                        data->entries[i]->flags |= ENTRY_SELECTED;
                        selcount++;

                        multi_allowed = new_multi_allowed;
                    }
                }
            }
            else if (!new_select_state && state)
            {
                data->entries[i]->flags &= ~ENTRY_SELECTED;
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

static LONG CompareEntries(Object *obj, struct MUI_ListData *data, struct ListEntry *a,
    struct ListEntry *b);

/* Insert one appended entry into the sorted prefix, after equal keys. */
static LONG InsertSortedEntry(Object *obj, struct MUI_ListData *data, struct ListEntry **entries, LONG at)
{
    struct ListEntry *entry = entries[at];
    LONG lo = 0, hi = at, mid;
    while (lo < hi)
    {
        mid = lo + (hi - lo) / 2;
        if (CompareEntries(obj, data, entries[mid], entry) <= 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    while (at > lo)
    {
        entries[at] = entries[at - 1];
        entries[at]->flags |= ENTRY_RENDER;
        at--;
    }
    entries[lo] = entry;
    entry->flags |= ENTRY_RENDER;
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

    if (!(PrepareInsertListEntries(data, pos, count)))
        return ~0;

    while (pos < until)
    {
        struct ListEntry *lentry;

        lentry = AllocListEntry(data);
        if (lentry)
            lentry->data = (APTR)DoMethod(obj, MUIM_List_Construct,
                (IPTR)*toinsert, (IPTR)data->pool);
        if (!lentry || !lentry->data)
        {
            if (lentry) FreeListEntry(data, lentry);
            /* The reserved gap extends past entries_num. Move only the
             * original tail, never a negative (entries_num - until) size. */
            memmove(&data->entries[pos], &data->entries[until],
                (data->entries_num - data->insert_position)
                * sizeof(struct ListEntry *));
            count = pos - data->insert_position;
            if (!count) return ~0;
            failed = TRUE;
            break;
        }

        lentry->flags |= ENTRY_RENDER;
        data->entries[pos] = lentry;
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
        CalcVertVisible(cl, obj);
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
        struct ListEntry *inserted = data->entries[data->insert_position];
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
            DoMethod(obj, MUIM_List_Sort);
            for (i = 0; i < data->entries_num; i++)
                if (data->entries[i] == inserted)
                {
                    data->insert_position = i;
                    break;
                }
        }

        if ((adjusted) && (data->flags & LIST_QUIET))
            data->update = UPDATEMODE_ALL;
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
    *msg->entry = data->entries[pos]->data;
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
static LONG CompareEntries(Object *obj, struct MUI_ListData *data, struct ListEntry *a,
    struct ListEntry *b)
{
    struct MUIP_List_Compare msg = { MUIM_List_Compare, NULL, NULL, {0}, 0 };
    msg.column = data->sort_column;
    msg.entry1 = a->data;
    msg.entry2 = b->data;
    LONG result = (LONG)DoMethodA(obj, (Msg)&msg);
    if (data->sort_column >= 0 && data->sort_column < data->columns &&
        data->ci[data->sort_column].descending)
        result = result < 0 ? 1 : result > 0 ? -1 : 0;
    return result;
}

static void SiftEntries(Object *obj, struct MUI_ListData *data, struct ListEntry **entries,
    LONG root, LONG count)
{
    struct ListEntry *entry = entries[root];
    LONG child;

    while (root < count / 2)
    {
        child = root * 2 + 1;
        if (child + 1 < count
            && CompareEntries(obj, data, entries[child], entries[child + 1]) < 0)
            child++;
        if (CompareEntries(obj, data, entry, entries[child]) >= 0) break;
        entries[root] = entries[child];
        entries[root]->flags |= ENTRY_RENDER;
        root = child;
    }
    entries[root] = entry;
    entry->flags |= ENTRY_RENDER;
}

IPTR List__MUIM_Sort(struct IClass *cl, Object *obj,
    struct MUIP_List_Sort *msg)
{
    struct MUI_ListData *data = INST_DATA(cl, obj);
    if (data->edit_object) ListEndEdit(cl, obj, MUIV_List_EndEdit_Abort, TRUE);
    struct ListEntry **entries = data->entries;
    struct ListEntry *active = NULL, *entry;
    LONG count = data->entries_num, i;

    if (count < 2) return 0;
    /* Verify order each time: callers may change comparison keys in place. */
    for (i = 1; i < count; i++)
        if (CompareEntries(obj, data, entries[i - 1], entries[i]) > 0) break;
    if (i == count) return 0;
    if (data->entries_active >= 0 && data->entries_active < count)
        active = entries[data->entries_active];

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
            entry = entries[0];
            entries[0] = entries[i];
            entries[i] = entry;
            entry->flags |= ENTRY_RENDER;
            SiftEntries(obj, data, entries, 0, i);
        }
    }
    if (active)
        for (i = 0; i < count; i++)
            if (entries[i] == active)
            {
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
    int i;

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

    /* Shift all entries in the range between the 'from' and 'to' positions */
    if (from < to)
    {
        struct ListEntry *backup = data->entries[from];
        for (i = from; i < to; i++)
            data->entries[i] = data->entries[i + 1];
        data->entries[to] = backup;
    }
    else
    {
        struct ListEntry *backup = data->entries[from];
        for (i = from; i > to; i--)
            data->entries[i] = data->entries[i - 1];
        data->entries[to] = backup;
    }

#if 0    /* Not done in MUI 3 */
    /* Update index of active entry */
    if (from == data->entries_active)
        data->entries_active = to;
    else if (data->entries_active > from && data->entries_active < to)
        data->entries_active--;
    else if (data->entries_active < from && data->entries_active >= to)
        data->entries_active++;
#endif

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
        if (data->entries[i]->flags & ENTRY_SELECTED)
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
        mx -= _left(data->area);
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
        else if (mx >= _width(data->area))
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
    if (data->edit_object && msg->imsg && msg->imsg->Class == IDCMP_RAWKEY &&
        msg->imsg->Code == 0x45)
    {
        DoMethod(_app(obj), MUIM_Application_PushMethod, obj, 2,
            MUIM_List_EndEdit, MUIV_List_EndEdit_Abort);
        return MUI_EventHandlerRC_Eat;
    }

    struct MUI_List_TestPos_Result pos;
    LONG seltype = MUIV_List_Select_On, old_active, new_active, visible,
        first, last, i;
    IPTR result = 0;
    BOOL select = FALSE, clear = FALSE, range_select = FALSE, changing;
    WORD delta;
    typeof(msg->muikey) muikey = msg->muikey;

    new_active = old_active = XGET(obj, MUIA_List_Active);
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
                DoMethod(obj, MUIM_List_Jump, 0);
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
            DoMethod(obj, MUIM_List_Jump, MUIV_List_Jump_Up);
            break;

        case MUIKEY_RIGHT:
        case MUIKEY_WORDRIGHT:
            DoMethod(obj, MUIM_List_Jump, MUIV_List_Jump_Down);
            break;

        case MUIKEY_LINESTART:
            DoMethod(obj, MUIM_List_Jump, MUIV_List_Jump_Top);
            break;

        case MUIKEY_LINEEND:
            DoMethod(obj, MUIM_List_Jump, MUIV_List_Jump_Bottom);
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
    }
    else if (msg->imsg)
    {
        DoMethod(obj, MUIM_List_TestPos, msg->imsg->MouseX, msg->imsg->MouseY,
            (IPTR) &pos);

        switch (msg->imsg->Class)
        {
        case IDCMP_MOUSEBUTTONS:
            if (msg->imsg->Code == SELECTDOWN)
            {
                if (data->title && msg->imsg->MouseY >= _mtop(obj) &&
                    msg->imsg->MouseY < data->entries_top_pixel &&
                    msg->imsg->MouseX >= _mleft(data->area) &&
                    msg->imsg->MouseX <= _mright(data->area))
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
                if (_isinobject(data->area, msg->imsg->MouseX,
                    msg->imsg->MouseY)
                    && !(data->horiz_shown && _isinobject(data->horiz,
                        msg->imsg->MouseX, msg->imsg->MouseY)))
                {
                    data->mouse_click = MOUSE_CLICK_ENTRY;

                    if (!data->read_only && pos.entry != -1)
                    {
                        new_active = pos.entry;

                        clear = (data->multiselect
                            == MUIV_Listview_MultiSelect_Shifted
                            && (msg->imsg->Qualifier
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
                            msg->imsg->Seconds, msg->imsg->Micros))
                        {
                            set(obj, MUIA_Listview_DoubleClick, TRUE);
                            data->last_active = -1;
                            data->last_secs = data->last_mics = 0;
                        }
                        else
                        {
                            data->last_active = pos.entry;
                            data->last_secs = msg->imsg->Seconds;
                            data->last_mics = msg->imsg->Micros;

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
                if (msg->imsg->Code == SELECTUP && data->mouse_click)
                {
                    /* cache click position ... */
                    data->mouse_x = msg->imsg->MouseX;
                    data->mouse_y = msg->imsg->MouseY;

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
            if (data->vert && _isinobject(data->vert, msg->imsg->MouseX,
                msg->imsg->MouseY))
                delta = 1;
            else if (_isinobject(data->area, msg->imsg->MouseX,
                msg->imsg->MouseY))
                delta = 4;
            else
                delta = 0;

            if (delta != 0)
            {
                switch (msg->imsg->Code)
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
static LONG ListCompareRaw(Object *obj, struct MUI_ListData *data, APTR a, APTR b)
{
    struct MUIP_List_Compare msg = {MUIM_List_Compare, a, b, {data->sort_column}, 0};
    LONG result = (LONG)DoMethodA(obj, (Msg)&msg);
    if (data->sort_column >= 0 && data->sort_column < data->columns &&
        data->ci[data->sort_column].descending)
        result = result < 0 ? 1 : result > 0 ? -1 : 0;
    return result;
}

static void ListSiftRaw(Object *obj, struct MUI_ListData *data, APTR *entries,
    LONG root, LONG count)
{
    APTR entry = entries[root];
    while (root < count / 2)
    {
        LONG child = root * 2 + 1;
        if (child + 1 < count && ListCompareRaw(obj, data, entries[child], entries[child + 1]) < 0) child++;
        if (ListCompareRaw(obj, data, entry, entries[child]) >= 0) break;
        entries[root] = entries[child]; root = child;
    }
    entries[root] = entry;
}

static IPTR ListSortEntries(struct IClass *cl, Object *obj, APTR *entries)
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

#if !defined(ZUNE68_GCC_NATIVE)
#include "../listx.c"
#endif

BOOPSI_DISPATCHER(IPTR, List_Dispatcher, cl, obj, msg)
{
    switch (msg->MethodID)
    {
    case MUIM_FindAreaObject:
    {
        struct MUI_ListData *data = INST_DATA(cl, obj);
        IPTR found = data->edit_object ? DoMethodA(data->edit_object, msg) : 0;
        return found ? found : DoSuperMethodA(cl, obj, msg);
    }
    case MUIM_List_SortEntries:
        return ListSortEntries(cl, obj, ((struct MUIP_List_SortEntries *)msg)->entries);
    case MUIM_List_Edit:
        return ListEdit(cl, obj, ((struct MUIP_List_Edit *)msg)->row,
            ((struct MUIP_List_Edit *)msg)->column);
    case MUIM_List_EndEdit:
        return ListEndEdit(cl, obj, ((struct MUIP_List_EndEdit *)msg)->mode, FALSE);
    case MUIM_List_CreateEditObject: return 0;
    case MUIM_List_EditDone: return TRUE;
    case OM_NEW:
        return List__OM_NEW(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return List__OM_DISPOSE(cl, obj, msg);
    case OM_SET:
        return List__OM_SET(cl, obj, (struct opSet *)msg);
    case OM_GET:
        return List__OM_GET(cl, obj, (struct opGet *)msg);

    case MUIM_Setup:
        return List__MUIM_Setup(cl, obj, (struct MUIP_Setup *)msg);
    case MUIM_Cleanup:
        return List__MUIM_Cleanup(cl, obj, (struct MUIP_Cleanup *)msg);
    case MUIM_HandleEvent:
        return List__MUIM_HandleEvent(cl, obj, (struct MUIP_HandleEvent *)msg);
    case MUIM_AskMinMax:
        return List__MUIM_AskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
    case MUIM_Show:
        return List__MUIM_Show(cl, obj, (struct MUIP_Show *)msg);
    case MUIM_Hide:
        return List__MUIM_Hide(cl, obj, (struct MUIP_Hide *)msg);
    case MUIM_Draw:
        return List__MUIM_Draw(cl, obj, (struct MUIP_Draw *)msg);
    case MUIM_Zune_Layout:
        return List__MUIM_Layout(cl, obj, (struct MUIP_Zune_Layout *)msg);
    case MUIM_List_Clear:
        return List__MUIM_Clear(cl, obj, (struct MUIP_List_Clear *)msg);
    case MUIM_List_Sort:
        return List__MUIM_Sort(cl, obj, (struct MUIP_List_Sort *)msg);
    case MUIM_List_Exchange:
        return List__MUIM_Exchange(cl, obj,
            (struct MUIP_List_Exchange *)msg);
    case MUIM_List_Insert:
        return List__MUIM_Insert(cl, obj, (APTR) msg);
    case MUIM_List_InsertSingle:
        return List__MUIM_InsertSingle(cl, obj, (APTR) msg);
    case MUIM_List_GetEntry:
        return List__MUIM_GetEntry(cl, obj, (APTR) msg);
    case MUIM_List_Redraw:
        return List__MUIM_Redraw(cl, obj, (APTR) msg);
    case MUIM_List_Remove:
        return List__MUIM_Remove(cl, obj, (APTR) msg);
    case MUIM_List_Select:
        return List__MUIM_Select(cl, obj, (APTR) msg);
    case MUIM_List_Construct:
        return List__MUIM_Construct(cl, obj, (APTR) msg);
    case MUIM_List_Destruct:
        return List__MUIM_Destruct(cl, obj, (APTR) msg);
    case MUIM_List_Compare:
        return List__MUIM_Compare(cl, obj, (APTR) msg);
    case MUIM_List_Display:
        return List__MUIM_Display(cl, obj, (APTR) msg);
    case MUIM_List_SelectChange:
        return List__MUIM_SelectChange(cl, obj, (APTR) msg);
    case MUIM_List_CreateImage:
        return List__MUIM_CreateImage(cl, obj, (APTR) msg);
    case MUIM_List_DeleteImage:
        return List__MUIM_DeleteImage(cl, obj, (APTR) msg);
    case MUIM_List_Jump:
        return List__MUIM_Jump(cl, obj, (APTR) msg);
    case MUIM_List_Move:
        return List__MUIM_Move(cl, obj, (struct MUIP_List_Move *)msg);
    case MUIM_List_NextSelected:
        return List__MUIM_NextSelected(cl, obj,
           (struct MUIP_List_NextSelected *)msg);
    case MUIM_List_TestPos:
        return List__MUIM_TestPos(cl, obj, (APTR) msg);
    case MUIM_DragQuery:
        return List__MUIM_DragQuery(cl, obj, (APTR) msg);
    case MUIM_DragFinish:
        return List__MUIM_DragFinish(cl, obj, (APTR) msg);
    case MUIM_DragReport:
        return zune_list_drag_report(cl, obj, (APTR) msg);
    case MUIM_DragDrop:
        return zune_list_drag_drop(cl, obj, (APTR) msg);
    case MUIM_CreateDragImage:
        return zune_list_create_drag_image(cl, obj, (APTR) msg);
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
