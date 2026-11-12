/*
    Copyright  2002-2026, The AROS Development Team.
    All rights reserved.

*/

/* List's instance data and the definitions classes/list.c and the list
 * module (listx.c) share. Private to them. */

#ifndef _MUI_CLASSES_LIST_PRIVATE_H
#define _MUI_CLASSES_LIST_PRIVATE_H

#define ENTRY_TITLE (-1)
/* Display array slots reserved at least (see ParseListFormat) */
#define LIST_MIN_RESERVED 16

#define FORMAT_TEMPLATE "DELTA=D/K/N,PREPARSE=P/K,WEIGHT=W/K/N,MINWIDTH=MIW/K," \
    "MAXWIDTH=MAW/K,COL=C/K/N,BAR/S,H=HIDDEN/S,SORTABLE/S,ORDER/K," \
    "ISOBJECT/S,ELIDE=E/K,FIXWIDTH/K/N"

#define BAR_WIDTH 2

#define UPDATEMODE_ALL                  (1 << 0)
#define UPDATEMODE_ENTRY                (1 << 1)
#define UPDATEMODE_NEEDED               (1 << 2)

enum
{
    ARG_DELTA,
    ARG_PREPARSE,
    ARG_WEIGHT,
    ARG_MINWIDTH,
    ARG_MAXWIDTH,
    ARG_COL,
    ARG_BAR,
    ARG_HIDDEN,
    ARG_SORTABLE,
    ARG_ORDER,
    ARG_ISOBJECT,
    ARG_ELIDE,
    ARG_FIXWIDTH,
    ARG_CNT
};


struct ListEntry
{
    APTR data;
    LONG width;    /* Line width */
    LONG height;   /* Line height */
    WORD flags;    /* see below */
    LONG widths[]; /* Widths of the columns */
};

#define ENTRY_SELECTED   (1<<0)
#define ENTRY_RENDER     (1<<1)


struct ColumnInfo
{
    int colno;      /* Column number */
    int user_width; /* user set width; -1 if entry width */
    int min_width;  /* min width percentage */
    int max_width;  /* min width percentage */
    int weight;
    int delta;      /* ignored for the first and last column, defaults to 4 */
    int bar;
    STRPTR preparse;
    int entries_width; /* width of the entries (maximum of all widths) */
    UBYTE hidden, sortable, descending;
    int order_id;
    UBYTE min_pixels, max_pixels, is_object, layout_hidden;
    LONG elide, layout_width, layout_max;
    UBYTE pad[4];
};
/* 64 bytes: data->ci[col] is a shift (it was 66 bytes: a shift, an add
 * and a doubling at each of the ~110 places that index it) */
_Static_assert(sizeof(struct ColumnInfo) == 64, "ColumnInfo is 64 bytes");

struct MUI_ImageSpec_intern;

struct MUI_ListData
{
    /* bool attrs */
    ULONG flags;

    APTR intern_pool;    /* The internal pool which the class has allocated */
    LONG intern_puddle_size;
    LONG intern_thresh_size;
    APTR pool;           /* the pool which is used to allocate list entries */

    struct Hook *construct_hook;
    struct Hook *compare_hook;
    struct Hook *destruct_hook;
    struct Hook *display_hook;
    struct Hook *multi_test_hook;

    struct Hook default_compare_hook;

    /* List management, currently we use a simple flat array, which is not
     * good if many entries are inserted/deleted */
    LONG entries_num;           /* Number of Entries in the list */
    LONG entries_allocated;
    struct ListEntry **entries;

    LONG entries_first;         /* first visible entry */
    LONG entries_visible;       /* number of visible entries,
                                 * determined at MUIM_Zune_Layout */
    LONG entries_active;
    LONG insert_position;       /* pos of the last insertion */

    LONG entry_maxheight;       /* Maximum height of an entry */
    ULONG entry_minheight;      /* from MUIA_List_MinLineHeight */

    LONG entries_totalheight;
    LONG entries_maxwidth;

    LONG vertprop_entries;
    LONG vertprop_visible;
    LONG vertprop_first;

    LONG confirm_entries_num;   /* These are the correct entries num, used
                                 * so you cannot set MUIA_List_Entries to
                                 * wrong values */

    LONG entries_top_pixel;     /* Where the entries start */

    /* Column managment, is allocated by ParseListFormat() and freed
     * by CleanListFormat() */
    STRPTR format;
    LONG columns;               /* Number of columns the list has */
    LONG columns_allocated;     /* List has space for columns_allocated columns */
    struct ColumnInfo *ci;
    STRPTR *preparses;
    STRPTR *strings_mem;        /* safe pointer to allocated memory for strings[] */
    STRPTR *strings;            /* the strings for the display function, one
                                 * more than needed (for the entry position) */

    /* Titlestuff */
    WORD title_height;          /* The complete height of the title */
    STRPTR title;               /* On single column lists this is the title,
                                 * otherwise 1. NULL for no title(s) */

    /* Cursor images */
    struct MUI_ImageSpec_intern *list_cursor;
    struct MUI_ImageSpec_intern *list_select;
    struct MUI_ImageSpec_intern *list_selcur;

    /* Render optimization */
    int update_pos;

    LONG drop_mark;
    LONG drop_mark_y;

    /* list images */
    struct MinList images;

    /* user prefs */
    UWORD prefs_linespacing;
    UWORD prefs_smoothval;

    /* render space handling */
    Object *area;

    /***************************/
    /* Former Listview members */
    /***************************/

    Object *vert;

    /* MUI 3.9: a horizontal scroller below the framed entry area when the
     * columns are wider than it; the entries scroll by horiz_first
     * pixels */
    Object *horiz;
    LONG horiz_first;
    struct Hook layout_hook;
    IPTR scroller_pos;

    /* clicked column */
    LONG click_column;
    LONG def_click_column;

    LONG mouse_x;
    LONG mouse_y;

    /* double click */
    ULONG last_secs;
    ULONG last_mics;
    ULONG last_active;

    struct MUI_EventHandlerNode ehn;

    LONG seltype;

    struct Hook hook;
    LONG hscroll_visibility, title_click, sort_column;
    Object *edit_object, *edit_focus;
    LONG edit_row, edit_column;
    APTR edit_entry;
    LONG max_columns;
    CONST_STRPTR *title_array;
    LONG title_columns;
    STRPTR *mapped_strings;
    LONG columns_reserved;      /* slots in strings[] and preparses[] */
    LONG columns_used;          /* highest column number the format shows + 1 */

    /* The switches and small modes as bytes, together (they were BOOLs,
     * enums and LONGs between pointers, each with its padding) */
    int update; /* UPDATEMODE_*: 1 - update everything, 2 - redraw
                   * entry at update_pos, 3 - scroll to current
                   * entries_first (old value is in update_pos) */
    UBYTE prefs_refresh;        /* ListviewRefresh */
    UBYTE prefs_multi;          /* ListviewMulti */
    UBYTE prefs_smoothed;
    UBYTE area_replaced;
    UBYTE area_connected;
    UBYTE vert_connected;
    UBYTE horiz_shown;
    UBYTE read_only;
    UBYTE multiselect;          /* MUIV_Listview_MultiSelect_* */
    UBYTE drag_type;            /* MUIV_Listview_DragType_* */
    UBYTE mouse_click;          /* see below if mouse is held down */
    UBYTE doubleclick;
    UBYTE stripes, editable;
    BOOL ending_edit;
    UBYTE format_layout, max_columns_explicit;
};

#define MOUSE_CLICK_ENTRY 1     /* on entry clicked */
#define MOUSE_CLICK_TITLE 2     /* on title clicked */

#define LIST_ADJUSTWIDTH   (1<<0)
#define LIST_ADJUSTHEIGHT  (1<<1)
#define LIST_AUTOVISIBLE   (1<<2)
#define LIST_DRAGSORTABLE  (1<<3)
#define LIST_SHOWDROPMARKS (1<<4)
#define LIST_QUIET         (1<<5)
#define LIST_CHANGED    (1<<6)
#define LIST_HORIZCHECK (1<<7)  /* check the horizontal scroller when quiet ends */
#define LIST_DIMSVALID  (1<<8)  /* CalcWidths' results still hold (List_Dispatcher) */


static inline __attribute__((always_inline)) LONG ColumnWidth(struct MUI_ListData *data, LONG column)
{
    return data->format_layout ? data->ci[column].layout_width :
        data->ci[column].entries_width;
}
static inline __attribute__((always_inline)) BOOL ColumnHidden(struct MUI_ListData *data, LONG column)
{
    return data->ci[column].hidden || data->ci[column].layout_hidden;
}

/* listx.c: in-place editing and drag and drop (the list module in the
 * native build) */
IPTR zune_list_edit(struct IClass *cl, Object *obj, LONG row, LONG column);
IPTR zune_list_end_edit(struct IClass *cl, Object *obj, ULONG mode,
    BOOL force);
void zune_list_layout_editor(Object *obj, struct MUI_ListData *data);
IPTR zune_list_drag_report(struct IClass *cl, Object *obj,
    struct MUIP_DragReport *msg);
IPTR zune_list_drag_drop(struct IClass *cl, Object *obj,
    struct MUIP_DragDrop *msg);
IPTR zune_list_create_drag_image(struct IClass *cl, Object *obj,
    struct MUIP_CreateDragImage *msg);
void zune_list_draw_sort_mark(Object *obj, struct MUI_ListData *data,
    LONG x1, LONG y, LONG h);
/* listx.c too: what only lists of programs that get MUI 4/5 use. The
 * horizontal scroller and the title buttons (MUI 3.9) are made or shown
 * only for those; ISOBJECT cells and MUIA_List_ColumnOrder are MUI 5's. */
BOOL zune_list_column_order(struct MUI_ListData *data, const UBYTE *order);
LONG zune_list_cell_dims(Object *obj, struct MUI_ListData *data,
    struct ListEntry *entry, LONG j);
void zune_list_draw_cell(Object *list, Object *cell, LONG left, LONG top,
    LONG width, LONG height);
void zune_list_layout_horiz(Object *obj, struct MUI_ListData *data,
    LONG left, LONG top, LONG width, LONG height);
void zune_list_check_horiz(Object *obj, struct MUI_ListData *data);
APTR zune_list_draw_title_strip(Object *obj, struct MUI_ListData *data);
/* list.c, for the module: the columns' widths for the area's width */
void zune_list_layout_columns(struct MUI_ListData *data);
LONG zune_list_compare(Object *obj, struct MUI_ListData *data, APTR a, APTR b);
IPTR zune_list_sort_entries(struct IClass *cl, Object *obj, APTR *entries);
void zune_list_set_columns(Object *obj, struct MUI_ListData *data,
    struct TagItem *tag, BOOL init);
void zune_list_format_mui45(struct ColumnInfo *ci, IPTR *args);

#endif
