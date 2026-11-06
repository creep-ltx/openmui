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
    BOOL hidden, sortable, descending;
    int order_id;
    BOOL min_pixels, max_pixels, is_object, layout_hidden;
    LONG elide, layout_width, layout_max;
};

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
    int title_height;           /* The complete height of the title */
    STRPTR title;               /* On single column lists this is the title,
                                 * otherwise 1. NULL for no title(s) */

    /* Cursor images */
    struct MUI_ImageSpec_intern *list_cursor;
    struct MUI_ImageSpec_intern *list_select;
    struct MUI_ImageSpec_intern *list_selcur;

    /* Render optimization */
    int update; /* 1 - update everything, 2 - redraw entry at update_pos,
                 * 3 - scroll to current entries_first (old value is in
                 * update_pos) */
    int update_pos;

    LONG drop_mark;
    LONG drop_mark_y;

    /* list images */
    struct MinList images;

    /* user prefs */
    ListviewRefresh prefs_refresh;
    UWORD prefs_linespacing;
    BOOL prefs_smoothed;
    UWORD prefs_smoothval;

    /* render space handling */
    Object *area;
    BOOL area_replaced;
    BOOL area_connected;

    /***************************/
    /* Former Listview members */
    /***************************/

    Object *vert;
    BOOL vert_connected;

    /* MUI 3.9: a horizontal scroller below the framed entry area when the
     * columns are wider than it; the entries scroll by horiz_first
     * pixels */
    Object *horiz;
    BOOL horiz_shown;
    LONG horiz_first;
    struct Hook layout_hook;
    IPTR scroller_pos;
    BOOL read_only;
    IPTR multiselect;
    LONG drag_type;

    /* clicked column */
    LONG click_column;
    LONG def_click_column;

    LONG mouse_click;            /* see below if mouse is held down */
    LONG mouse_x;
    LONG mouse_y;

    /* double click */
    ULONG last_secs;
    ULONG last_mics;
    ULONG last_active;

    struct MUI_EventHandlerNode ehn;

    /* user prefs */
    ListviewMulti prefs_multi;

    BOOL doubleclick;
    LONG seltype;

    struct Hook hook;
    LONG hscroll_visibility, title_click, sort_column;
    BOOL stripes, editable;
    Object *edit_object, *edit_focus;
    LONG edit_row, edit_column;
    APTR edit_entry;
    BOOL ending_edit;
    LONG max_columns;
    CONST_STRPTR *title_array;
    LONG title_columns;
    STRPTR *mapped_strings;
    BOOL format_layout, max_columns_explicit;
    LONG columns_reserved;      /* slots in strings[] and preparses[] */
    LONG columns_used;          /* highest column number the format shows + 1 */
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

#endif
