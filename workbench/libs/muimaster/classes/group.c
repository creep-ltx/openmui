/*
    Copyright (C) 2002-2026, The AROS Development Team.
    Copyright (C) 1999, David Le Corfec.
    All rights reserved.

    Rewritten for the 68000 in 2026 (OpenMUI, round rw-group,
    docs/lean/README.md "Rewrite: Group"): one layout and one minmax
    function for both directions (the axis an index), the children walked
    by their nodes, the dispatcher a sorted table, the rarely used state
    (title group, grid, virtual area) in a block of its own, the gaps of a
    group painted with one pen when its background is one.
*/

#include <exec/types.h>
#include <stddef.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>

extern struct Library *MUIMasterBase;

#include "muimaster_intern.h"
#include <string.h>
#include "mui.h"
#include "support.h"
#include "prefs.h"

/*  #define MYDEBUG 1 */
#include "debug.h"

/* A child that takes no place in the layout: not shown, a window border
 * gadget, or one that disappeared (MUIA_*Disappear) */
#define IS_HIDDEN(obj) ((_flags(obj) & (MADF_SHOWME | MADF_BORDERGADGET \
    | MADF_DISAPPEARED)) != MADF_SHOWME)

/* Every child in the list's order. The node's successor is read before the
 * body runs, so the body may take its child out of the list (as
 * NextObject() allows); no call per child, the node in a register. */
#define FOR_CHILD(list, n, child) \
    for (n = (list)->mlh_Head; n->mln_Succ \
        && ((child = BASEOBJECT(n)), (n = n->mln_Succ), 1); )

/* Private attribute/method definitions */
#define MUIM_Group_Insert      (MUIB_MUI|0x00424d34)    /* MUI: V20 */
struct MUIP_Group_Insert
{
    STACKED ULONG MethodID;
    STACKED Object *obj;
    STACKED Object *pred;
};

/* Private MUI virtual-group painting protocol. These method IDs are used
 * by existing subclasses to prepare and release their content clipping.
 * MUI 3.9 calls neither method for an ordinary Group or Area. */
#define MUIM_Virtgroup_BeginPaint 0x80425b5cUL
#define MUIM_Virtgroup_EndPaint   0x8042da52UL

struct layout2d_elem
{
    WORD min;
    WORD max;
    WORD dim;
    ULONG weight;
};

/* A virtual group's state (MUIA_Group_Virtual at OM_NEW) */
struct GroupVirtual
{
    struct MUI_EventHandlerNode ehn;
    LONG virt_offx, virt_offy;  /* diplay offsets */
    LONG old_virt_offx, old_virt_offy;  /* Saved virtual positions,
                                         * used for a scroll update */
    LONG virt_mwidth, virt_mheight;     /* The complete width */
    LONG told_mwidth, told_mheight;     /* Size last notified */
    LONG saved_minwidth, saved_minheight;
    APTR paint_clip;            /* previous clipping during virtual painting */
};

/* What few groups need, in a block of their own (NULL for the others,
 * nearly all): a title group (MUI 4's tabs), a grid (Columns, Rows) and
 * a virtual area. A virtual group's block holds its GroupVirtual too
 * (made at OM_NEW); the others end before it. */
struct GroupExtra
{
    Object *titlegroup;
    UWORD columns, rows;        /* 1 and 1: no grid */
    UWORD row_cap, col_cap;     /* entries the arrays have room for */
    struct layout2d_elem *row_infos, *col_infos;
    ULONG horiz_weight_sum, vert_weight_sum;    /* the grid's */
    struct GroupVirtual virt;
};
#define GROUP_EXTRA_SMALL offsetof(struct GroupExtra, virt)

/* Every group, list, listview and scrollbar carries this: 36 bytes (68
 * before the 68000 rewrite, MUI 3.8's 42). The fields read on every
 * child walk and every draw sit on longword boundaries of the object
 * (Group's data starts 2 bytes past one: hence active_page first), and
 * the size keeps a subclass's data (List's) on the boundaries it had.
 * Counts, spacings and sizes are WORDs (MUI's sizes are, MUI_MAXMAX is
 * 10000). horiz_spacing and vert_spacing are indexed by the axis. */
struct MUI_GroupData
{
    WORD active_page;
    /* The children. MUI keeps them in the group itself (no Family
     * object); the Family methods below work on the list as Family's
     * did (family.c). */
    struct MinList childlist;
    struct Hook *layout_hook;
    ULONG flags;                /* GROUP_* (a UWORD costs stack in the
                                 * recursive methods) */
    ULONG weight_sum;           /* a horiz/vert group's, along its axis */
    struct GroupExtra *x;       /* NULL unless titled, a grid or virtual */
    WORD horiz_spacing;         /* -1 until setup, unless set */
    WORD vert_spacing;
    UWORD num_children;
};

/* The fields of an Area's data by axis (0 horizontal, 1 vertical), as
 * WORD offsets from its minimum width: one pointer per child reaches its
 * minimum, maximum, default, size, weight and disappear level along
 * either axis */
#define AX_FIELD(f) ((offsetof(struct MUI_AreaData, f) \
    - offsetof(struct MUI_AreaData, mad_MinMax.MinWidth)) / sizeof(WORD))
#define AX_MIN 0
#define AX_MAX AX_FIELD(mad_MinMax.MaxWidth)
#define AX_DEF AX_FIELD(mad_MinMax.DefWidth)
#define AX_SIZE AX_FIELD(mad_Box.Width)
#define AX_WEIGHT AX_FIELD(mad_HorizWeight)
#define AX_DISAPPEAR AX_FIELD(mad_HorizDisappear)
_Static_assert(offsetof(struct MUI_MinMax, MinHeight)
    == offsetof(struct MUI_MinMax, MinWidth) + 2
    && offsetof(struct MUI_MinMax, MaxHeight)
    == offsetof(struct MUI_MinMax, MaxWidth) + 2
    && offsetof(struct MUI_MinMax, DefHeight)
    == offsetof(struct MUI_MinMax, DefWidth) + 2
    && offsetof(struct IBox, Height) == offsetof(struct IBox, Width) + 2
    && offsetof(struct MUI_AreaData, mad_VertWeight)
    == offsetof(struct MUI_AreaData, mad_HorizWeight) + 2
    && offsetof(struct MUI_AreaData, mad_VertDisappear)
    == offsetof(struct MUI_AreaData, mad_HorizDisappear) + 2
    && offsetof(struct MUI_GroupData, vert_spacing)
    == offsetof(struct MUI_GroupData, horiz_spacing) + 2,
    "the axis fields follow each other");
/* The child's fields, from AX_MIN on, along axis a */
#define AXP(child, a) (&_minwidth(child) + (a))

/* Note:
 * The MUI4 feature of group with tabs is implemented based on behaviour of
 * one application. What this application codes suggest it seems that passing
 * MUIV_Frame_Register together with MUIA_Group_PageMode, TRUE activates this
 * mode.
 * In such mode, the first passed group is used to register tab "titles" and
 * is always visible. The selection of object in this group selects the
 * matching (by position) group to be displayed
 */

#define GROUP_HORIZ       (1<<1)
#define GROUP_SAME_WIDTH  (1<<2)    /* << 1: SAME_HEIGHT (by axis) */
#define GROUP_SAME_HEIGHT (1<<3)
#define GROUP_CHANGING    (1<<4)
#define GROUP_PAGEMODE    (1<<5)
#define GROUP_VIRTUAL     (1<<6)
#define GROUP_HSPACING    (1<<7)    /* << 1: VSPACING (by axis) */
#define GROUP_VSPACING    (1<<8)
#define GROUP_CHANGED     (1<<9)
#define GROUP_PAINTING    (1<<10)
#define GROUP_UPDATE_PAGE (1<<11)   /* MUI_Redraw(): a page switch, no frame */
#define GROUP_UPDATE_SCROLL (1<<12) /* MUI_Redraw(): the virtual offsets moved */
#define GROUP_NOFWD_GET   (1<<13)   /* OM_GET is not passed to the children */
#define GROUP_NOFWD       (1<<14)   /* methods are not passed to the children */
#define GROUP_XFULL       (1<<15)   /* x has room for the GroupVirtual */
#define GROUP_UPDATE      (GROUP_UPDATE_PAGE | GROUP_UPDATE_SCROLL)

#define GROUP_SET_UPDATE(data, u) \
    ((data)->flags = ((data)->flags & ~GROUP_UPDATE) | (u))

static const struct GroupVirtual group_novirt;
/* A virtual group's state, zeroes for the others */
#define GV(data) (((data)->flags & GROUP_VIRTUAL) ? &(data)->x->virt \
    : (struct GroupVirtual *)&group_novirt)
/* Only for a virtual group */
#define VX(data) (&(data)->x->virt)

/* The instance data as a pointer of its own: with its fields at small
 * offsets GCC otherwise keeps the instance offset in a register for
 * (d8,An,Xn) addressing (List's round: a frame +4) */
static inline struct MUI_GroupData *GDATA_(APTR p)
{
    __asm__("" : "+a"(p));
    return p;
}
#define GDATA(cl, obj) GDATA_(INST_DATA(cl, obj))

static inline __attribute__((always_inline)) Object *group_title(struct MUI_GroupData *data)
{
    return data->x ? data->x->titlegroup : NULL;
}

/* A grid: Columns or Rows given (and not resolved to one cell) */
static inline __attribute__((always_inline)) BOOL group_grid(struct MUI_GroupData *data)
{
    struct GroupExtra *x = data->x;

    return x && (x->columns != 1 || x->rows != 1);
}

/* The group's extra block, made on first need (with room for the
 * virtual state once the group is virtual); NULL without memory */
static __attribute__((noinline)) struct GroupExtra *group_extra(
    struct MUI_GroupData *data)
{
    struct GroupExtra *x = data->x;
    ULONG full = data->flags & GROUP_VIRTUAL;

    if (x && (!full || (data->flags & GROUP_XFULL)))
        return x;
    if (!(x = AllocVec(full ? sizeof(*x) : GROUP_EXTRA_SMALL, MEMF_CLEAR)))
        return NULL;
    if (data->x)
    {
        CopyMem(data->x, x, GROUP_EXTRA_SMALL);
        FreeVec(data->x);
    }
    else
        x->columns = x->rows = 1;
    if (full)
        data->flags |= GROUP_XFULL;
    return data->x = x;
}

/* *infos with room for n entries, cleared; NULL without memory */
static struct layout2d_elem *group_infos(struct layout2d_elem **infos,
    UWORD *cap, ULONG n)
{
    if (*infos && *cap >= n)
    {
        memset(*infos, 0, n * sizeof(**infos));
        return *infos;
    }
    if (*infos)
        FreeVec(*infos);
    *infos = AllocVec(n * sizeof(**infos), MEMF_CLEAR);
    *cap = *infos ? n : 0;
    return *infos;
}

static inline struct MinList *group_children(struct MUI_GroupData *data)
{
    struct MinList *list = &data->childlist;

    /* The list is taken as a pointer of its own, as when the group kept
     * one to it: GCC otherwise keeps both the data and the list live in
     * the child walks, more bytes of stack per nesting level */
    __asm__("" : "+a"(list));
    return list;
}

/****** Group.mui/MUIC_Group ************
*
*   NAME
*       MUIC_Group
*
*   SUPERCLASS
*       Area.mui
*
*   DESCRIPTION
*       Own an ordered set of Area-compatible children in a list of its own,
*       as MUI does (no Family object). Arrange them horizontally,
*       vertically, in a grid, as pages or through a layout hook. Default orientation is vertical, with both stored
*       row and column counts initially one. Layout requires the connected
*       application's preferences and valid child min/max information.
*       Group owns its child list, its attached children and private grid
*       arrays. It borrows the layout Hook. Construction returns NULL on
*       superclass failure or any explicit NULL Group_Child tag.
*       Disposal frees grid arrays and disposes the remaining children, then
*       Area. Hide and Cleanup must already have ended active
*       display resources and handlers.
*       Only attach live compatible objects that are not already owned or linked
*       elsewhere. Adding adopts a child; removing unlinks without disposing it.
*       Serialize membership, layout and callbacks in the application task. The
*       implementation does not protect traversal against children destroying
*       themselves or changing the list.
*
*   METHODS
*       Setup calls Area then sets up every child, including children with
*       ShowMe FALSE. On child failure it cleans only the previously successful
*       children and returns FALSE; it does not clean the failing child or the
*       successful Area setup itself. Cleanup calls every child then Area.
*       Show/Hide select the active page or eligible visible children; child
*       results are ignored. Draw returns FALSE while InitChange is active.
*       AskMinMax asks children with ShowMe set, or every child when a layout
*       hook is set, and finalizes their sizes, then uses page layout, a custom
*       hook or built-in layout in that priority order. It returns zero
*       regardless of results. Layout uses the same priority and returns the
*       hook's result for a custom hook, otherwise TRUE. The library MUI_Layout
*       sets a child's box and returns FALSE when that box is outside the
*       child's min/max limits, otherwise the child's layout result.
*       FindUData/GetUData match Group first, then ask the children in list
*       order. GetUData reports a match without checking getter success.
*       SetUData may set Group, whose setter can already broadcast, and then
*       visits every child, so assignments can repeat; SetUDataOnce stops at the first match. Export/Import visit
*       children only and return zero; they do not call Area's own
*       Selected-state serialization.
*       OM_GET tries Group, then Area/Notify, then children until one getter
*       succeeds. Unknown methods normally go to every child before Area; child
*       results are discarded. Explicit lifecycle/utility/drag cases have
*       different routes. DoMethodNoForward controls only the paths that inspect
*       its forwarding flag, not every explicit child traversal.
*
*   NOTES
*       MUIM_Notify first probes the group's trigger getter with child-get
*       forwarding disabled. If recognized locally, it registers through the
*       superclass. Otherwise it probes each child and registers on those
*       reporting support. This is a Group override of Notify, so an event-only
*       attribute without a getter is not automatically registered locally.
*       Probe storage is a fixed 30-IPTR stack array; getters must not write
*       beyond it. Return TRUE on the child path does not prove that any
*       registration succeeded.
*       Family_GetChild walks the group's own list as Family's does. Group also
*       implements virtual offset/extent attributes used by Virtgroup. The
*       locally defined Group_Insert is an internal source interface; the
*       header's Group_ChildCount getter returns the stored child count, and
*       its ExitChange2 is explicitly private.
*
*   BUGS
*       Add/remove helpers ignore child results and adjust a stored
*       count without validating membership. Setup has incomplete failure
*       unwinding. Page indices and grid dimensions are not generally
*       range-checked. Several layout intermediates are WORD values, and
*       allocation/arithmetic failures have no comprehensive status. Treat
*       documented results as implementation results, not stronger success
*       guarantees.
*
*/
/****** Group.mui/MUIA_Group_Child ************
*
*   NAME
*       MUIA_Group_Child
*
*   FUNCTION
*       Append a non-NULL Area-compatible child through OM_ADDMEMBER. The
*       group owns attached children.
*
*   INPUTS
*       Object pointer; initialize only; repeatable.
*
*   NOTES
*       Runtime changes use AddHead/AddTail or OM_ADDMEMBER and Remove or
*       OM_REMMEMBER. Any explicit NULL Child tag makes construction fail after
*       tag processing and disposes attached children. Earlier superclass
*       allocation failure can occur before child adoption. Do not dispose an
*       attached child separately.
*
*   BUGS
*       When the frame request is MUIV_Frame_Register, the first Child tag also
*       stores the internal titlegroup pointer, even without PageMode.
*
*/
/****** Group.mui/MUIA_Group_ChildList ************
*
*   NAME
*       MUIA_Group_ChildList
*
*   FUNCTION
*       Return the borrowed child list owned by the group.
*
*   INPUTS
*       Exec List pointer; get only.
*
*   NOTES
*       Use NextObject for traversal. Do not edit links, free the list or retain
*       it beyond Group disposal. Membership must remain stable during traversal
*       and callbacks. This is not a copied snapshot.
*
*/
/****** Group.mui/MUIA_Group_Horiz ************
*
*   NAME
*       MUIA_Group_Horiz
*
*   FUNCTION
*       Select horizontal one-dimensional layout when true; default FALSE
*       selects vertical.
*
*   INPUTS
*       BOOL; initialize; getter also implemented.
*
*   NOTES
*       This orientation is used when rows and columns both equal one and
*       neither PageMode nor LayoutHook takes precedence. There is no setter.
*       The getter returns the GROUP_HORIZ bit value (2 when set), not
*       normalized TRUE (1). Test it as nonzero.
*
*/
/****** Group.mui/MUIA_Group_PageMode ************
*
*   NAME
*       MUIA_Group_PageMode
*
*   FUNCTION
*       Enable page-group layout, default FALSE. One content child is shown at a
*       time; pages with ShowMe set contribute size constraints and all pages
*       are laid out.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       PageMode takes precedence over LayoutHook, Rows/Columns and Horiz. With
*       MUIV_Frame_Register, the first Child tag is treated as a title group and
*       excluded from content-page counting. A Title object added as the first
*       child also becomes the title group and enables PageMode. The title
*       group's minimum height is reserved above the content, or below it when
*       its MUIA_Title_Position is MUIV_Title_Position_Bottom; page changes set
*       its Group_ActivePage. Group does not create tab objects or install
*       title-selection notifications itself.
*
*   BUGS
*       Show/Hide count all children except the title group, while Draw skips
*       ShowMe-FALSE children before incrementing the page count. Hidden pages
*       can therefore make these paths select different indices. There is no
*       PageMode setter or local getter.
*
*/
/****** Group.mui/MUIA_Group_ActivePage ************
*
*   NAME
*       MUIA_Group_ActivePage
*
*   FUNCTION
*       Choose a zero-based content-page index when PageMode is enabled. Default
*       zero. First selects zero, Last selects count minus one, Prev wraps from
*       zero, and Next/Advance both advance with modulo wrap. The title group,
*       if any, is excluded from that count.
*
*   INPUTS
*       LONG; initialize, set, get.
*
*   NOTES
*       A changed live page hides the previous selection, stores the new index,
*       lays out and shows the new selection, then redraws content. Before
*       PageMode is enabled this operation does nothing; initialization tag
*       order matters. Configure PageMode and children before using relative
*       selectors. Without content pages any selection stores zero. The getter
*       returns the stored index.
*
*   BUGS
*       Explicit indices are not clamped. Removing children does not repair an
*       out-of-range active index. See PageMode for hidden-child counting
*       differences.
*
*/
/****** Group.mui/MUIA_Group_Columns ************
*
*   NAME
*       MUIA_Group_Columns
*
*   FUNCTION
*       Request a grid columns count, converted to at least one, and clear the
*       other dimension to zero so min/max calculation derives it from the total
*       child count.
*
*   INPUTS
*       LONG declaration, stored as ULONG; initialize and set.
*
*   NOTES
*       The last Rows/Columns tag wins. On min/max calculation, a nondivisible
*       request falls back to one row containing all children. Dimensions are
*       derived from total membership, while most cell passes skip hidden and
*       border-gadget children. A runtime setter requests Window recalculation
*       when RenderInfo exists. There is no local getter.
*
*   BUGS
*       Negative or oversized values are not rejected before unsigned
*       conversion/allocation arithmetic. There is no partial-last-row layout
*       guarantee. Grid-array allocation failure returns from an internal helper
*       without a public failure result; later Layout can do nothing.
*
*/
/****** Group.mui/MUIA_Group_Rows ************
*
*   NAME
*       MUIA_Group_Rows
*
*   FUNCTION
*       Request a grid rows count, converted to at least one, and clear the
*       other dimension to zero so min/max calculation derives it from the total
*       child count.
*
*   INPUTS
*       LONG declaration, stored as ULONG; initialize and set.
*
*   NOTES
*       The last Rows/Columns tag wins. On min/max calculation, a nondivisible
*       request falls back to one column containing all children. Dimensions are
*       derived from total membership, while most cell passes skip hidden and
*       border-gadget children. A runtime setter requests Window recalculation
*       when RenderInfo exists. There is no local getter.
*
*   BUGS
*       Negative or oversized values are not rejected before unsigned
*       conversion/allocation arithmetic. There is no partial-last-row layout
*       guarantee. Grid-array allocation failure returns from an internal helper
*       without a public failure result; later Layout can do nothing.
*
*/
/****** Group.mui/MUIA_Group_HorizSpacing ************
*
*   NAME
*       MUIA_Group_HorizSpacing
*
*   FUNCTION
*       Store the horizontal gap between children or grid cells. Before setup
*       the unspecified value is stored as ULONG -1; setup replaces it with the
*       corresponding application preference, capped at 2 from the third
*       fallback stage of a window set up again to fit the screen.
*
*   INPUTS
*       LONG declaration, stored as ULONG; initialize, set, get.
*
*   NOTES
*       Explicit initialization or setting marks the value as caller-specified
*       so setup no longer supplies that default. The getter returns the stored
*       value. A spacing setter alone does not request relayout; finish a
*       controlled change with ExitChange when live.
*
*   BUGS
*       No nonnegative/range validation is performed. Negative or very large
*       values can wrap in unsigned/WORD layout arithmetic. Do not interpret -1
*       as a documented runtime reset-to-default operation.
*
*/
/****** Group.mui/MUIA_Group_VertSpacing ************
*
*   NAME
*       MUIA_Group_VertSpacing
*
*   FUNCTION
*       Store the vertical gap between children or grid cells. Before setup the
*       unspecified value is stored as ULONG -1; setup replaces it with the
*       corresponding application preference, capped at 2 from the third
*       fallback stage of a window set up again to fit the screen.
*
*   INPUTS
*       LONG declaration, stored as ULONG; initialize, set, get.
*
*   NOTES
*       Explicit initialization or setting marks the value as caller-specified
*       so setup no longer supplies that default. The getter returns the stored
*       value. A spacing setter alone does not request relayout; finish a
*       controlled change with ExitChange when live.
*
*   BUGS
*       No nonnegative/range validation is performed. Negative or very large
*       values can wrap in unsigned/WORD layout arithmetic. Do not interpret -1
*       as a documented runtime reset-to-default operation.
*
*/
/****** Group.mui/MUIA_Group_Spacing ************
*
*   NAME
*       MUIA_Group_Spacing
*
*   FUNCTION
*       Set both horizontal and vertical gaps and mark both as explicit.
*
*   INPUTS
*       LONG declaration, stored as ULONG; initialize and set.
*
*   NOTES
*       Tag order determines the result when combined with the axis-specific
*       tags. There is no local combined getter or automatic relayout. Use axis
*       getters to inspect the stored values; spacing has the same unchecked
*       conversion limitations as those attributes.
*
*/
/****** Group.mui/MUIA_Group_SameWidth ************
*
*   NAME
*       MUIA_Group_SameWidth
*
*   FUNCTION
*       Enable same-width constraints in built-in min/max/layout calculations.
*       Default FALSE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       The algorithms use the largest participating minimum and each child's
*       own limits; this is not an unconditional guarantee of identical final
*       dimensions. PageMode and custom hooks have their own behavior. There is
*       no runtime setter or local getter. SameSize sets or clears both flags;
*       later specific initialization tags can override one.
*
*/
/****** Group.mui/MUIA_Group_SameHeight ************
*
*   NAME
*       MUIA_Group_SameHeight
*
*   FUNCTION
*       Enable same-height constraints in built-in min/max/layout calculations.
*       Default FALSE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       The algorithms use the largest participating minimum and each child's
*       own limits; this is not an unconditional guarantee of identical final
*       dimensions. PageMode and custom hooks have their own behavior. There is
*       no runtime setter or local getter. SameSize sets or clears both flags;
*       later specific initialization tags can override one.
*
*/
/****** Group.mui/MUIA_Group_SameSize ************
*
*   NAME
*       MUIA_Group_SameSize
*
*   FUNCTION
*       Enable both same-width and same-height constraints in built-in
*       min/max/layout calculations. Default FALSE.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       The algorithms use the largest participating minimum and each child's
*       own limits; this is not an unconditional guarantee of identical final
*       dimensions. PageMode and custom hooks have their own behavior. There is
*       no runtime setter or local getter. SameSize sets or clears both flags;
*       later specific initialization tags can override one.
*
*/
/****** Group.mui/MUIA_Group_LayoutHook ************
*
*   NAME
*       MUIA_Group_LayoutHook
*
*   FUNCTION
*       Borrow a custom layout hook used when PageMode is FALSE. NULL selects
*       built-in layout. Keep the Hook and callback code alive until replacement
*       or Group disposal.
*
*   INPUTS
*       Hook pointer; initialize; setter also implemented.
*
*   NOTES
*       CallHookPkt passes this Group as object and a temporary MUI_LayoutMsg as
*       message. lm_Children is the borrowed internal list; keep membership
*       stable. The MUILM_MINMAX return value is ignored and the MUILM_LAYOUT
*       return value becomes the Layout result; returning MUILM_UNKNOWN does
*       not request built-in fallback. There is no local getter and replacing
*       the hook does not itself relayout.
*       For MUILM_MINMAX, the limits of every child, including children with
*       ShowMe FALSE, have already been collected. Fill all six lm_MinMax
*       fields with content requirements excluding this group's frame/spacing.
*       This stack message is not initialized for you. Group raises maxima and
*       defaults below minima, adds the values to its Area contribution and
*       caps the maxima at MUI_MAXMAX. It does not clamp a too-large hook
*       default back to the maximum.
*       For MUILM_LAYOUT, the message starts cleared; lm_Layout.Width/Height
*       contain available inner dimensions. Lay out children with MUI_Layout
*       using coordinates relative to the group inner origin. For virtual
*       groups, write the resulting virtual content Width/Height back to those
*       same fields. The private fields have no public contract. Do not retain
*       the message or list as owned storage.
*
*   BUGS
*       A hook that omits a min/max field leaves an uninitialized value in use.
*       MUILM_MINMAX failure is not propagated; Group AskMinMax returns zero
*       regardless. Group itself does not enforce the custom layout's child size
*       constraints.
*
*/
/****** Group.mui/MUIA_Group_Forward ************
*
*   NAME
*       MUIA_Group_Forward
*
*   FUNCTION
*       Control whether this OM_SET forwards its filtered tag list to children.
*       Default TRUE for each call; the last Forward tag controls that call.
*       FALSE still applies group/superclass changes.
*
*   INPUTS
*       BOOL; set only, per setter call.
*
*   NOTES
*       The flag is not stored and has no local getter. Suppression applies to
*       this setter's ordinary child broadcast, not to explicit child effects of
*       page switching or other operations. The setter mutates the caller's
*       writable tags to TAG_IGNORE for filtered attributes and does not restore
*       them.
*       The filter removes HelpLine, HelpNode, ObjectID, UserData, ContextMenu,
*       ContextMenuTrigger, ControlChar, CustomBackfill, CycleChain, Draggable,
*       FillArea, Group_ActivePage, Group_LayoutHook, Frame, FrameTitle,
*       HorizWeight, Pressed, ShortHelp, ShowMe, VertWeight, Weight,
*       Virtgroup_Left/Top, AppMessage and Timer.
*       Selected is not explicitly filtered, but Area may already suppress an
*       unchanged Selected tag. Unfiltered group tags such as spacing can also
*       reach child groups.
*       Disabled is delayed: child forwarding prepends a NestedDisabled
*       operation and also passes Disabled, then the superclass applies the
*       group's Disabled tag. Repeated TRUE calls can accumulate child nesting
*       even though Disabled itself is Boolean. Forward FALSE omits that child
*       propagation.
*
*   BUGS
*       Multiple Disabled tags are initially changed to TAG_IGNORE, but only the
*       last one is restored and applied as the group's delayed value. Child
*       setters can mutate the shared tag list, affecting later children. There
*       is no tag-list copy per child.
*
*/
/****** Group.mui/MUIA_Group_Virtual ************
*
*   NAME
*       MUIA_Group_Virtual
*
*   FUNCTION
*       Enable virtual-content layout, clipping and edge autoscroll; default
*       FALSE. The constructor marks the object as a virtual group and
*       connection marks descendants as inside a virtual group.
*
*   INPUTS
*       BOOL; initialize only.
*
*   NOTES
*       AskMinMax saves full content minima but exposes a viewport minimum of
*       Area's contribution plus eight pixels per dimension. Layout computes
*       virtual extents and clamps offsets. Group implements Virtgroup_Left/Top
*       setters and getters plus Width/Height/MinWidth/MinHeight getters for
*       this machinery. It does not create scrollbar objects.
*       A click inside registers tick events; moving outside while tracking
*       scrolls four pixels per tick, clamped to extents. A later non-SELECTDOWN
*       button event stops tick tracking. There is no runtime Virtual setter or
*       local getter.
*
*   BUGS
*       Virtual offset setters clamp to the current extents, so offsets set
*       before the first layout become zero. On a nonvirtual group the offsets
*       are stored unclamped and shift its children until the next layout.
*
*/
/****** Group.mui/MUIM_Group_AddTail ************
*
*   NAME
*       MUIM_Group_AddTail
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Group_AddTail, (IPTR)child);
*
*   FUNCTION
*       Insert a child at the end of the child list and increase the stored
*       count. Set its parent to this Group, connect it when application
*       information exists, and set it up immediately when the Group is set up.
*
*   INPUTS
*       child - Non-NULL live Area-compatible object, not already owned or
*       linked elsewhere. Ownership transfers through attachment.
*
*   RESULT
*       Always TRUE; child connection and child Setup results are
*       ignored.
*
*   NOTES
*       For live structural changes, bracket the operations with
*       InitChange/ExitChange. Addition alone does not show or lay out the
*       child. If the group is disabled, the added child receives a no-notify
*       NestedDisabled increment and Disabled=TRUE. OM_ADDMEMBER uses the
*       AddTail path.
*
*   BUGS
*       No NULL/membership validation is provided by the Group helper; it
*       dereferences child even for a NULL child. A reported TRUE
*       does not establish successful setup. Subsequent removal does not undo
*       inherited disabled state.
*
*/
/****** Group.mui/MUIM_Group_AddHead ************
*
*   NAME
*       MUIM_Group_AddHead
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Group_AddHead, (IPTR)child);
*
*   FUNCTION
*       Insert a child at the beginning of the child list and increase the
*       stored count. Set its parent to this Group, connect it when application
*       information exists, and set it up immediately when the Group is set up.
*
*   INPUTS
*       child - Non-NULL live Area-compatible object, not already owned or
*       linked elsewhere. Ownership transfers through attachment.
*
*   RESULT
*       Always TRUE; child connection and child Setup results are
*       ignored.
*
*   NOTES
*       For live structural changes, bracket the operations with
*       InitChange/ExitChange. Addition alone does not show or lay out the
*       child. If the group is disabled, the added child receives a no-notify
*       NestedDisabled increment and Disabled=TRUE. OM_ADDMEMBER uses the
*       AddTail path.
*
*   BUGS
*       No NULL/membership validation is provided by the Group helper; it
*       dereferences child even for a NULL child. A reported TRUE
*       does not establish successful setup. Subsequent removal does not undo
*       inherited disabled state.
*
*/
/****** Group.mui/MUIM_Group_Remove ************
*
*   NAME
*       MUIM_Group_Remove
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Group_Remove, (IPTR)child);
*
*   FUNCTION
*       Hide the child when the Group is drawable, clean it when the Group is
*       set up, disconnect it, decrement the stored count and remove it from
*       the child list.
*
*   INPUTS
*       child - Non-NULL live child currently attached to this Group.
*
*   RESULT
*       Always TRUE; child results are ignored.
*
*   NOTES
*       The child is not disposed; the caller resumes responsibility for its
*       lifetime. OM_REMMEMBER uses this path. Use InitChange/ExitChange for
*       live removal. As with Zune's former Family object, the removal also
*       disconnects the child, so the connected path can send
*       DisconnectParent twice. The final removal clears the parent pointer.
*
*   BUGS
*       No membership/count validation occurs. Passing an unrelated child can
*       corrupt ownership/counts. The active page is not repaired. Removal
*       leaves any disabled state previously imposed on the child intact.
*
*/
/****** Group.mui/MUIM_Group_InitChange ************
*
*   NAME
*       MUIM_Group_InitChange
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Group_InitChange);
*
*   FUNCTION
*       Clear the changed marker and set the changing flag. Drawing returns
*       FALSE while that flag is set.
*
*   INPUTS
*       No arguments beyond MethodID. Keep the group and application alive
*       throughout the change sequence.
*
*   RESULT
*       Always TRUE.
*
*   NOTES
*       Pair with ExitChange around live add/remove or layout changes. This
*       begins neither a lock nor a rollback transaction. It does not hide
*       children or defer their connection/setup/cleanup.
*
*   BUGS
*       There is one flag, not a nesting counter. Calling InitChange twice on
*       the same object does not require two exits and clears the prior changed
*       marker.
*
*/
/****** Group.mui/MUIM_Group_ExitChange ************
*
*   NAME
*       MUIM_Group_ExitChange
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Group_ExitChange);
*
*   FUNCTION
*       Clear changing/changed flags and request Window recalculation when set
*       up, unless an ancestor group is still changing.
*
*   INPUTS
*       No arguments beyond MethodID. Requires a valid connected Group ancestor
*       chain for live recalculation.
*
*   RESULT
*       Always TRUE, including when recalculation is skipped or its result
*       ignored.
*
*   NOTES
*       Recalculation is attempted even without a recorded structural change;
*       the changed-only guard is disabled. An outer group's later exit is
*       responsible for updating a nested change. This is not a commit-success
*       report or a restoration of failed operations.
*
*/
/****** Group.mui/MUIM_Group_Sort ************
*
*   NAME
*       MUIM_Group_Sort
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Group_Sort, (IPTR)first, ... ,
*       (IPTR)NULL);
*
*   FUNCTION
*       Replace the child order with the supplied NULL-terminated
*       sequence, as Family_Sort does.
*
*   INPUTS
*       first, ... - Every currently attached child exactly once in the desired
*       order, followed by NULL. A retained method message must be writable.
*
*   RESULT
*       Always TRUE.
*
*   NOTES
*       This is explicit ordering, not a comparison callback sort. It does not
*       update the stored child count, reconnect children, change active
*       page/titlegroup, mark a change or relayout. Use InitChange/ExitChange
*       for a live reorder.
*
*   BUGS
*       The implementation resets the list without validating completeness,
*       uniqueness or membership. Omitting a child detaches its list node
*       without cleanup/disposal/count repair; duplicates or foreign objects can
*       corrupt the list.
*
*/
/****** Group.mui/MUIM_Group_DoMethodNoForward ************
*
*   NAME
*       MUIM_Group_DoMethodNoForward
*
*   SYNOPSIS
*       IPTR result = DoMethod(obj, MUIM_Group_DoMethodNoForward, methodid,
*       ...);
*
*   FUNCTION
*       Set the private no-forward flag, dispatch the embedded method on this
*       object and clear the flag before returning.
*
*   INPUTS
*       methodid - ULONG MethodID beginning the embedded message.
*       ... - Its correctly laid-out argument fields. Keep the Group alive
*       through the call.
*
*   RESULT
*       The embedded method's result.
*
*   NOTES
*       This suppresses Group_DispatchMsg broadcasts and OM_GET fallback to
*       children. It does not suppress explicit loops in Setup/Cleanup,
*       Import/Export, user-data searches or the Group Notify override. It is
*       therefore not a universal self-only execution mechanism.
*
*   BUGS
*       The flag is set to one and later reset to zero rather than
*       saved/restored. Nested use can clear an outer suppression early. The
*       implementation passes the address of the declared ULONG DoMethodID field
*       as a message; use the ABI-defined structure layout for direct messages,
*       not an invented IPTR-array layout on every target.
*
*/

#include "native_version.h"

IPTR Group__MUIM_Show(struct IClass *cl, Object *obj,
    struct MUIP_Show *msg);
IPTR Group__MUIM_Hide(struct IClass *cl, Object *obj,
    struct MUIP_Hide *msg);
static ULONG Group_DispatchMsg(struct IClass *cl, Object *obj, Msg msg);
extern const struct __MUIBuiltinClass _MUI_Group_desc;

/* Family_Reorder: move the NULL-terminated array behind after (NULL: to
 * the front, -1: to the end), links only. FALSE for a stranger or a
 * duplicate. */
static BOOL group_contains(struct MinList *list, Object *obj)
{
    struct MinNode *n;
    Object *child;

    FOR_CHILD(list, n, child)
        if (child == obj) return TRUE;
    return FALSE;
}

static IPTR group_reorder(struct MUI_GroupData *data, Object *after,
    Object **array)
{
    struct List *list = (struct List *)&data->childlist;
    struct Node *at;
    Object **p, **q;

    if (after && after != (Object *)-1 &&
        !group_contains(&data->childlist, after)) return FALSE;
    for (p = array; *p; p++)
    {
        if (*p == after || !group_contains(&data->childlist, *p))
            return FALSE;
        for (q = array; q < p; q++)
            if (*p == *q) return FALSE;
    }
    for (p = array; *p; p++)
        Remove((struct Node *)_OBJECT(*p));
    at = after == (Object *)-1 ? list->lh_TailPred :
        after ? (struct Node *)_OBJECT(after) : NULL;
    for (p = array; *p; p++)
    {
        struct Node *node = (struct Node *)_OBJECT(*p);
        Insert(list, node, at);
        /* MUI keeps a fixed insertion point except when appending. */
        if (after == (Object *)-1) at = node;
    }
    return TRUE;
}

/* Whether the page a switch shows already has the box the page layout
 * (group_layout_pagemode) gives it: every layout of a page group lays out
 * all its pages, and a page whose contents change is laid out again by
 * the window's RecalcDisplay, so a page switch in a group whose box did
 * not change has nothing to lay out. MUI 3.8 does not lay out at a switch
 * either (a Cycle set: 2.4 ms there, 2.8 with the layout, 2.4 without,
 * opbench). Only for an object of the Group class itself (a subclass may
 * act on the layout: Register selects its tab there) without a title or a
 * virtual area; any other, or a box that differs, is laid out as before. */
static __attribute__((noinline)) BOOL group_page_laid_out(Object *obj,
    struct MUI_GroupData *data)
{
    struct MinNode *n;
    Object *child = NULL;
    LONG i = data->active_page, w, h;

    if ((data->flags & GROUP_VIRTUAL) || group_title(data)
#if defined(__MAXON__) || defined(__amigaos4__)
        || OCLASS(obj)->cl_Dispatcher.h_Entry
#else
        || OCLASS(obj)->cl_Dispatcher.h_SubEntry
#endif
            != (HOOKFUNC)_MUI_Group_desc.dispatcher)
        return FALSE;
    for (n = data->childlist.mlh_Head; n->mln_Succ; n = n->mln_Succ)
        if (--i < 0)
        {
            child = BASEOBJECT(n);
            break;
        }
    if (!child)
        return FALSE;
    w = MIN(_mwidth(obj), _maxwidth(child));
    h = MIN(_mheight(obj), _maxheight(child));
    return w >= _minwidth(child) && h >= _minheight(child)
        && _width(child) == w && _height(child) == h
        && _left(child) == _mleft(obj) + (_mwidth(obj) - w) / 2
        && _top(child) == _mtop(obj) + (_mheight(obj) - h) / 2;
}

/*
 * Change the stored page only in PageMode. Requires valid Group state and a
 * nonempty content set for Next/Advance modulo arithmetic.
 * Hide/layout/show/redraw the changed live selection and update the
 * borrowed title group. No bounds validation or error result; explicit
 * invalid indices can select no child.
 */
static void change_active_page(struct IClass *cl, Object *obj, LONG page)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    LONG newpage, num_children;

    if (!(data->flags & GROUP_PAGEMODE))
        return;
    num_children = data->num_children - (group_title(data) != NULL);
    if (num_children <= 0)
    {
        data->active_page = 0;
        return;
    }

    switch (page)
    {
    case MUIV_Group_ActivePage_First:
        newpage = 0;
        break;
    case MUIV_Group_ActivePage_Last:
        newpage = num_children - 1;
        break;
    case MUIV_Group_ActivePage_Prev:
        newpage = data->active_page - 1;
        if (newpage == -1)
            newpage = num_children - 1;
        break;
    case MUIV_Group_ActivePage_Next:
    case MUIV_Group_ActivePage_Advance:
        newpage = (data->active_page + 1) % num_children;
        break;
    default:
        newpage = page;
        break;
    }

    if (newpage != data->active_page)
    {
        if (_flags(obj) & MADF_CANDRAW)
            Group__MUIM_Hide(cl, obj, NULL);

        data->active_page = newpage;

        if (_flags(obj) & MADF_CANDRAW)
        {
            if (!group_page_laid_out(obj, data))
                DoLayoutMethod(obj);
            Group__MUIM_Show(cl, obj, NULL);
            GROUP_SET_UPDATE(data, GROUP_UPDATE_PAGE);
            MUI_Redraw(obj, MADF_DRAWUPDATE);
        }

        if (group_title(data))
            set(group_title(data), MUIA_Group_ActivePage, newpage);
    }
}

/* Make child the group's title group (and the group a page group): a
 * Title object added first, or the first child of a group with a Register
 * frame */
static void group_set_title(struct MUI_GroupData *data, Object *child)
{
    struct GroupExtra *x;

    if ((x = group_extra(data)))
        x->titlegroup = child;
}

/*
 * The insertion of AddHead, AddTail, OM_ADDMEMBER and Insert (msg->obj,
 * Insert's pred after it: the methods' messages agree). Requires a valid
 * unowned Area child and a valid predecessor for Insert. Connection and
 * Setup results are ignored; the count increases. Attach parent/virtual
 * state and propagate disabled nesting. Return TRUE, not verified
 * insertion/setup success; a NULL child is ignored.
 */
static IPTR group_add(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    Object *child = ((struct MUIP_Group_Insert *)msg)->obj;
    struct List *list = (struct List *)&data->childlist;

    if (!child)
        return TRUE;
    /* Family's AddHead/AddTail/Insert on the group's own list (rootclass's
     * OM_ADDTAIL is this AddTail, without a dispatch through every class
     * of the child) */
    if (msg->MethodID == MUIM_Group_AddHead)
        AddHead(list, (struct Node *)_OBJECT(child));
    else if (msg->MethodID == MUIM_Group_Insert)
        Insert(list, (struct Node *)_OBJECT(child),
            (struct Node *)_OBJECT(((struct MUIP_Group_Insert *)msg)->pred));
    else
        AddTail(list, (struct Node *)_OBJECT(child));

    if (++data->num_children == 1 &&
        (muiAreaData(child)->mad_Flags2 & MADF2_ISTITLE))
    {
        group_set_title(data, child);
        data->flags |= GROUP_PAGEMODE;
    }

    if (data->flags & GROUP_CHANGING)
        data->flags |= GROUP_CHANGED;

    /* if we are in an application tree, propagate pointers */
    if (muiNotifyData(obj)->mnd_GlobalInfo)
    {
        /* Only children of groups can have parents */
        if ((_flags(obj) & MADF_INVIRTUALGROUP)
            || (data->flags & GROUP_VIRTUAL))
            _flags(child) |= MADF_INVIRTUALGROUP;
        DoMethod(child, MUIM_ConnectParent, (IPTR) obj);
    }

    /* Ensure new children are disabled if their parent is. The group's
     * own state (Area's answer to MUIA_Disabled), not an OM_GET through
     * intuition and every class: 145 GetAttr calls per opbench "app+windows
     * create" (MUI 3.8 makes 15 in all) */
    if (muiAreaData(obj)->mad_DisableCount)
        SetAttrs(child, MUIA_NoNotify, TRUE, MUIA_NestedDisabled, TRUE,
            MUIA_Disabled, TRUE, TAG_DONE);

    /* Some apps (Odyssey) expect _parent() will work before group tree is
     * added to application tree */
    muiNotifyData(child)->mnd_ParentObject = obj;

    if (_flags(obj) & MADF_SETUP)
        DoSetupMethod(child, muiRenderInfo(obj));
    return TRUE;
}

/**************************************************************************
 OM_NEW - Constructor
**************************************************************************/
/*
 * Construct Area, the child list and default Group state; consume
 * initialization tags in order. Adopt Child objects; remember first
 * register-frame child as borrowed titlegroup. Return object or NULL on
 * superclass failure or explicit NULL child. Failure disposal
 * releases already adopted children. No grid arrays are allocated here.
 */
IPTR Group__OM_NEW(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_GroupData *data;
    struct TagItem *tags, *tag;
    BOOL bad = FALSE;

    obj = (Object *) DoSuperMethodA(cl, obj, (Msg) msg);
    if (!obj)
    {
        zune_dispose_children(msg->ops_AttrList);
        return 0;
    }

    muiAreaData(obj)->mad_Flags2 |= MADF2_ISGROUP;

    data = INST_DATA(cl, obj);
    NewList((struct List *)&data->childlist);
    data->horiz_spacing = -1;
    data->vert_spacing = -1;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));)
    {
        IPTR v = tag->ti_Data;

        switch (tag->ti_Tag)
        {
        case MUIA_Group_Child:
            if (v)
            {
                /* Group's own insertion, not OM_ADDMEMBER through the
                 * object's class: MUI 3.8 and MUI 5 do not send the
                 * initial children to a subclass (TabGroup.mcc forwards
                 * OM_ADDMEMBER to a page group that does not exist yet
                 * and lost every child on Zune) */
                /* the tag item {MUIA_Group_Child, child} is a message
                 * for group_add's AddTail path ({method, child}) */
                group_add(cl, obj, (Msg)tag);
                /* The first child of a group with a Register frame is its
                 * title (Area keeps the frame asked for: no OM_GET through
                 * every class of the object) */
                if (muiAreaData(obj)->mad_Frame == MUIV_Frame_Register
                    && !group_title(data))
                    group_set_title(data, (Object *)v);
            }
            else
                bad = TRUE;
            break;

        case MUIA_Group_ActivePage:
            change_active_page(cl, obj, (LONG) v);
            break;

        case MUIA_Group_Columns:
        case MUIA_Group_Rows:
        {
            struct GroupExtra *x = group_extra(data);

            if (x)
            {
                v = MAX((ULONG) v, 1);
                if (tag->ti_Tag == MUIA_Group_Columns)
                    x->columns = v, x->rows = 0;
                else
                    x->rows = v, x->columns = 0;
            }
            break;
        }

        case MUIA_Group_Horiz:
            _handle_bool_tag(data->flags, v, GROUP_HORIZ);
            break;

        case MUIA_Group_HorizSpacing:
            data->flags |= GROUP_HSPACING;
            data->horiz_spacing = v;
            break;

        case MUIA_Group_LayoutHook:
            data->layout_hook = (struct Hook *)v;
            break;

        case MUIA_Group_PageMode:
            _handle_bool_tag(data->flags, v, GROUP_PAGEMODE);
            break;

        case MUIA_Group_SameHeight:
            _handle_bool_tag(data->flags, v, GROUP_SAME_HEIGHT);
            break;

        case MUIA_Group_SameSize:
            _handle_bool_tag(data->flags, v,
                GROUP_SAME_HEIGHT | GROUP_SAME_WIDTH);
            break;

        case MUIA_Group_SameWidth:
            _handle_bool_tag(data->flags, v, GROUP_SAME_WIDTH);
            break;

        case MUIA_Group_Spacing:
            data->flags |= (GROUP_HSPACING | GROUP_VSPACING);
            data->horiz_spacing = v;
            data->vert_spacing = v;
            break;

        case MUIA_Group_VertSpacing:
            data->flags |= GROUP_VSPACING;
            data->vert_spacing = v;
            break;

        case MUIA_Group_Virtual:
            _handle_bool_tag(data->flags, v, GROUP_VIRTUAL);
            break;
        }
    }

    if (data->flags & GROUP_VIRTUAL)
    {
        struct GroupExtra *x;

        if (bad || !(x = group_extra(data)))
            bad = TRUE;
        else
        {
            /* This is only used for virtual groups */
            x->virt.ehn.ehn_Events = IDCMP_MOUSEBUTTONS;  /* filled on demand */
            x->virt.ehn.ehn_Priority = 10;  /* hears the click before all
                                             * other normal objects */
            x->virt.ehn.ehn_Flags = MUI_EHF_GUIMODE;
            x->virt.ehn.ehn_Object = obj;
            x->virt.ehn.ehn_Class = cl;
            /* MUI_Redraw() installs a clip region for it, MUI_Layout()
             * adds its offsets */
            _flags(obj) |= MADF_ISVIRTUALGROUP;
        }
    }
    else if (OCLASS(obj) == cl)
        /* A plain group passes its parent's background on while it has
         * none of its own: Group_PassesBackground() */
        muiAreaData(obj)->mad_Flags2 |= MADF2_PLAINGROUP;

    if (bad)
    {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }
    return (IPTR) obj;
}

/**************************************************************************
 OM_DISPOSE
**************************************************************************/
/*
 * Free owned row/column arrays and dispose the remaining
 * children, then invoke Area disposal. Requires prior Hide/Cleanup of
 * active resources and no surviving borrowers. Does not dispose the
 * borrowed Hook separately; return superclass result.
 */
IPTR Group__OM_DISPOSE(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct GroupExtra *x = data->x;
    struct MinNode *n;
    Object *child;

    if (x)
    {
        FreeVec(x->row_infos);
        FreeVec(x->col_infos);
        FreeVec(x);
    }
    FOR_CHILD(&data->childlist, n, child)
        MUI_DisposeObject(child);
    return DoSuperMethodA(cl, obj, msg);
}

/* Scrolling changes screen coordinates without re-running client layouts.
 * Walk actual Group subclasses so forwarded/custom ChildList attributes
 * cannot make a non-group look like a container. Hidden children move too. */
static void Group_ShiftChildren(struct IClass *cl, Object *obj,
    LONG dx, LONG dy)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    FOR_CHILD(&data->childlist, n, child)
    {
        struct IClass *childclass = OCLASS(child);

        _left(child) -= dx;
        _top(child) -= dy;
        muiAreaData(child)->mad_VirtualTop -= dy;
        while (childclass && childclass != cl)
            childclass = childclass->cl_Super;
        if (childclass)
            Group_ShiftChildren(cl, child, dx, dy);
    }
}

/* The attributes a set on a group keeps to itself, not passed on to its
 * children: the low words of their IDs (all MUIA 0x8042xxxx), sorted for
 * a binary search; a table instead of a switch saves about 200 bytes.
 * The group's own shape (Columns, Horiz, Rows, spacing) is among them:
 * MUI 3.8 and MUI 5 do not pass it on to child groups
 * (tests/lean/probe/gforward.c); UP-MCC's panels set their spacing on
 * their own groups. Searched in five halving steps (the filter's frame
 * stays as it was). */
#define GROUP_OWN(a) ((UWORD)(a))
static const UWORD group_own_attrs[] = {
    GROUP_OWN(MUIA_UserData), GROUP_OWN(MUIA_CustomBackfill),
    GROUP_OWN(MUIA_Draggable), GROUP_OWN(MUIA_HelpNode),
    GROUP_OWN(MUIA_ControlChar), GROUP_OWN(MUIA_AppMessage),
    GROUP_OWN(MUIA_CycleChain), GROUP_OWN(MUIA_Weight),
    GROUP_OWN(MUIA_Pressed), GROUP_OWN(MUIA_Group_ActivePage),
    GROUP_OWN(MUIA_Virtgroup_Top), GROUP_OWN(MUIA_Group_Horiz),
    GROUP_OWN(MUIA_Timer), GROUP_OWN(MUIA_HorizWeight),
    GROUP_OWN(MUIA_Group_Spacing), GROUP_OWN(MUIA_ShortHelp),
    GROUP_OWN(MUIA_Virtgroup_Left), GROUP_OWN(MUIA_FillArea),
    GROUP_OWN(MUIA_VertWeight), GROUP_OWN(MUIA_ShowMe),
    GROUP_OWN(MUIA_ContextMenuTrigger), GROUP_OWN(MUIA_HelpLine),
    GROUP_OWN(MUIA_Frame), GROUP_OWN(MUIA_Group_Rows),
    GROUP_OWN(MUIA_ContextMenu), GROUP_OWN(MUIA_Group_LayoutHook),
    GROUP_OWN(MUIA_Group_HorizSpacing), GROUP_OWN(MUIA_FrameTitle),
    GROUP_OWN(MUIA_ObjectID), GROUP_OWN(MUIA_Group_VertSpacing),
    GROUP_OWN(MUIA_Group_Columns), 0xffff  /* 32 entries for the search */
};
#define GROUP_OWN_LT(a, b) ((UWORD)(a) < (UWORD)(b) \
    && ((a) >> 16) == 0x8042 && ((b) >> 16) == 0x8042)
_Static_assert(GROUP_OWN_LT(MUIA_UserData, MUIA_CustomBackfill)
    && GROUP_OWN_LT(MUIA_CustomBackfill, MUIA_Draggable)
    && GROUP_OWN_LT(MUIA_Draggable, MUIA_HelpNode)
    && GROUP_OWN_LT(MUIA_HelpNode, MUIA_ControlChar)
    && GROUP_OWN_LT(MUIA_ControlChar, MUIA_AppMessage)
    && GROUP_OWN_LT(MUIA_AppMessage, MUIA_CycleChain)
    && GROUP_OWN_LT(MUIA_CycleChain, MUIA_Weight)
    && GROUP_OWN_LT(MUIA_Weight, MUIA_Pressed)
    && GROUP_OWN_LT(MUIA_Pressed, MUIA_Group_ActivePage)
    && GROUP_OWN_LT(MUIA_Group_ActivePage, MUIA_Virtgroup_Top)
    && GROUP_OWN_LT(MUIA_Virtgroup_Top, MUIA_Group_Horiz)
    && GROUP_OWN_LT(MUIA_Group_Horiz, MUIA_Timer)
    && GROUP_OWN_LT(MUIA_Timer, MUIA_HorizWeight)
    && GROUP_OWN_LT(MUIA_HorizWeight, MUIA_Group_Spacing)
    && GROUP_OWN_LT(MUIA_Group_Spacing, MUIA_ShortHelp)
    && GROUP_OWN_LT(MUIA_ShortHelp, MUIA_Virtgroup_Left)
    && GROUP_OWN_LT(MUIA_Virtgroup_Left, MUIA_FillArea)
    && GROUP_OWN_LT(MUIA_FillArea, MUIA_VertWeight)
    && GROUP_OWN_LT(MUIA_VertWeight, MUIA_ShowMe)
    && GROUP_OWN_LT(MUIA_ShowMe, MUIA_ContextMenuTrigger)
    && GROUP_OWN_LT(MUIA_ContextMenuTrigger, MUIA_HelpLine)
    && GROUP_OWN_LT(MUIA_HelpLine, MUIA_Frame)
    && GROUP_OWN_LT(MUIA_Frame, MUIA_Group_Rows)
    && GROUP_OWN_LT(MUIA_Group_Rows, MUIA_ContextMenu)
    && GROUP_OWN_LT(MUIA_ContextMenu, MUIA_Group_LayoutHook)
    && GROUP_OWN_LT(MUIA_Group_LayoutHook, MUIA_Group_HorizSpacing)
    && GROUP_OWN_LT(MUIA_Group_HorizSpacing, MUIA_FrameTitle)
    && GROUP_OWN_LT(MUIA_FrameTitle, MUIA_ObjectID)
    && GROUP_OWN_LT(MUIA_ObjectID, MUIA_Group_VertSpacing)
    && GROUP_OWN_LT(MUIA_Group_VertSpacing, MUIA_Group_Columns),
    "group_own_attrs: MUIA 0x8042xxxx, sorted");

/*
 * Group__OM_SET's forwarding: turn the tags the children must not see into
 * TAG_IGNORE, put MUIA_NestedDisabled in front (nestedAttrs) for a
 * MUIA_Disabled. TRUE when a tag is left to pass on: a set of the active
 * page alone (a Cycle's every change) sent every page an OM_SET of ignored
 * tags (opbench "cycle set"). Not inlined: Group__OM_SET's frame is on the
 * stack once per nesting level.
 */
static __attribute__((noinline)) BOOL group_forward_filter(struct opSet *msg,
    struct TagItem *nestedAttrs)
{
    struct TagItem *tags, *tag;
    BOOL left = FALSE;

    tags = msg->ops_AttrList;
    while ((tag = NextTagItem(&tags)) != NULL)
    {
        ULONG id = tag->ti_Tag;

        if (id == MUIA_NoNotify)
            continue;
        if (id == MUIA_Disabled)
        {
            nestedAttrs[0].ti_Data = tag->ti_Data;
            msg->ops_AttrList = nestedAttrs;
        }
        else if ((id >> 16) == 0x8042)
        {
            const UWORD *p = group_own_attrs;
            UWORD w = id;

            if (p[16] <= w) p += 16;
            if (p[8] <= w) p += 8;
            if (p[4] <= w) p += 4;
            if (p[2] <= w) p += 2;
            if (p[1] <= w) p += 1;
            if (*p == w)
            {
                tag->ti_Tag = TAG_IGNORE;
                continue;
            }
        }
        left = TRUE;
    }
    return left;
}

/* A virtual group's offsets as OM_SET asks for them (x, y, clamped to its
 * area): its children shifted, shown again and drawn */
static __attribute__((noinline)) void group_virt_scroll(struct IClass *cl,
    Object *obj, LONG x, LONG y)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct GroupVirtual *v = VX(data);

    x = CLAMP(x, 0, MAX(0, v->virt_mwidth - _mwidth(obj)));
    y = CLAMP(y, 0, MAX(0, v->virt_mheight - _mheight(obj)));

    if (x == v->virt_offx && y == v->virt_offy)
        return;
    x -= v->virt_offx;
    y -= v->virt_offy;
    v->virt_offx += x;
    v->virt_offy += y;
    Group_ShiftChildren(cl, obj, x, y);
    if (_flags(obj) & MADF_CANDRAW)
    {
        Group__MUIM_Hide(cl, obj, NULL);
        Group__MUIM_Show(cl, obj, NULL);
        GROUP_SET_UPDATE(data, GROUP_UPDATE_SCROLL);
        MUI_Redraw(obj, MADF_DRAWOBJECT);
    }
}

/**************************************************************************
 OM_SET
**************************************************************************/
/*
 * Apply Group tags, then Area, then optionally broadcast filtered writable
 * tags. Delay Disabled until after child propagation; only the last
 * Disabled tag is restored. Columns/Rows request Window recalculation;
 * spacing/hook alone do not. Virtual offsets may hide/layout/show/redraw
 * even outside normal setup. Return superclass setter result, ignoring
 * child/layout results; tag IDs can remain TAG_IGNORE.
 */
IPTR Group__OM_SET(struct IClass *cl, Object *obj, struct opSet *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct TagItem *tags = msg->ops_AttrList;
    struct TagItem *disableTag = NULL;
    struct TagItem *tag;
    BOOL forward = TRUE;
    BOOL need_recalc = FALSE;
    IPTR retval;
    LONG virt_offx = GV(data)->virt_offx, virt_offy = GV(data)->virt_offy;

    /* Group attributes before DoSuperMethodA(): a notification on
     * MUIA_Group_ActivePage whose hook gets the attribute sees the new
     * page */
    while ((tag = NextTagItem(&tags)) != NULL)
    {
        IPTR v = tag->ti_Data;

        switch (tag->ti_Tag)
        {
        case MUIA_Disabled:
            disableTag = tag;
            tag->ti_Tag = TAG_IGNORE;
            break;
        case MUIA_Group_Columns:
        case MUIA_Group_Rows:
        {
            struct GroupExtra *x = group_extra(data);

            if (x)
            {
                v = MAX((ULONG) v, 1);
                if (tag->ti_Tag == MUIA_Group_Columns)
                    x->columns = v, x->rows = 0;
                else
                    x->rows = v, x->columns = 0;
                need_recalc = TRUE;
            }
            break;
        }
        case MUIA_Group_Horiz:
            /* MUI 3.8 also takes this after OM_NEW although its autodoc
             * says [I..]: TheBar's demo3 sets it on its Radio objects
             * (a row of radio buttons; Zune stacked them). */
            if (!(data->flags & GROUP_HORIZ) != !v)
            {
                data->flags ^= GROUP_HORIZ;
                need_recalc = TRUE;
            }
            break;
        case MUIA_Group_ActivePage:
            change_active_page(cl, obj, (LONG) v);
            break;
        case MUIA_Group_Forward:
            forward = v;
            break;
        case MUIA_Group_HorizSpacing:
            data->flags |= GROUP_HSPACING;
            data->horiz_spacing = v;
            break;
        case MUIA_Group_Spacing:
            data->flags |= (GROUP_HSPACING | GROUP_VSPACING);
            data->horiz_spacing = v;
            data->vert_spacing = v;
            break;
        case MUIA_Group_VertSpacing:
            data->flags |= GROUP_VSPACING;
            data->vert_spacing = v;
            break;
        case MUIA_Group_LayoutHook:
            /* [ach] MUI takes this after OM_NEW too, although its
             * autodoc says otherwise; programs use it */
            data->layout_hook = (struct Hook *)v;
            break;
        /* only a virtual group scrolls its children */
        case MUIA_Virtgroup_Left:
            virt_offx = v;
            break;
        case MUIA_Virtgroup_Top:
            virt_offy = v;
            break;
        }
    }

    if (need_recalc && muiRenderInfo(obj))
        DoMethod(_win(obj), MUIM_Window_RecalcDisplay, (IPTR) obj);

    retval = DoSuperMethodA(cl, obj, (Msg) msg);
    if (disableTag)
        disableTag->ti_Tag = MUIA_Disabled;

    if (forward)
    {
        struct TagItem *attrsOrig = msg->ops_AttrList;
        struct TagItem nestedAttrs[2] = {
            { MUIA_NestedDisabled,  FALSE},
            { TAG_MORE,             (IPTR)msg->ops_AttrList}
        };

        if (group_forward_filter(msg, nestedAttrs))
            Group_DispatchMsg(cl, obj, (Msg) msg);
        msg->ops_AttrList = attrsOrig;
    }

    if (disableTag)
    {
        struct TagItem *origTags = msg->ops_AttrList;
        struct TagItem disableAttrs[2] = {
            { MUIA_Disabled,  disableTag->ti_Data },
            { TAG_DONE, 		0}
        };

        msg->ops_AttrList = disableAttrs;
        DoSuperMethodA(cl, obj, (Msg) msg);
        msg->ops_AttrList = origTags;
    }

    if (data->flags & GROUP_VIRTUAL)
        group_virt_scroll(cl, obj, virt_offx, virt_offy);

    return retval;
}

/**************************************************************************
 OM_GET
**************************************************************************/
/*
 * Write recognized Group/virtual getters to caller storage, including
 * private MUIA_Group_ChildCount from the unchecked stored count. Otherwise
 * try Area then children until success unless forwarding is suppressed.
 * Return getter status. Borrowed ChildList stays group-owned; Horiz
 * returns its raw flag bit.
 */
IPTR Group__OM_GET(struct IClass *cl, Object *obj, struct opGet *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    IPTR v;

    switch (msg->opg_AttrID)
    {
    case MUIA_Version:
    case MUIA_Revision:
        /* Notify reports the version of the object's class library. */
        return DoSuperMethodA(cl, obj, (Msg)msg);
    case MUIA_Group_ActivePage: v = data->active_page; break;
    case MUIA_Group_ChildList: v = (IPTR)&data->childlist; break;
    case MUIA_Group_Horiz: v = data->flags & GROUP_HORIZ; break;
    case MUIA_Group_HorizSpacing: v = data->horiz_spacing; break;
    case MUIA_Group_VertSpacing: v = data->vert_spacing; break;
    case MUIA_Group_ChildCount: v = data->num_children; break;
    case MUIA_Virtgroup_Left: v = GV(data)->virt_offx; break;
    case MUIA_Virtgroup_Top: v = GV(data)->virt_offy; break;
    case MUIA_Virtgroup_Width: v = GV(data)->virt_mwidth; break;
    case MUIA_Virtgroup_Height: v = GV(data)->virt_mheight; break;
    case MUIA_Virtgroup_MinWidth: v = GV(data)->saved_minwidth; break;
    case MUIA_Virtgroup_MinHeight: v = GV(data)->saved_minheight; break;
    default:
        /* our handler didn't understand the attribute: the superclass,
         * then the children */
        if (DoSuperMethodA(cl, obj, (Msg) msg))
            return 1;
        if (!(data->flags & (GROUP_NOFWD_GET | GROUP_NOFWD)))
        {
            struct MinNode *n;
            Object *child;

            FOR_CHILD(&data->childlist, n, child)
                if (DoMethodA(child, (Msg) msg))
                    return 1;
        }
        return 0;
    }
    *msg->opg_Storage = v;
    return 1;
}

/* MUIM_Group_AddTail, MUIM_Group_AddHead, OM_ADDMEMBER and the private
 * MUIM_Group_Insert (msg->pred after msg->obj: a current child, not
 * NULL): group_add */

IPTR Group__MUIM_Remove(struct IClass *cl, Object *obj,
    struct MUIP_Group_Remove *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    Object *child = msg->obj;

    if (!child)
        return TRUE;
    if (child == group_title(data))
        data->x->titlegroup = NULL;
    if (_flags(obj) & MADF_CANDRAW)
        DoHideMethod(child);
    if (_flags(obj) & MADF_SETUP)
        DoMethod(child, MUIM_Cleanup);
    if (muiNotifyData(obj)->mnd_GlobalInfo)
    {
        DoMethod(child, MUIM_DisconnectParent);
        muiNotifyData(child)->mnd_ParentObject = NULL;
        _flags(child) &= ~MADF_INVIRTUALGROUP;
    }

    if (data->flags & GROUP_CHANGING)
        data->flags |= GROUP_CHANGED;
    data->num_children--;
    /* What Family_Remove did after the group's part; rootclass's OM_REMOVE
     * is this Remove() */
    DoMethod(child, MUIM_DisconnectParent);
    muiNotifyData(child)->mnd_ParentObject = NULL;
    Remove((struct Node *)_OBJECT(child));
    return TRUE;
}

/*
 * Find a child by number or relative to ref, as Family_GetChild does.
 * The returned child is borrowed.
 */
IPTR Group__MUIM_Family_GetChild(struct IClass *cl, Object *obj,
    struct MUIP_Family_GetChild *m)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child, *prev = NULL;
    LONG nr = m->nr, counter = 0;

    FOR_CHILD(&data->childlist, n, child)
    {
        if (nr >= 0 && nr == counter)
            return (IPTR) child;
        if (m->ref && m->ref == child)
        {
            if (nr == MUIV_Family_GetChild_Next)
                return n->mln_Succ ? (IPTR) BASEOBJECT(n) : 0;
            if (nr == MUIV_Family_GetChild_Previous)
                return (IPTR) prev;
        }
        if (nr == MUIV_Family_GetChild_First)
            return (IPTR) child;
        prev = child;
        counter++;
    }
    if (nr == MUIV_Family_GetChild_Last)
        return (IPTR) prev;
    return 0;
}

/*
 * MUIM_ConnectParent and MUIM_GlobalSetup (MUI private): Area first, then
 * each child becomes ours (parent and inherited virtual flag) and gets the
 * method (ConnectParent with this group as the parent). Return TRUE.
 */
static IPTR Group__MUIM_ConnectParent(struct IClass *cl, Object *obj,
    Msg msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    DoSuperMethodA(cl, obj, msg);
    FOR_CHILD(&data->childlist, n, child)
    {
        if ((_flags(obj) & MADF_INVIRTUALGROUP)
            || (data->flags & GROUP_VIRTUAL))
            _flags(child) |= MADF_INVIRTUALGROUP;
        muiNotifyData(child)->mnd_ParentObject = obj;
        DoMethod(child, MUIM_ConnectParent, (IPTR) obj);
    }
    return TRUE;
}

static IPTR Group__MUIM_GlobalSetup(struct IClass *cl, Object *obj,
    Msg msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    DoSuperMethodA(cl, obj, msg);
    FOR_CHILD(&data->childlist, n, child)
    {
        if ((_flags(obj) & MADF_INVIRTUALGROUP)
            || (data->flags & GROUP_VIRTUAL))
            _flags(child) |= MADF_INVIRTUALGROUP;
        muiNotifyData(child)->mnd_ParentObject = obj;
        DoMethodA(child, msg);
    }
    return TRUE;
}

/*
 * Disconnect every child, clear its parent and inherited virtual flag, then
 * disconnect Area. Return TRUE; the children are not disposed.
 */
IPTR Group__MUIM_DisconnectParent(struct IClass *cl, Object *obj,
    struct MUIP_ConnectParent *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    FOR_CHILD(&data->childlist, n, child)
    {
        /* _parent() is the group link, set on adding and cleared on
         * removal. Voyager removes a view from its parent after removing
         * the window from the application. */
        DoMethodA(child, (Msg) msg);
        _flags(child) &= ~MADF_INVIRTUALGROUP;
    }
    DoSuperMethodA(cl, obj, (Msg) msg);
    return TRUE;
}

/*
 * Put group in exchange state
 */
IPTR Group__MUIM_InitChange(struct IClass *cl, Object *obj,
    struct MUIP_Group_InitChange *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);

    data->flags &= ~GROUP_CHANGED;
    data->flags |= GROUP_CHANGING;
    return TRUE;
}

/*
 * MUIM_Group_ExitChange always (MUI 3.8 and MUI 4.0 on m68k lay out again
 * each time), the private MUIM_Group_ExitChange2 only while changing:
 * leave the change state and, when set up, recalculate the window unless
 * a parent group is changing (Zune's prefs program changes pages inside
 * NList's own InitChange/ExitChange). Return TRUE.
 */
static __attribute__((noinline)) IPTR group_exit_change(struct IClass *cl,
    Object *obj)
{
    Object *win, *parent = obj;

    ((struct MUI_GroupData *)INST_DATA(cl, obj))->flags
        &= ~(GROUP_CHANGING | GROUP_CHANGED);
    if (!(_flags(obj) & MADF_SETUP) || !(win = _win(obj)))
        return TRUE;
    while ((parent = _parent(parent)) && parent != win)
        if (((struct MUI_GroupData *)INST_DATA(cl, parent))->flags
            & GROUP_CHANGING)
            return TRUE;
    DoMethod(win, MUIM_Window_RecalcDisplay, (IPTR) obj);
    return TRUE;
}

IPTR Group__MUIM_ExitChange(struct IClass *cl, Object *obj, Msg msg)
{
    return group_exit_change(cl, obj);
}

static IPTR Group__MUIM_ExitChange2(struct IClass *cl, Object *obj, Msg msg)
{
    if (!(((struct MUI_GroupData *)INST_DATA(cl, obj))->flags
        & GROUP_CHANGING))
        return TRUE;
    return group_exit_change(cl, obj);
}

/*
 * Sort the family
 */
static IPTR GroupMoveMember(struct IClass *cl, Object *obj,
    struct MUIP_Group_MoveMember *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child, *after = NULL, *array[2];
    LONG remaining = 0, pos;
    BOOL found = FALSE;

    FOR_CHILD(&data->childlist, n, child)
        if (child == msg->o) found = TRUE;
        else remaining++;
    if (!found) return FALSE;
    pos = msg->pos;
    if (pos < 0) pos = remaining + pos + 1;
    if (pos < 0) pos = 0;
    if (pos > remaining) pos = remaining;
    for (n = data->childlist.mlh_Head; pos && n->mln_Succ; n = n->mln_Succ)
        if ((child = BASEOBJECT(n)) != msg->o) { after = child; pos--; }
    array[0] = msg->o;
    array[1] = NULL;
    return group_reorder(data, after, array);
}

static IPTR GroupReorder(struct IClass *cl, Object *obj,
    struct MUIP_Group_Reorder *msg)
{
    return group_reorder(INST_DATA(cl, obj), msg->after, msg->array);
}

IPTR Group__MUIM_Sort(struct IClass *cl, Object *obj,
    struct MUIP_Group_Sort *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    Object **p;

    /* Family_Sort: the list becomes the array, in its order */
    NewList((struct List *)&data->childlist);
    for (p = msg->obj; *p; p++)
        AddTail((struct List *)&data->childlist,
            (struct Node *)_OBJECT(*p));
    return TRUE;
}

/**************************************************************************
 MUIM_Group_DoMethodNoForward

 Executes the given method but does not forward it to the children
**************************************************************************/
IPTR Group__MUIM_DoMethodNoForward(struct IClass *cl, Object *obj,
    struct MUIP_Group_DoMethodNoForward *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    IPTR rc;

    data->flags |= GROUP_NOFWD;
    rc = DoMethodA(obj, (Msg) & msg->DoMethodID);
    data->flags &= ~GROUP_NOFWD;
    return rc;
}

/*
 * Broadcast the borrowed message to every child unless the forwarding is
 * off (DoMethodNoForward). Discard every child result and return TRUE.
 * Explicit traversal methods do not use this suppression helper.
 */
static ULONG Group_DispatchMsg(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    if (!(data->flags & GROUP_NOFWD))
        FOR_CHILD(&data->childlist, n, child)
            DoMethodA(child, msg);
    return TRUE;
}

/* A method the group does not answer: to the children, then Area */
static IPTR Group_Forward(struct IClass *cl, Object *obj, Msg msg)
{
    Group_DispatchMsg(cl, obj, msg);
    return DoSuperMethodA(cl, obj, msg);
}

/**************************************************************************
 MUIM_Setup
**************************************************************************/
/*
 * Set up Area then all children including hidden children. Return FALSE on
 * superclass or child failure; on child failure clean the earlier
 * children and Area again. Resolve unspecified spacing from preferences.
 * Register virtual mouse handler only after all succeed.
 */
IPTR Group__MUIM_Setup(struct IClass *cl, Object *obj,
    struct MUIP_Setup *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct ZunePrefsNew *prefs;
    struct MinNode *n;
    Object *child;

    if (!DoSuperMethodA(cl, obj, (Msg) msg))
        return FALSE;

    prefs = muiGlobalInfo(obj)->mgi_Prefs;
    if (!(data->flags & GROUP_HSPACING))
        data->horiz_spacing = prefs->group_hspacing;
    if (!(data->flags & GROUP_VSPACING))
        data->vert_spacing = prefs->group_vspacing;
    /* MUI 3.8 puts 2 pixels beside and 1 pixel between rows of objects
     * (measured: a row of three buttons, a column of texts) from the third
     * stage of fitting a window into the screen */
    if (muiRenderInfo(obj)->mri_Fallback >= FALLBACK_SPACING)
    {
        if (!(data->flags & GROUP_HSPACING))
            data->horiz_spacing = MIN(data->horiz_spacing, 2);
        if (!(data->flags & GROUP_VSPACING))
            data->vert_spacing = MIN(data->vert_spacing, 1);
    }
    FOR_CHILD(&data->childlist, n, child)
    {
        if (!DoSetupMethod(child, msg->RenderInfo))
        {
            /* MUIM_Cleanup to all objects that received MUIM_Setup */
            Object *failed = child;

            FOR_CHILD(&data->childlist, n, child)
            {
                if (child == failed)
                    break;
                DoMethod(child, MUIM_Cleanup);
            }
            /* And undo our own (Area's) setup: a failed MUIM_Setup leaves
             * the object as it was. The group stayed marked as set up, so
             * the window's dispose sent it MUIM_Cleanup later, and the
             * group passed that on to children that had never been set
             * up (MUIMine 1.2, whose too large mine field window fails to
             * open: the machine reset when it quit). */
            DoSuperMethod(cl, obj, MUIM_Cleanup);
            return FALSE;
        }
    }

    if (data->flags & GROUP_VIRTUAL)
        DoMethod(_win(obj), MUIM_Window_AddEventHandler,
            (IPTR) &VX(data)->ehn);
    return TRUE;
}

/**************************************************************************
 MUIM_Cleanup
**************************************************************************/
/*
 * Remove virtual event handler, clean every child, then clean Area and
 * return its result.
 */
IPTR Group__MUIM_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    if (data->flags & GROUP_VIRTUAL)
        DoMethod(_win(obj), MUIM_Window_RemEventHandler,
            (IPTR) &VX(data)->ehn);
    FOR_CHILD(&data->childlist, n, child)
        DoMethodA(child, msg);
    return DoSuperMethodA(cl, obj, msg);
}

/**************************************************************************
 MUIM_Draw - draw the group
**************************************************************************/

/*
 * Allocate a Region covering the group minus drawable child rectangles
 * clipped to its inner box, selecting active page where needed. Return
 * owned Region or NULL on allocation failure. Caller must dispose it or
 * transfer it to MUI_AddClipRegion, which consumes it even on failure.
 * Requires live geometry; page counting includes hidden children.
 */
static struct Region *group_children_clip_region(struct IClass *cl,
    Object *obj);

/* The children a group's gaps leave out, at most max: each visible one's
 * box clipped to the group's inner box (of a page group the title group
 * and the active page). The count, or -1 when there are more. */
static int group_collect_children(struct IClass *cl, Object *obj,
    struct Rectangle *kids, int max)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    Object *title = group_title(data);
    struct MinNode *n;
    Object *child;
    WORD l = _mleft(obj), t = _mtop(obj), r = _mright(obj),
        b = _mbottom(obj);
    LONG page = data->active_page + 1;
    int count = 0;

    FOR_CHILD(&data->childlist, n, child)
    {
        WORD x0, y0, x1, y1;

        if (child != title)
        {
            if ((data->flags & GROUP_PAGEMODE) && --page)
                continue;
        }
        if (!(_flags(child) & MADF_CANDRAW)
            || _width(child) <= 0 || _height(child) <= 0)
            continue;
        x0 = MAX(_left(child), l);
        y0 = MAX(_top(child), t);
        x1 = MIN(_right(child), r);
        y1 = MIN(_bottom(child), b);
        if (x1 < x0 || y1 < y0)
            continue;
        if (count == max)
            return -1;
        kids->MinX = x0;
        kids->MinY = y0;
        kids->MaxX = x1;
        kids->MaxY = y1;
        kids++;
        count++;
    }
    return count;
}

/* Paint the group background only where no child covers the inner box,
 * in bands (every child edge starts one) instead of under a clip region:
 * region clipping allocated temporary regions and rebuilt the layer clip
 * for every group drawn. Gaps of consecutive bands with the same
 * horizontal extent are painted as one rectangle when the background
 * gives the same pixels however it is cut (a fill costs about the same
 * whatever its height; a margin beside a column of children is cut at
 * every child edge). When the background is a plain pen, all through to
 * the window's, the gaps are RectFills of that pen: no MUIM_DrawBackground
 * walk up the parents per gap. kids is sorted here. More children than
 * GROUP_GAP_CHILDREN: the arrays in one block of memory. */
#define GROUP_GAP_CHILDREN 32
struct gap { WORD x0, x1, y0, y1; };

static void group_paint_gap(Object *obj, const struct gap *g, LONG pen,
    ULONG flags)
{
    if (pen >= 0)
        RectFill(_rp(obj), g->x0, g->y0, g->x1, g->y1);
    else
        zune_draw_background(obj, g->x0, g->y0, g->x1 - g->x0 + 1,
            g->y1 - g->y0 + 1, g->x0, g->y0, flags);
}

static __attribute__((noinline)) void group_fill_gaps(Object *obj,
    struct Rectangle *kids, int n, ULONG flags)
{
    WORD ysbuf[2 * GROUP_GAP_CHILDREN + 2];
    struct gap gbuf[2 * (GROUP_GAP_CHILDREN + 1)];
    WORD *ys = ysbuf, *y;
    struct gap *pend = gbuf, *cur = gbuf + GROUP_GAP_CHILDREN + 1, *g, *p;
    struct Rectangle *k, *end = kids + n;
    APTR heap = NULL;
    WORD left = _mleft(obj), right = _mright(obj);
    LONG pen;
    BOOL merge;
    int npend = 0;

    if (n > GROUP_GAP_CHILDREN)
    {
        if (!(heap = AllocVec(2 * (n + 1) * sizeof(struct gap)
                + (2 * n + 2) * sizeof(WORD), MEMF_ANY)))
            return;
        pend = heap;
        cur = pend + n + 1;
        ys = (WORD *)(cur + n + 1);
    }
    if (zune_background_fill_pen(obj, &pen) && pen >= 0)
    {
        merge = TRUE;
        if (_rp(obj)->FgPen != (BYTE) pen)
            SetAPen(_rp(obj), pen);
    }
    else
    {
        pen = -1;
        merge = zune_background_is_position_free(obj);
    }

    if (n == 0)
    {
        /* no child (a List, its own Group): the inner box, one gap */
        pend->x0 = left;
        pend->x1 = right;
        pend->y0 = _mtop(obj);
        pend->y1 = _mbottom(obj);
        if (pend->y1 >= pend->y0 && right >= left)
            group_paint_gap(obj, pend, pen, flags);
        return;
    }

    /* the band edges, sorted (few and nearly in order); the children by
     * their left edge, so that each band's gaps come out in order */
    y = ys;
    *y++ = _mtop(obj);
    *y++ = _mbottom(obj) + 1;
    for (k = kids; k < end; k++)
    {
        WORD *s;
        struct Rectangle r = *k, *m;

        for (m = k; m > kids && m[-1].MinX > r.MinX; m--)
            *m = m[-1];
        *m = r;
        *y = r.MinY;
        for (s = y++; s > ys && s[-1] > s[0]; s--)
        {
            WORD t = s[0]; s[0] = s[-1]; s[-1] = t;
        }
        *y = r.MaxY + 1;
        for (s = y++; s > ys && s[-1] > s[0]; s--)
        {
            WORD t = s[0]; s[0] = s[-1]; s[-1] = t;
        }
    }
    for (; ys + 1 < y; ys++)
    {
        WORD y0 = ys[0], y1 = ys[1] - 1, x = left;
        int ncur;

        if (y0 > y1)
            continue;
        /* Bands split at every child edge, so a child covers a band fully
         * or not at all */
        g = cur;
        for (k = kids; k < end; k++)
            if (k->MinY <= y0 && k->MaxY >= y1)
            {
                if (k->MinX > x)
                {
                    g->x0 = x;
                    g->x1 = k->MinX - 1;
                    g->y0 = y0;
                    g->y1 = y1;
                    g++;
                }
                if (k->MaxX >= x)
                    x = k->MaxX + 1;
            }
        if (right >= x)
        {
            g->x0 = x;
            g->x1 = right;
            g->y0 = y0;
            g->y1 = y1;
            g++;
        }
        ncur = g - cur;
        /* Pending gaps this band extends continue; the others are painted
         * (both lists in order of x) */
        g = cur;
        for (p = pend; p < pend + npend; p++)
        {
            if (merge)
            {
                while (g < cur + ncur && g->x0 < p->x0)
                    g++;
                if (g < cur + ncur && g->x0 == p->x0 && g->x1 == p->x1)
                {
                    g->y0 = p->y0;
                    continue;
                }
            }
            group_paint_gap(obj, p, pen, flags);
        }
        g = pend;
        pend = cur;
        cur = g;
        npend = ncur;
    }
    for (p = pend; p < pend + npend; p++)
        group_paint_gap(obj, p, pen, flags);
    if (heap)
        FreeVec(heap);
}

static struct Region *group_children_clip_region(struct IClass *cl,
    Object *obj)
{
    struct Region *region = NewRegion();

    if (region)
    {
        struct MUI_GroupData *data = GDATA(cl, obj);
        struct Rectangle rect;
        LONG page = data->active_page + 1;
        struct MinNode *n;
        Object *child;

        rect.MinX = _left(obj);
        rect.MinY = _top(obj);
        rect.MaxX = _right(obj);
        rect.MaxY = _bottom(obj);
        OrRectRegion(region, &rect);
        FOR_CHILD(&data->childlist, n, child)
        {
            if ((data->flags & GROUP_PAGEMODE)
                && child != group_title(data) && --page)
                continue;
            if ((_flags(child) & MADF_CANDRAW)
                && (_width(child) > 0) && (_height(child) > 0))
            {
                rect.MinX = MAX(_left(child), _mleft(obj));
                rect.MinY = MAX(_top(child), _mtop(obj));
                rect.MaxX = MIN(_right(child), _mright(obj));
                rect.MaxY = MIN(_bottom(child), _mbottom(obj));
                if ((rect.MaxX >= rect.MinX) && (rect.MaxY >= rect.MinY))
                    ClearRectRegion(region, &rect);
            }
        }
    }
    return region;
}

/* A page switch (Group__MUIM_Draw, a page update): the background of the
 * inner box, in redraw-without-clear mode only where no child paints. Not
 * inlined: Group__MUIM_Draw's frame is on the stack once per nesting
 * level. When the new page alone (no title group) covers the inner box,
 * nothing is painted (opbench "cycle set", a Cycle being a page group of
 * Texts). Otherwise, for a background that gives the same pixels however
 * it is cut (zune_background_is_position_free), as group_fill_gaps' bands
 * with no clip region: installing one and taking it away again costs
 * about 2 ms on OS 3.1 (tests/native/drawcost.c), several fills a
 * fraction of that; the region paints it from the inner box's corner. */
/* The active page alone (no title group) covers the inner box: the clip
 * would hide all of the background */
static BOOL group_page_covers(struct MUI_GroupData *data, Object *obj)
{
    struct MinNode *n;
    LONG i = data->active_page;

    if (group_title(data))
        return FALSE;
    for (n = data->childlist.mlh_Head; n->mln_Succ; n = n->mln_Succ)
        if (--i < 0)
        {
            Object *child = BASEOBJECT(n);

            return (_flags(child) & MADF_CANDRAW)
                && _left(child) <= _mleft(obj) && _top(child) <= _mtop(obj)
                && _right(child) >= _mright(obj)
                && _bottom(child) >= _mbottom(obj);
        }
    return FALSE;
}

static __attribute__((noinline)) BOOL group_page_gaps(struct IClass *cl,
    Object *obj)
{
    struct Rectangle kids[GROUP_GAP_CHILDREN];
    int n;

    if (!zune_background_is_position_free(obj)
        || (n = group_collect_children(cl, obj, kids,
            GROUP_GAP_CHILDREN)) < 0)
        return FALSE;
    group_fill_gaps(obj, kids, n, 0);
    return TRUE;
}

static __attribute__((noinline)) void group_draw_page_background(
    struct IClass *cl, Object *obj)
{
    struct Region *r = NULL;
    APTR c = (APTR)-1;

    if (muiGlobalInfo(obj)->mgi_Prefs->window_redraw
        == WINDOW_REDRAW_WITHOUT_CLEAR)
    {
        if (group_page_covers(GDATA(cl, obj), obj)
            || group_page_gaps(cl, obj))
            return;
        r = group_children_clip_region(cl, obj);
    }

    if (r)
        c = MUI_AddClipRegion(muiRenderInfo(obj), r);
    zune_draw_background(obj, _mleft(obj), _mtop(obj), _mwidth(obj),
        _mheight(obj), _mleft(obj), _mtop(obj), 0);
    if (r)
        MUI_RemoveClipRegion(muiRenderInfo(obj), c);
}

/* Establish the outer virtual clip before a subclass installs its own.
 * MUI clipping handles represent the previous region, which can be NULL.
 * Existing subclasses rely on this outer clip when tracking their handle.
 */
IPTR Group__MUIM_Virtgroup_BeginPaint(struct IClass *cl, Object *obj)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct GroupVirtual *v;
    struct Region *region;
    struct Rectangle rect;

    if ((data->flags & (GROUP_PAINTING | GROUP_VIRTUAL)) != GROUP_VIRTUAL)
        return FALSE;
    v = VX(data);
    if (!(region = NewRegion()))
        return FALSE;
    rect.MinX = _mleft(obj);
    rect.MinY = _mtop(obj);
    rect.MaxX = _mright(obj);
    rect.MaxY = _mbottom(obj);
    if (_mwidth(obj) > 0 && _mheight(obj) > 0)
    {
        BOOL okay = TRUE;

        if (data->flags & GROUP_UPDATE_SCROLL)
        {
            /* only what the scroll uncovered */
            LONG dx = v->virt_offx - v->old_virt_offx;
            LONG dy = v->virt_offy - v->old_virt_offy;
            struct Rectangle exposed = rect;

            if (dx)
            {
                if (dx > 0)
                    exposed.MinX = MAX(rect.MinX, rect.MaxX - dx + 1);
                else
                    exposed.MaxX = MIN(rect.MaxX, rect.MinX - dx - 1);
                okay = OrRectRegion(region, &exposed);
            }
            if (okay && dy)
            {
                exposed = rect;
                if (dy > 0)
                    exposed.MinY = MAX(rect.MinY, rect.MaxY - dy + 1);
                else
                    exposed.MaxY = MIN(rect.MaxY, rect.MinY - dy - 1);
                okay = OrRectRegion(region, &exposed);
            }
        }
        else
            okay = OrRectRegion(region, &rect);
        if (!okay)
        {
            DisposeRegion(region);
            return FALSE;
        }
    }
    v->paint_clip = MUI_AddClipRegion(muiRenderInfo(obj), region);
    if (v->paint_clip == (APTR)-1)
        return FALSE;
    data->flags |= GROUP_PAINTING;
    return TRUE;
}

IPTR Group__MUIM_Virtgroup_EndPaint(struct IClass *cl, Object *obj)
{
    struct MUI_GroupData *data = GDATA(cl, obj);

    if (data->flags & GROUP_PAINTING)
    {
        MUI_RemoveClipping(muiRenderInfo(obj), VX(data)->paint_clip);
        VX(data)->paint_clip = (APTR)-1;
        data->flags &= ~GROUP_PAINTING;
    }
    return 0;
}

/* TRUE when the group's gaps show the window background (no group up to
 * the window has one of its own) and the window has just been painted
 * with it (mri_BgFresh): its gaps need no fill then */
static BOOL group_on_fresh_background(Object *obj)
{
    Object *p = obj;

    if (!muiRenderInfo(obj)->mri_BgFresh)
        return FALSE;
    while (p && Group_PassesBackground(p))
        p = _parent(p);
    return !p && zune_window_background_is_position_free(_win(obj));
}

/* Area's draw for a group in "redraw without clear" mode: the frame, and
 * the background only where no child will paint. Kept out of
 * Group__MUIM_Draw: its buffers would otherwise sit on the stack once for
 * every nesting level while the children draw. A group with more
 * children than group_fill_gaps' stack arrays: the rectangles on the heap
 * too (the clip region it took before, group_children_clip_region, costs a
 * ClearRectRegion per child, each walking and allocating the region's
 * rectangles: 2.8 s per 8-pixel scroll of a Virtgroup of 80 buttons on a
 * 68020, against MUI 3.8's 73 ms, round draw1, tests/lean/sgstep.c); the
 * region when memory is short. A custom backfill keeps one call for the
 * whole inner box; an update (a page switch) or a frame alone paints no
 * background, so no gaps either: the children are not collected then. */
static __attribute__((noinline)) void group_draw_frame_and_gaps(
    struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct Rectangle kbuf[GROUP_GAP_CHILDREN], *kids = kbuf;
    struct Region *r = NULL;
    APTR c = (APTR)-1;
    int n;

    if (!(_flags(obj) & MADF_FILLAREA))
    {
        /* Area paints only outside the inner box (a virtual group's frame
         * ring) */
        DoSuperMethodA(cl, obj, (Msg) msg);
        return;
    }
    if (!(muiAreaData(obj)->mad_Flags2 & MADF2_CUSTOMBACKFILL))
    {
        if (!(_flags(obj) & (MADF_DRAWOBJECT | MADF_DRAWALL)))
        {
            DoSuperMethodA(cl, obj, (Msg) msg);
            return;
        }
        if (data->num_children <= GROUP_GAP_CHILDREN
            || (kids = AllocVec(data->num_children * sizeof(*kids),
                MEMF_ANY)))
        {
            n = group_collect_children(cl, obj, kids,
                kids != kbuf ? data->num_children : GROUP_GAP_CHILDREN);
            /* Area paints the frame and its surround; the group fills the
             * uncovered parts of its inner box itself */
            _flags(obj) &= ~MADF_FILLAREA;
            DoSuperMethodA(cl, obj, (Msg) msg);
            _flags(obj) |= MADF_FILLAREA;
            if ((msg->flags & MADF_DRAWOBJECT)
                && !group_on_fresh_background(obj))
                group_fill_gaps(obj, kids, n, _flags(obj));
            if (kids != kbuf)
                FreeVec(kids);
            return;
        }
    }
    /* In refresh mode each clip costs two refresh cycles */
    if ((r = group_children_clip_region(cl, obj)))
        c = MUI_AddClipRegion(muiRenderInfo(obj), r);
    DoSuperMethodA(cl, obj, (Msg) msg);
    if (r)
        MUI_RemoveClipRegion(muiRenderInfo(obj), c);
}

/* Paint the background of an object's box outside its inner box: the
 * bands above and below it, then the strips beside it */
static __attribute__((noinline)) void group_clear_ring(Object *obj)
{
    WORD r[4][4], (*p)[4] = r;
    WORD l = _left(obj), t = _top(obj), rt = _right(obj), b = _bottom(obj);
    WORD ml = _mleft(obj), mt = _mtop(obj), mr = _mright(obj),
        mb = _mbottom(obj);

    if (mt > t)
        p[0][0] = l, p[0][1] = t, p[0][2] = rt, p[0][3] = mt - 1, p++;
    if (mb < b)
        p[0][0] = l, p[0][1] = mb + 1, p[0][2] = rt, p[0][3] = b, p++;
    if (mb >= mt)
    {
        if (ml > l)
            p[0][0] = l, p[0][1] = mt, p[0][2] = ml - 1, p[0][3] = mb, p++;
        if (mr < rt)
            p[0][0] = mr + 1, p[0][1] = mt, p[0][2] = rt, p[0][3] = mb, p++;
    }
    while (p > r)
    {
        WORD *q = *--p;

        /* in the order above (the last first: the same pixels) */
        zune_draw_background(obj, q[0], q[1], q[2] - q[0] + 1,
            q[3] - q[1] + 1, q[0], q[1], 0);
    }
}

/* A virtual group's draw before its children (Group__MUIM_Draw): the
 * scroll moves the pixels there are, then Area draws the frame, and the
 * contents are painted inside the paint pair. 1 to draw the children; 0
 * when a scroll has nothing to move (the draw ends there, TRUE); -1 when
 * the subclass declines to paint (the scroll is then done, FALSE). */
static __attribute__((noinline)) LONG group_virtual_draw(struct IClass *cl,
    Object *obj, struct MUIP_Draw *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct GroupVirtual *v = VX(data);
    ULONG fill = _flags(obj) & MADF_FILLAREA;

    /* Move existing pixels before BeginPaint lets a subclass restrict the
     * drawing region. Layout hooks must retain their previous scroll state
     * until that callback computes the newly exposed content. */
    if (data->flags & GROUP_UPDATE_SCROLL)
    {
        LONG dx = v->virt_offx - v->old_virt_offx;
        LONG dy = v->virt_offy - v->old_virt_offy;
        struct Rectangle *bounds = &muiRenderInfo(obj)->mri_ClipRect;
        LONG left = MAX(_mleft(obj), bounds->MinX);
        LONG top = MAX(_mtop(obj), bounds->MinY);
        LONG right = MIN(_mright(obj), bounds->MaxX);
        LONG bottom = MIN(_mbottom(obj), bounds->MaxY);

        if ((!dx && !dy) || right < left || bottom < top)
            return 0;
        ScrollWindowRaster(_window(obj), dx, dy, left, top, right, bottom);
    }

    _flags(obj) &= ~MADF_FILLAREA;
    /* The spacing between the frame and the contents is not part of
     * the paint pair, so clear it before Area puts the frame on top;
     * a page switch otherwise left the previous page's pixels there
     * (xSysInfo's Hardware page). Area moves the draw flags into the
     * message only below. */
    if ((_flags(obj) & MADF_DRAWOBJECT) && !(_flags(obj) & MADF_DRAWUPDATE))
        group_clear_ring(obj);
    if (muiGlobalInfo(obj)->mgi_Prefs->window_redraw
        == WINDOW_REDRAW_WITHOUT_CLEAR)
        group_draw_frame_and_gaps(cl, obj, msg);
    else
        DoSuperMethodA(cl, obj, (Msg) msg);
    _flags(obj) |= fill;

    if (!DoMethod(obj, MUIM_Virtgroup_BeginPaint))
    {
        /* The raster has already moved, even if the client declines
         * painting. Never apply that same scroll delta twice. */
        v->old_virt_offx = v->virt_offx;
        v->old_virt_offy = v->virt_offy;
        GROUP_SET_UPDATE(data, 0);
        return -1;
    }
    /* Custom backfills receive the full logical viewport. The exposed
     * region limits actual painting, including on partial scrolls. */
    if ((msg->flags & MADF_DRAWOBJECT) && !(msg->flags & MADF_DRAWUPDATE))
        zune_draw_background(obj, _mleft(obj), _mtop(obj), _mwidth(obj),
            _mheight(obj), _mleft(obj), _mtop(obj), 0);
    return 1;
}

/*
 * Draw Area and children, with separate page-switch and virtual-scroll
 * update paths. Return FALSE during InitChange, otherwise normally TRUE;
 * ignore child draw results. Requires drawable render state and stable
 * membership.
 */
IPTR Group__MUIM_Draw(struct IClass *cl, Object *obj,
    struct MUIP_Draw *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct Rectangle group_rect;
    struct MinNode *n;
    Object *child, *title;
    LONG page;

    if (data->flags & GROUP_CHANGING)
        return FALSE;

    if (data->flags & GROUP_VIRTUAL)
    {
        LONG go = group_virtual_draw(cl, obj, msg);

        if (go <= 0)
            return go == 0;
    }
    else
    {
        ULONG fill = _flags(obj) & MADF_FILLAREA;

        /* MUI 3.8 and MUI 5 paint a group's gaps whatever its
         * MUIA_FillArea (FALSE only keeps the box under a child that
         * paints nothing: tests/lean/probe/bgfill.c); UP-MCC's panels
         * have FALSE and show their face that way */
        if (!(muiAreaData(obj)->mad_Flags2 & MADF2_CUSTOMBACKFILL))
            _flags(obj) |= MADF_FILLAREA;
        if (muiGlobalInfo(obj)->mgi_Prefs->window_redraw
            == WINDOW_REDRAW_WITHOUT_CLEAR)
            group_draw_frame_and_gaps(cl, obj, msg);
        else
            DoSuperMethodA(cl, obj, (Msg) msg);
        _flags(obj) = (_flags(obj) & ~MADF_FILLAREA) | fill;
    }

    if ((msg->flags & MADF_DRAWUPDATE) && (data->flags & GROUP_UPDATE_PAGE))
        group_draw_page_background(cl, obj);
    else if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWALL)))
        goto finish;

    title = group_title(data);
    group_rect = muiRenderInfo(obj)->mri_ClipRect;
    page = data->active_page + 1;
    FOR_CHILD(&data->childlist, n, child)
    {
        if (!(_flags(child) & MADF_SHOWME))
            continue;
        if (child != title)
        {
            /* not the active page */
            if ((data->flags & GROUP_PAGEMODE) && --page)
                continue;
        }
        /* Do not issue a re-draw to title group during page switch:
         * it redraws itself through its MUIA_Group_ActivePage */
        else if ((data->flags & GROUP_PAGEMODE)
            && (msg->flags & MADF_DRAWUPDATE)
            && (data->flags & GROUP_UPDATE_PAGE))
            continue;
        /* a window border scroller (Scrollgroup's UseWinBorder) is shown
         * but has no place in the group: intuition draws it */
        if (_flags(child) & (MADF_DISAPPEARED | MADF_BORDERGADGET))
            continue;
        MUI_Redraw(child, MADF_DRAWOBJECT | MADF_DRAWFRAME);
        muiRenderInfo(obj)->mri_ClipRect = group_rect;
    }

finish:
    if (data->flags & GROUP_VIRTUAL)
    {
        DoMethod(obj, MUIM_Virtgroup_EndPaint);
        VX(data)->old_virt_offx = VX(data)->virt_offx;
        VX(data)->old_virt_offy = VX(data)->virt_offy;
    }
    GROUP_SET_UPDATE(data, 0);
    return TRUE;
}

/**************************************************************************
 MUIM_AskMinMax
**************************************************************************/

/* Normalize t (maxima at least the minima, defaults within) and add it to
 * the group's own (Area's frame and spacing) */
static void group_add_minmax(struct MUI_MinMax *mi, struct MUI_MinMax *t)
{
    t->MaxHeight = MAX(t->MaxHeight, t->MinHeight);
    t->MaxWidth = MAX(t->MaxWidth, t->MinWidth);
    t->DefHeight = CLAMP(t->DefHeight, t->MinHeight, t->MaxHeight);
    t->DefWidth = CLAMP(t->DefWidth, t->MinWidth, t->MaxWidth);
    mi->MinWidth += t->MinWidth;
    mi->MinHeight += t->MinHeight;
    mi->MaxWidth += t->MaxWidth;
    mi->MaxHeight += t->MaxHeight;
    mi->DefWidth += t->DefWidth;
    mi->DefHeight += t->DefHeight;
}

/* A horizontal (a 0) or vertical (a 1) group: its children along the axis,
 * with the spacing between them; across it the largest minimum and
 * default and the smallest maximum. A child of weight 0 counts its
 * minimum as its maximum along the axis. The weight sum is kept for the
 * layout. The two directions differ where MUI 3.8 does (measured): a
 * horizontal group adds its children's default widths, also of weight 0,
 * and SameWidth raises the minimum it counts; a vertical group adds the
 * minimum height of a child of weight 0, and SameHeight sets the
 * children's minimum heights. */
static void group_minmax_1d(struct MUI_GroupData *data,
    struct MinList *children, struct MUI_MinMax *mi)
{
    struct MUI_MinMax tmp;
    WORD *t = &tmp.MinWidth;    /* t[a]: min, t[a + 2]: max, t[a + 4]: def */
    struct MinNode *n;
    Object *child;
    LONG a = (data->flags & GROUP_HORIZ) ? 0 : 1, b = 1 - a;
    WORD sp = (&data->horiz_spacing)[a], maxmin = 0;
    UWORD nvis = data->num_children;
    ULONG wsum = 0;
    BOOL same = (data->flags & (GROUP_SAME_WIDTH << a)) != 0;
    BOOL counted = FALSE, crossweight = FALSE;

    FOR_CHILD(children, n, child)
    {
        if (IS_HIDDEN(child))
            nvis--;
        else if (same)
            maxmin = MAX(maxmin, AXP(child, a)[AX_MIN]);
    }

    t[b] = t[b + 4] = 0;
    t[b + 2] = MUI_MAXMAX;
    if (nvis > 0)
    {
        t[a] = t[a + 4] = (nvis - 1) * sp;
        /* MUI 3.9 adds spacing and each child's maximum, itself at most
         * MUI_MAXMAX, until the sum reaches MUI_MAXMAX, without clamping
         * the sum (tests/native/demolayout.c, widgetsizes.c) */
        t[a + 2] = -sp;
    }
    else
    {
        t[a] = t[a + 4] = 0;
        t[a + 2] = MUI_MAXMAX;
    }

    FOR_CHILD(children, n, child)
    {
        WORD *m = AXP(child, a), *c = AXP(child, b), min;

        if (IS_HIDDEN(child))
            continue;
        if (!a)
            min = same ? MIN(MAX(maxmin, m[AX_MIN]), m[AX_MAX]) : m[AX_MIN];
        else
        {
            if (same)
                m[AX_MIN] = MIN(maxmin,
                    m[AX_WEIGHT] ? m[AX_MAX] : m[AX_MIN]);
            min = m[AX_MIN];
        }
        /* MUI 3.9 leaves children that may disappear, and their spacing,
         * out of the minimum and default (tests/native/disappear.c) */
        if (m[AX_DISAPPEAR] > 0)
        {
            t[a] -= sp;
            t[a + 4] -= sp;
        }
        else
        {
            /* a child whose own children may all disappear has a
             * negative minimum; MUI 3.9 counts it as 0 here */
            t[a] += MAX(min, 0);
            /* MUI 3.8 counts the default width of a child of horizontal
             * weight 0 too (measured: a string of weight 0 beside a
             * button, 112 + 6 + 32), although the child stays at its
             * minimum in the layout; a vertical group counts the
             * minimum height of a child of vertical weight 0 */
            min = (!a || m[AX_WEIGHT]) ? m[AX_DEF] : m[AX_MIN];
            t[a + 4] += MAX(min, 0);
        }
        if (t[a + 2] < MUI_MAXMAX)
            t[a + 2] += sp + MIN(m[AX_WEIGHT] ? m[AX_MAX] : m[AX_MIN],
                MUI_MAXMAX);
        t[b] = MAX(t[b], c[AX_MIN]);
        t[b + 4] = MAX(t[b + 4], c[AX_DEF]);
        /* if all children have null weight then maxheight=minheight; if
         * some do, the maxheight is the min of all maxheights */
        t[b + 2] = MIN(t[b + 2], c[AX_MAX]);
        wsum += (UWORD)m[AX_WEIGHT];
        counted = TRUE;
        if ((UWORD)c[AX_WEIGHT] > 0)
            crossweight = TRUE;
    }
    /* an empty group keeps its full maximum, so MUIA_FixWidth and
     * MUIA_FixHeight size it, as in MUI 3.9 (tests/native/emptyfixprobe.c) */
    if (counted && !crossweight)
        t[b + 2] = t[b];
    data->weight_sum = wsum;
    group_add_minmax(mi, &tmp);
}

/* The rows (row-major order) and columns of a grid. SameHeight/SameWidth
 * adjust child minima/defaults subject to zero-weight maxima. A row or
 * column is weighed by its children's average weight (MUI 3.9). */
static void minmax_2d_rows_pass(struct GroupExtra *x,
    struct MinList *children, struct MUI_MinMax *req, BOOL same,
    WORD maxmin_height, WORD maxdef_height)
{
    struct MinNode *n = children->mlh_Head;
    struct layout2d_elem *row = x->row_infos;
    int i;

    /* do not rewind after the while, to process line by line */
    for (i = 0; i < x->rows; i++, row++)
    {
        /* calculate min and max height of this row */
        int min_h = 0, def_h = 0, max_h = MUI_MAXMAX, j = 0;
        BOOL found_nonzero_vweight = FALSE;

        row->weight = 0;
        while (n->mln_Succ)
        {
            Object *child = BASEOBJECT(n);

            n = n->mln_Succ;
            if (IS_HIDDEN(child))
                continue;
            if (same)
            {
                WORD w0max = _vweight(child) ? _maxheight(child)
                    : _minheight(child);

                _minheight(child) = MIN(maxmin_height, w0max);
                w0max = _vweight(child) ? _maxheight(child)
                    : _minheight(child);
                _defheight(child) = MIN(maxdef_height, w0max);
            }
            min_h = MAX(min_h, _minheight(child));
            def_h = MAX(def_h, _vweight(child) ? _defheight(child)
                : _minheight(child));
            max_h = MIN(max_h, _maxheight(child));
            if (_vweight(child) > 0)
            {
                found_nonzero_vweight = TRUE;
                row->weight += _vweight(child);
            }
            if ((++j % x->columns) == 0)
                break;
        }
        if (!found_nonzero_vweight)
            max_h = min_h;
        else
            max_h = MAX(max_h, min_h);
        if (x->columns > 0)
            row->weight /= x->columns;
        row->min = min_h;
        row->max = max_h;
        x->vert_weight_sum += row->weight;
        req->MinHeight += min_h;
        req->DefHeight += def_h;
        req->MaxHeight += max_h;
        if (req->MaxHeight > MUI_MAXMAX)
            req->MaxHeight = MUI_MAXMAX;
    }
}

static void minmax_2d_columns_pass(struct GroupExtra *x,
    struct MinList *children, struct MUI_MinMax *req, BOOL same,
    WORD maxmin_width, WORD maxdef_width)
{
    struct layout2d_elem *col = x->col_infos;
    int i;

    for (i = 0; i < x->columns; i++, col++)
    {
        /* calculate min and max width of this column */
        int min_w = 0, def_w = 0, max_w = MUI_MAXMAX, j = 0;
        BOOL found_nonzero_hweight = FALSE;
        struct MinNode *n;
        Object *child;

        col->weight = 0;
        /* process all children to get children on a column */
        FOR_CHILD(children, n, child)
        {
            if (IS_HIDDEN(child))
                continue;
            if ((j++ % x->columns) != i)
                continue;
            if (same)
            {
                WORD w0max = _hweight(child) ? _maxwidth(child)
                    : _minwidth(child);

                _minwidth(child) = MIN(maxmin_width, w0max);
                w0max = _hweight(child) ? _maxwidth(child)
                    : _minwidth(child);
                _defwidth(child) = MIN(maxdef_width, w0max);
            }
            min_w = MAX(min_w, _minwidth(child));
            def_w = MAX(def_w, _hweight(child) ? _defwidth(child)
                : _minwidth(child));
            /* null weight children limit the max size if they're
             * alone, but not if they are with non-null weight ones */
            max_w = MIN(max_w, _maxwidth(child));
            if (_hweight(child) > 0)
            {
                found_nonzero_hweight = TRUE;
                col->weight += _hweight(child);
            }
        }
        if (!found_nonzero_hweight)
            max_w = min_w;
        else
            max_w = MAX(max_w, min_w);
        /* MUI 3.9 weighs a column by its children's average weight */
        if (x->rows > 0)
            col->weight /= x->rows;
        col->min = min_w;
        col->max = max_w;
        x->horiz_weight_sum += col->weight;
        req->MinWidth += min_w;
        req->DefWidth += def_w;
        req->MaxWidth += max_w;
        if (req->MaxWidth > MUI_MAXMAX)
            req->MaxWidth = MUI_MAXMAX;
    }
}

/*
 * A grid: its dimensions from the total child count, the last row filled
 * as far as the children go (MUI 3.8; MultiRen: 26 objects in 4 columns,
 * 7 rows). Given rows become the columns they need, then the rows those
 * columns need, so that every pass agrees. Without memory for the arrays
 * nothing is added and the layout does nothing.
 */
static void group_minmax_2d(struct MUI_GroupData *data,
    struct MinList *children, struct MUI_MinMax *mi)
{
    struct GroupExtra *x = data->x;
    struct MinNode *n;
    Object *child;
    struct MUI_MinMax tmp;
    WORD maxmin_width = 0, maxmin_height = 0;
    WORD maxdef_width = 0, maxdef_height = 0;

    if (!x->columns)
        x->columns = (data->num_children + x->rows - 1) / x->rows;
    if (x->columns < 1)
        x->columns = 1;
    x->rows = (data->num_children + x->columns - 1) / x->columns;
    if (x->rows < 1)
        x->rows = 1;

    if (!group_infos(&x->row_infos, &x->row_cap, x->rows)
        || !group_infos(&x->col_infos, &x->col_cap, x->columns))
        return;

    x->horiz_weight_sum = 0;
    x->vert_weight_sum = 0;

    tmp.MinHeight = tmp.DefHeight = tmp.MaxHeight =
        (x->rows - 1) * data->vert_spacing;
    tmp.MinWidth = tmp.DefWidth = tmp.MaxWidth =
        (x->columns - 1) * data->horiz_spacing;

    /* get minimum dims if same dims for all children are needed */
    if (data->flags & (GROUP_SAME_WIDTH | GROUP_SAME_HEIGHT))
        FOR_CHILD(children, n, child)
        {
            if (!(_flags(child) & MADF_SHOWME))
                continue;
            maxmin_width = MAX(maxmin_width, _minwidth(child));
            maxmin_height = MAX(maxmin_height, _minheight(child));
            maxdef_width = MAX(maxdef_width, _hweight(child)
                ? _defwidth(child) : _minwidth(child));
            maxdef_height = MAX(maxdef_height, _vweight(child)
                ? _defheight(child) : _minheight(child));
        }

    minmax_2d_rows_pass(x, children, &tmp,
        (data->flags & GROUP_SAME_HEIGHT) != 0, maxmin_height, maxdef_height);
    minmax_2d_columns_pass(x, children, &tmp,
        (data->flags & GROUP_SAME_WIDTH) != 0, maxmin_width, maxdef_width);
    group_add_minmax(mi, &tmp);
}

/*
 * A page group: the ShowMe pages' limits combined (MUI 3.9: it may grow as
 * far as its largest page may, so a page of fixed height does not cap the
 * others, xSysInfo), the title group's minimum height added above, its
 * width only as another page's.
 */
static void group_minmax_pagemode(struct MUI_GroupData *data,
    struct MinList *children, struct MUI_MinMax *mi)
{
    Object *title = group_title(data), *child;
    struct MinNode *n;
    struct MUI_MinMax tmp = { 0, 0, 0, 0, 0, 0 };

    FOR_CHILD(children, n, child)
    {
        WORD w0;

        if (!(_flags(child) & MADF_SHOWME) || child == title)
            continue;
        tmp.MinHeight = MAX(tmp.MinHeight, _minheight(child));
        tmp.MinWidth = MAX(tmp.MinWidth, _minwidth(child));
        tmp.MaxHeight = MAX(tmp.MaxHeight,
            _vweight(child) ? _maxheight(child) : _minheight(child));
        tmp.MaxWidth = MAX(tmp.MaxWidth,
            _hweight(child) ? _maxwidth(child) : _minwidth(child));
        w0 = _vweight(child) ? _defheight(child) : _minheight(child);
        if (w0 < MUI_MAXMAX)
            tmp.DefHeight = MAX(tmp.DefHeight, w0);
        w0 = _hweight(child) ? _defwidth(child) : _minwidth(child);
        if (w0 < MUI_MAXMAX)
            tmp.DefWidth = MAX(tmp.DefWidth, w0);
    }

    if (tmp.MaxWidth == 0 && tmp.MaxHeight == 0)
        tmp.MaxWidth = tmp.MaxHeight = MUI_MAXMAX;

    if (title)
    {
        tmp.MinWidth = MAX(tmp.MinWidth, _minwidth(title));
        tmp.DefWidth = MAX(tmp.DefWidth, _defwidth(title));
        tmp.MaxWidth = MAX(tmp.MaxWidth, _maxwidth(title));
        tmp.MinHeight += _minheight(title);
        tmp.MaxHeight += _vweight(title) ? _maxheight(title)
            : _minheight(title);
        tmp.DefHeight += _vweight(title) ? _defheight(title)
            : _minheight(title);
    }
    group_add_minmax(mi, &tmp);
}

/* A layout hook's MUILM_MINMAX: what it fills (maxima and defaults raised
 * to the minima) added to the group's own, maxima capped at MUI_MAXMAX.
 * Its own function: the message is on the stack only here. */
static __attribute__((noinline)) void group_minmax_hook(Object *obj,
    struct MUI_GroupData *data, struct MUI_MinMax *mi)
{
    struct MUI_LayoutMsg lm;

    lm.lm_Children = &data->childlist;
    lm.lm_Type = MUILM_MINMAX;
    CallHookPkt(data->layout_hook, obj, &lm);

    if (lm.lm_MinMax.MaxHeight < lm.lm_MinMax.MinHeight)
        lm.lm_MinMax.MaxHeight = lm.lm_MinMax.MinHeight;
    if (lm.lm_MinMax.DefHeight < lm.lm_MinMax.MinHeight)
        lm.lm_MinMax.DefHeight = lm.lm_MinMax.MinHeight;
    if (lm.lm_MinMax.MaxWidth < lm.lm_MinMax.MinWidth)
        lm.lm_MinMax.MaxWidth = lm.lm_MinMax.MinWidth;
    if (lm.lm_MinMax.DefWidth < lm.lm_MinMax.MinWidth)
        lm.lm_MinMax.DefWidth = lm.lm_MinMax.MinWidth;

    mi->MinWidth += lm.lm_MinMax.MinWidth;
    mi->MinHeight += lm.lm_MinMax.MinHeight;
    mi->MaxWidth += lm.lm_MinMax.MaxWidth;
    if (mi->MaxWidth > MUI_MAXMAX)
        mi->MaxWidth = MUI_MAXMAX;
    mi->MaxHeight += lm.lm_MinMax.MaxHeight;
    if (mi->MaxHeight > MUI_MAXMAX)
        mi->MaxHeight = MUI_MAXMAX;
    mi->DefWidth += lm.lm_MinMax.DefWidth;
    mi->DefHeight += lm.lm_MinMax.DefHeight;
}

/* Ask the children (those shown, every one for a layout hook: custom
 * layout hooks see every child, as in MUI 3.8/3.9, so hidden children
 * need valid dimensions too) and finish their sizes; every child counts
 * again (the layout decides which disappear). Its own function: the
 * message and the child's minmax are on the stack only here, not on every
 * nesting level of the groups' AskMinMax. */
static __attribute__((noinline)) void group_ask_children(
    struct MinList *children, ULONG id, BOOL all)
{
    struct MUIP_AskMinMax m;
    struct MUI_MinMax mm;
    struct MinNode *n;
    Object *child;

    m.MethodID = id;
    FOR_CHILD(children, n, child)
    {
        _flags(child) &= ~MADF_DISAPPEARED;
        if (!(_flags(child) & MADF_SHOWME) && !all)
            continue;
        /* Area points the message at the child's own minmax */
        m.MinMaxInfo = &mm;
        DoMethodA(child, (Msg) &m);
        __area_finish_minmax(child, m.MinMaxInfo);
    }
}

/*
 * Ask Area and the children, then select page, custom Hook or built-in
 * constraints. Hook must fill all six fields of its otherwise uninitialized
 * min/max message; its return is ignored. Virtual groups save full minima
 * but expose Area minima plus eight. Return zero.
 */
IPTR Group__MUIM_AskMinMax(struct IClass *cl, Object *obj,
    struct MUIP_AskMinMax *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MUI_MinMax *mi;
    WORD super_minwidth, super_minheight;

    /* our superclass first fills in its size with frame, inner spacing
     * etc. (and points the message at the group's own minmax) */
    DoSuperMethodA(cl, obj, (Msg) msg);
    mi = msg->MinMaxInfo;
    /* kept here, not in the virtual state: a layout hook may get
     * MUIA_Virtgroup_MinWidth/Height during MINMAX and sees the last
     * minimum, as before the rewrite */
    super_minwidth = mi->MinWidth;
    super_minheight = mi->MinHeight;

    group_ask_children(&data->childlist, msg->MethodID,
        data->layout_hook != NULL);

    if (data->flags & GROUP_PAGEMODE)
        group_minmax_pagemode(data, &data->childlist, mi);
    else if (data->layout_hook)
        group_minmax_hook(obj, data, mi);
    else if (!group_grid(data))
        group_minmax_1d(data, &data->childlist, mi);
    else
        group_minmax_2d(data, &data->childlist, mi);

    if (data->flags & GROUP_VIRTUAL)
    {
        /* MUI 3.8/3.9 keep eight pixels of virtual content visible */
        VX(data)->saved_minwidth = mi->MinWidth;
        VX(data)->saved_minheight = mi->MinHeight;
        mi->MinWidth = super_minwidth + 8;
        mi->MinHeight = super_minheight + 8;
    }
    return 0;
}

/**************************************************************************
 Layout
**************************************************************************/

/* Clamp one size to its bounds while adjusting the remainder, the
 * grow/shrink sizes and weight totals. Same-size zero-weight handling
 * precedes clamping. */
static void Layout1D_minmax_constraint(WORD *sizep, WORD minsize,
    WORD maxsize, WORD *remainp, WORD *sizegrowp, WORD *sizeshrinkp,
    ULONG *weightgrowp, ULONG *weightshrinkp, UWORD weight, WORD samesize)
{
    WORD size = *sizep, remain = *remainp,
        sizegrow = *sizegrowp, sizeshrink = *sizeshrinkp;
    ULONG weightgrow = *weightgrowp, weightshrink = *weightshrinkp;
    BOOL samefix = (samesize > 0) && (weight == 0);

    if (samefix)
    {
        remain += size - samesize;
        size = samesize;
        sizeshrink -= size;
        sizegrow -= size;
    }

    if (size <= minsize)        // too little
    {
        remain += size - minsize;
        size = minsize;
        sizeshrink -= size;
        if (size == maxsize)
            sizegrow -= size;
    }
    else if (size >= maxsize)   // too big
    {
        remain += size - maxsize;
        size = maxsize;
        sizegrow -= size;
    }

    if (!samefix)
    {
        if (size < maxsize)
            weightgrow += weight;
        if (size > minsize)
            weightshrink += weight;
    }

    *sizep = size;
    *remainp = remain;
    *sizegrowp = sizegrow;
    *sizeshrinkp = sizeshrink;
    *weightgrowp = weightgrow;
    *weightshrinkp = weightshrink;
}

/* Redistribute the remaining space to one weighted child (rounded down
 * like MUI 3.9; the last child takes the rest), updating size and all
 * running totals. Zero weight returns without change. */
static void Layout1D_redistribution(WORD *sizep, WORD minsize,
    WORD maxsize, WORD *remainp, WORD *sizegrowp, WORD *sizeshrinkp,
    ULONG *weightgrowp, ULONG *weightshrinkp, UWORD weight)
{
    WORD size = *sizep, remain = *remainp,
        sizegrow = *sizegrowp, sizeshrink = *sizeshrinkp;
    ULONG weightgrow = *weightgrowp, weightshrink = *weightshrinkp;

    if (weight == 0)
        return;

    if ((remain > 0) && (size < maxsize))
    {
        LONG newsize = (sizegrow * weight) / weightgrow;

        /* off-by-1 errors that may toggle the remainder's sign: converge
         * to 0 */
        if (remain - newsize + size < 0)
        {
            size += remain;
            remain = 0;
        }
        else
        {
            remain -= newsize - size;
            size = newsize;
            sizegrow -= size;
            weightgrow -= weight;
        }
    }
    else if ((remain < 0) && (size > minsize))
    {
        LONG newsize = (sizeshrink * weight) / weightshrink;

        if (remain - newsize + size > 0)
        {
            size += remain;
            remain = 0;
        }
        else
        {
            remain -= newsize - size;
            size = newsize;
            sizeshrink -= size;
            weightshrink -= weight;
        }
    }

    *sizep = size;
    *remainp = remain;
    *sizegrowp = sizegrow;
    *sizeshrinkp = sizeshrink;
    *weightgrowp = weightgrow;
    *weightshrinkp = weightshrink;
}

/* Final settling pass: the two redistribution passes above can leave a
 * child outside its min/max range while others absorb the difference.
 * Clamp every child, then share any remainder by weight among children
 * that can still move, until no space is left or nothing can move. */
static void Layout1D_settle(struct MinList *children, WORD remainder,
    LONG a)
{
    struct MinNode *n;
    Object *child;
    int round;

    for (round = 0; round < 32; round++)
    {
        ULONG weights = 0, movable = 0;
        LONG share;

        FOR_CHILD(children, n, child)
        {
            WORD *m = AXP(child, a);

            if (IS_HIDDEN(child))
                continue;
            if (m[AX_SIZE] < m[AX_MIN])
            {
                remainder -= m[AX_MIN] - m[AX_SIZE];
                m[AX_SIZE] = m[AX_MIN];
            }
            else if (m[AX_SIZE] > m[AX_MAX])
            {
                remainder += m[AX_SIZE] - m[AX_MAX];
                m[AX_SIZE] = m[AX_MAX];
            }
        }
        if (remainder == 0)
            return;

        FOR_CHILD(children, n, child)
        {
            WORD *m = AXP(child, a);

            if (IS_HIDDEN(child))
                continue;
            if ((remainder > 0 && m[AX_SIZE] < m[AX_MAX])
                || (remainder < 0 && m[AX_SIZE] > m[AX_MIN]))
            {
                movable++;
                weights += (UWORD)m[AX_WEIGHT];
            }
        }
        if (movable == 0)
            return;

        share = remainder;
        for (n = children->mlh_Head; n->mln_Succ && remainder != 0;
            n = n->mln_Succ)
        {
            WORD *m;
            LONG delta, weight;

            child = BASEOBJECT(n);
            if (IS_HIDDEN(child))
                continue;
            m = AXP(child, a);
            if (!((remainder > 0 && m[AX_SIZE] < m[AX_MAX])
                    || (remainder < 0 && m[AX_SIZE] > m[AX_MIN])))
                continue;
            weight = (UWORD)m[AX_WEIGHT];
            delta = weights ? share * weight / (LONG)weights
                : share / (LONG)movable;
            if (delta == 0)
                delta = remainder > 0 ? 1 : -1;
            if ((delta > 0 && delta > remainder)
                || (delta < 0 && delta < remainder))
                delta = remainder;
            if (m[AX_SIZE] + delta > m[AX_MAX])
                delta = m[AX_MAX] - m[AX_SIZE];
            if (m[AX_SIZE] + delta < m[AX_MIN])
                delta = m[AX_MIN] - m[AX_SIZE];
            m[AX_SIZE] += delta;
            remainder -= delta;
        }
    }
}

/* At most two clamp/redistribution passes over the visible children
 * (along axis a), then the settling pass */
static __attribute__((noinline)) void Layout1D_minmax_constraints_and_redistrib(
    struct MinList *children, WORD total_size, WORD remainder,
    WORD samesize, LONG a)
{
    struct MinNode *n;
    Object *child;
    int i;

    for (i = 0; i < 2; i++)
    {
        WORD size_growables = total_size;
        WORD size_shrinkables = total_size;
        ULONG weight_growables = 0;
        ULONG weight_shrinkables = 0;

        FOR_CHILD(children, n, child)
        {
            WORD *m = AXP(child, a);

            if (IS_HIDDEN(child))
                continue;
            Layout1D_minmax_constraint(&m[AX_SIZE], m[AX_MIN], m[AX_MAX],
                &remainder, &size_growables, &size_shrinkables,
                &weight_growables, &weight_shrinkables, m[AX_WEIGHT],
                samesize);
        }
        // mid-pass break
        if (remainder == 0)
            break;
        // distribute remaining space to possible candidates
        for (n = children->mlh_Head; n->mln_Succ && remainder != 0;
            n = n->mln_Succ)
        {
            WORD *m;

            child = BASEOBJECT(n);
            if (IS_HIDDEN(child))
                continue;
            m = AXP(child, a);
            Layout1D_redistribution(&m[AX_SIZE], m[AX_MIN], m[AX_MAX],
                &remainder, &size_growables, &size_shrinkables,
                &weight_growables, &weight_shrinkables, m[AX_WEIGHT]);
        }
        // dont break here if remainder == 0, some minmax constraints
        // may not be respected
    }
    Layout1D_settle(children, remainder, a);
}

/* The rest of Layout1D_mui_redistrib's remainder, a pixel at a time to
 * the movable children from the last one back, weighted children first */
static __attribute__((noinline)) void Layout1D_rest(struct MinList *children,
    LONG remainder, LONG a)
{
    struct MinNode *n;
    Object *child;

    while (remainder != 0)
    {
        BOOL moved = FALSE;
        int round;

        for (round = 0; round < 2 && remainder != 0 && !moved; round++)
            for (n = children->mlh_TailPred; n->mln_Pred && remainder != 0;
                n = n->mln_Pred)
            {
                WORD *m;

                child = BASEOBJECT(n);
                if (IS_HIDDEN(child))
                    continue;
                m = AXP(child, a);
                if (round == 0 && m[AX_WEIGHT] == 0)
                    continue;
                if (remainder > 0 && m[AX_SIZE] < m[AX_MAX])
                {
                    ++m[AX_SIZE];
                    --remainder;
                    moved = TRUE;
                }
                else if (remainder < 0 && m[AX_SIZE] > m[AX_MIN])
                {
                    --m[AX_SIZE];
                    ++remainder;
                    moved = TRUE;
                }
            }
        if (!moved)
            return;
    }
}

/* One of Layout1D_mui_redistrib's passes: each child that can still move
 * gets its weight's share of the remainder (over all children's weight,
 * truncated). TRUE when one moved. */
static __attribute__((noinline)) BOOL Layout1D_share(struct MinList *children,
    LONG remainder, ULONG total_weight, LONG a)
{
    struct MinNode *n;
    Object *child;
    BOOL moved = FALSE;

    FOR_CHILD(children, n, child)
    {
        WORD *m = AXP(child, a);
        LONG delta;

        if (IS_HIDDEN(child))
            continue;
        if ((remainder > 0 && m[AX_SIZE] >= m[AX_MAX])
            || (remainder < 0 && m[AX_SIZE] <= m[AX_MIN]))
            continue;
        delta = remainder * (LONG)(UWORD)m[AX_WEIGHT] / (LONG)total_weight;
        if (delta)
        {
            m[AX_SIZE] += delta;
            moved = TRUE;
        }
    }
    return moved;
}

/* MUI 3.9's redistribution (tests/native/hdist.c): after the weighted
 * first split, each pass clamps every child to its range and then adds
 * to each child that can still move its weight's share of that pass's
 * remainder, taken from the weight of all children and truncated, so
 * clamped children keep their weight in the divisor. After eight
 * passes the rest goes one pixel at a time to the movable children from
 * the last one back, weighted children first. */
static __attribute__((noinline)) void Layout1D_mui_redistrib(
    struct MinList *children, WORD total_size, ULONG total_weight, LONG a)
{
    struct MinNode *n;
    Object *child;
    LONG remainder;
    int pass;

    for (pass = 0; pass <= 8; pass++)
    {
        remainder = total_size;
        FOR_CHILD(children, n, child)
        {
            WORD *m = AXP(child, a);

            if (IS_HIDDEN(child))
                continue;
            m[AX_SIZE] = CLAMP(m[AX_SIZE], m[AX_MIN], m[AX_MAX]);
            remainder -= m[AX_SIZE];
        }
        if (remainder == 0)
            return;
        if (pass == 8)
            break;

        if (!Layout1D_share(children, remainder, total_weight, a))
            break;
    }

    Layout1D_rest(children, remainder, a);
}

/* MUIA_Horiz/VertDisappear, like MUI 3.9 (tests/native/disappear.c):
 * while the minimum sizes of the remaining children do not fit, all
 * children of the lowest disappear level still present disappear. They
 * get their minimum size and are hidden (like MUI 3.9, the batch keeps
 * boxes at its minimum size, one after the other behind the children
 * still present); the others are laid out, and only they are counted and
 * weighed. */
static __attribute__((noinline)) void group_disappear(
    struct MUI_GroupData *data, struct MinList *children, LONG a,
    WORD available)
{
    WORD spacing = (&data->horiz_spacing)[a];
    BOOL any = FALSE;
    struct MinNode *n;
    Object *child;
    ULONG wsum = 0;

    FOR_CHILD(children, n, child)
    {
        _flags(child) &= ~MADF_DISAPPEARED;
        if (AXP(child, a)[AX_DISAPPEAR] > 0)
            any = TRUE;
    }
    if (!any)
        return;

    for (;;)
    {
        LONG need = 0, level = 0, pos;
        WORD count = 0;

        FOR_CHILD(children, n, child)
        {
            WORD *m = AXP(child, a);

            if (IS_HIDDEN(child))
                continue;
            need += m[AX_MIN];
            count++;
            if (m[AX_DISAPPEAR] > 0 && (level == 0 || m[AX_DISAPPEAR] < level))
                level = m[AX_DISAPPEAR];
        }
        if (count > 0)
            need += (count - 1) * spacing;
        if (need <= available || level == 0)
            break;
        pos = 0;
        FOR_CHILD(children, n, child)
        {
            WORD *m = AXP(child, a);

            if (IS_HIDDEN(child))
                continue;
            if (m[AX_DISAPPEAR] == level)
            {
                _flags(child) |= MADF_DISAPPEARED;
                if (_flags(child) & MADF_CANDRAW)
                    DoHideMethod(child);
                MUI_Layout(child, a ? 0 : pos, a ? pos : 0,
                    _minwidth(child), _minheight(child), 0);
            }
            pos += m[AX_MIN] + spacing;
        }
    }

    FOR_CHILD(children, n, child)
    {
        if (!IS_HIDDEN(child))
            wsum += (UWORD)AXP(child, a)[AX_WEIGHT];
    }
    data->weight_sum = wsum;
}

/* Show children that reappeared in a shown group, after their layout */
static __attribute__((noinline)) void group_reappear(Object *obj,
    struct MinList *children)
{
    struct MinNode *n;
    Object *child;

    if (!(_flags(obj) & MADF_CANDRAW))
        return;
    FOR_CHILD(children, n, child)
        if (!IS_HIDDEN(child) && !(_flags(child) & MADF_CANDRAW))
            DoShowMethod(child);
}

/* A virtual group's area: its inner box within the saved minimum and its
 * maximum; the box the layouts below fill */
static void group_virtual_area(Object *obj, struct GroupVirtual *v)
{
    v->virt_mwidth = CLAMP(_mwidth(obj), v->saved_minwidth - _subwidth(obj),
        _maxwidth(obj) - _subwidth(obj));
    v->virt_mheight = CLAMP(_mheight(obj),
        v->saved_minheight - _subheight(obj),
        _maxheight(obj) - _subheight(obj));
}

/* The sizes along axis a of a horizontal/vertical group's children, before
 * they are placed: each child's weight's share of what the spacing leaves
 * (its minimum without weights), then clamped and the rest shared out (by
 * MUI 3.9's rule, Layout1D_mui_redistrib, unless SameWidth/SameHeight
 * says otherwise or nothing has weight). Its own function, off the
 * stack while the children are laid out. */
static __attribute__((noinline)) void group_sizes_1d(
    struct MUI_GroupData *data, struct MinList *children, LONG a,
    WORD length)
{
    struct MinNode *n;
    Object *child;
    ULONG weight_sum = data->weight_sum, total_weight;
    WORD total_size, total_init_size = 0, remainder, samesize = 0;
    WORD left;
    UWORD nvis = 0;

    group_disappear(data, children, a, length);
    /* the children laid out (counted here, not kept from AskMinMax) */
    FOR_CHILD(children, n, child)
        if (!IS_HIDDEN(child))
            nvis++;
    total_weight = weight_sum;
    total_size = length - (nvis - 1) * (&data->horiz_spacing)[a];

    // weight constraints: the ideal size of each object, and their total
    left = total_size;
    FOR_CHILD(children, n, child)
    {
        WORD *m = AXP(child, a);
        UWORD weight = m[AX_WEIGHT];

        if (IS_HIDDEN(child))
            continue;
        if (total_weight > 0)
            m[AX_SIZE] = (left * weight) / total_weight;
        else
            m[AX_SIZE] = m[AX_MIN];
        total_weight -= weight;
        left -= m[AX_SIZE];
        total_init_size += m[AX_SIZE];
    }

    /* a horizontal group without weights has no remainder (as before) */
    if (a || weight_sum > 0)
        remainder = total_size - total_init_size;
    else
        remainder = 0;

    /* This is also true for non virtual groups, but if this would be the
     * case then there is a bug in the layout function */
    if ((data->flags & GROUP_VIRTUAL) && total_size < 0)
        total_size = 0;

    if (data->flags & (GROUP_SAME_WIDTH << a))
    {
        /* the largest minimum of the children AskMinMax counted */
        FOR_CHILD(children, n, child)
            if ((_flags(child) & (MADF_SHOWME | MADF_BORDERGADGET))
                == MADF_SHOWME)
                samesize = MAX(samesize, AXP(child, a)[AX_MIN]);
    }
    else if (weight_sum > 0)
    {
        Layout1D_mui_redistrib(children, total_size, weight_sum, a);
        return;
    }
    Layout1D_minmax_constraints_and_redistrib(children, total_size,
        remainder, samesize, a);
}

/*
 * A horizontal (a 0) or vertical (a 1) group: the children's sizes along
 * the axis, then each placed after the one before it with the spacing
 * between, across the axis centred at its size within its limits (the
 * maximum wins over a larger minimum, as in MUI 3.9,
 * tests/native/defclamp.c). A virtual group lays out its whole area.
 */
static void group_layout_1d(struct MUI_GroupData *data, Object *obj)
{
    struct MinList *children = &data->childlist;
    struct MinNode *n;
    Object *child;
    LONG a = (data->flags & GROUP_HORIZ) ? 0 : 1;
    WORD box[2], pos = 0;

    if (data->flags & GROUP_VIRTUAL)
    {
        group_virtual_area(obj, VX(data));
        box[0] = VX(data)->virt_mwidth;
        box[1] = VX(data)->virt_mheight;
    }
    else
    {
        box[0] = _mwidth(obj);
        box[1] = _mheight(obj);
    }

    group_sizes_1d(data, children, a, box[a]);

    // do the layout
    FOR_CHILD(children, n, child)
    {
        WORD *m, size, off;

        if (IS_HIDDEN(child))
            continue;
        m = AXP(child, 1 - a);
        size = MIN(MAX(box[1 - a], m[AX_MIN]), m[AX_MAX]);
        off = (box[1 - a] - size) / 2;
        m = AXP(child, a);
        if (!(a ? MUI_Layout(child, off, pos, size, m[AX_SIZE], 0)
            : MUI_Layout(child, pos, off, m[AX_SIZE], size, 0)))
            return;
        pos += (&data->horiz_spacing)[a] + m[AX_SIZE];
    }

    if (!(data->flags & GROUP_VIRTUAL))
        group_reappear(obj, children);
}

/*
 * Initialize row and column dimensions proportionally to the weights and
 * the available content space.
 */
static void Layout2D_weight_constraint(struct layout2d_elem *infos,
    WORD n, ULONG total_weight, WORD total_size, WORD *total_init)
{
    *total_init = 0;
    for (; n > 0; n--, infos++)
    {
        if (total_weight > 0)
            infos->dim = (total_size * infos->weight) / total_weight;
        else
            infos->dim = infos->min;
        total_weight -= infos->weight;
        total_size -= infos->dim;
        *total_init += infos->dim;
    }
}

/*
 * Run two passes over one axis' array, clamping dimensions and
 * redistributing the remainder with the 1D helpers.
 */
static void Layout2D_minmax_constraints_and_redistrib(struct layout2d_elem
    *infos, WORD nitems, WORD total_size, WORD samesize, WORD remainder)
{
    int j;

    for (j = 0; j < 2; j++)
    {
        WORD size_growables = total_size;
        WORD size_shrinkables = total_size;
        ULONG weight_growables = 0;
        ULONG weight_shrinkables = 0;
        int i;

        for (i = 0; i < nitems; i++)
            Layout1D_minmax_constraint(&infos[i].dim, infos[i].min,
                infos[i].max, &remainder, &size_growables,
                &size_shrinkables, &weight_growables, &weight_shrinkables,
                infos[i].weight, samesize);
        if (remainder == 0)
            break;
        for (i = 0; i < nitems; i++)
            Layout1D_redistribution(&infos[i].dim, infos[i].min,
                infos[i].max, &remainder, &size_growables,
                &size_shrinkables, &weight_growables, &weight_shrinkables,
                infos[i].weight);
    }
}

/*
 * A grid: the rows' heights and the columns' widths from their weights
 * and limits, then the children placed row-major, each within its limits
 * and centred in its cell. Nothing without valid dimensions and arrays.
 */
static void group_layout_2d(struct MUI_GroupData *data, Object *obj)
{
    struct GroupExtra *x = data->x;
    struct layout2d_elem *row, *col;
    struct MinNode *n;
    WORD layout_width, layout_height, total_h, total_w, init_h, init_w;
    LONG top = 0;
    int i;

    if (x->rows == 0 || x->columns == 0
        || data->num_children > x->rows * x->columns
        || x->row_infos == NULL || x->col_infos == NULL)
        return;

    if (data->flags & GROUP_VIRTUAL)
    {
        group_virtual_area(obj, VX(data));
        layout_width = VX(data)->virt_mwidth;
        layout_height = VX(data)->virt_mheight;
    }
    else
    {
        layout_width = _mwidth(obj);
        layout_height = _mheight(obj);
    }

    total_h = layout_height - (x->rows - 1) * data->vert_spacing;
    total_w = layout_width - (x->columns - 1) * data->horiz_spacing;
    Layout2D_weight_constraint(x->row_infos, x->rows, x->vert_weight_sum,
        total_h, &init_h);
    Layout2D_weight_constraint(x->col_infos, x->columns,
        x->horiz_weight_sum, total_w, &init_w);
    Layout2D_minmax_constraints_and_redistrib(x->row_infos, x->rows,
        total_h, 0, total_h - init_h);
    Layout2D_minmax_constraints_and_redistrib(x->col_infos, x->columns,
        total_w, 0, total_w - init_w);

    /* distribute space */
    n = data->childlist.mlh_Head;
    for (i = 0, row = x->row_infos; i < x->rows; i++, row++)
    {
        LONG left = 0;
        int j = 0;

        col = x->col_infos;
        while (n->mln_Succ)
        {
            Object *child = BASEOBJECT(n);
            LONG cw, ch;

            n = n->mln_Succ;
            if (IS_HIDDEN(child))
                continue;
            /* centred if the cell is larger than the child's maximum */
            cw = MAX(MIN(_maxwidth(child), col->dim), _minwidth(child));
            ch = MAX(MIN(_maxheight(child), row->dim), _minheight(child));
            if (!MUI_Layout(child, left + (col->dim - cw) / 2,
                    top + (row->dim - ch) / 2, cw, ch, 0))
                return;
            left += data->horiz_spacing + col->dim;
            col++;
            if ((++j % x->columns) == 0)
                break;
        }
        top += data->vert_spacing + row->dim;
    }
}

/*
 * A page group: every page laid out (hidden ones too), centred in the
 * available width, the title group's minimum height reserved above the
 * content (below it with MUIV_Title_Position_Bottom).
 */
static void group_layout_pagemode(struct MUI_GroupData *data, Object *obj)
{
    Object *title = group_title(data), *child;
    struct MinNode *n;
    WORD layout_width, layout_height;
    int yoffset = 0, ypage = 0, ytitle = 0;

    if (data->flags & GROUP_VIRTUAL)
    {
        group_virtual_area(obj, VX(data));
        layout_width = VX(data)->virt_mwidth;
        layout_height = VX(data)->virt_mheight;
    }
    else
    {
        layout_width = _mwidth(obj);
        layout_height = _mheight(obj);
    }

    if (title)
    {
        IPTR position = 0;

        ypage = yoffset = _minheight(title);
        layout_height -= yoffset;
        get(title, MUIA_Title_Position, &position);
        if (position == MUIV_Title_Position_Bottom)
            ypage = 0, ytitle = layout_height;
    }

    FOR_CHILD(&data->childlist, n, child)
    {
        int w = MIN(layout_width, _maxwidth(child));
        int h = MIN(layout_height, _maxheight(child));

        if (child == title)
            MUI_Layout(child, (layout_width - w) / 2,
                ytitle, w, yoffset, 0);
        else
            MUI_Layout(child, (layout_width - w) / 2,
                ypage + (layout_height - h) / 2, w, h, 0);
    }
}

/* A layout hook's MUILM_LAYOUT (the message cleared, the inner size in
 * it); a virtual group's area at least its inner box. The hook's
 * result. Its own function: the message is on the stack only here. */
static __attribute__((noinline)) IPTR group_layout_hook(
    struct MUI_GroupData *data, Object *obj)
{
    struct MUI_LayoutMsg lm = { 0 };
    IPTR result;

    lm.lm_Children = &data->childlist;
    lm.lm_Type = MUILM_LAYOUT;
    lm.lm_Layout.Width = _mwidth(obj);
    lm.lm_Layout.Height = _mheight(obj);
    result = CallHookPkt(data->layout_hook, obj, &lm);
    if (data->flags & GROUP_VIRTUAL)
    {
        /* A layout hook may need less space than the viewport. Keep
         * the virtual extent large enough to cover its visible area. */
        VX(data)->virt_mwidth = MAX(lm.lm_Layout.Width, _mwidth(obj));
        VX(data)->virt_mheight = MAX(lm.lm_Layout.Height, _mheight(obj));
    }
    return result;
}

/* After a virtual group's layout: its offsets within its new area, and a
 * new area size told (a relayout of the virtual group alone, a layout
 * hook reporting a new page size, must still reach a Scrollgroup's
 * scrollers) */
static __attribute__((noinline)) void group_layout_virtual(
    struct MUI_GroupData *data, Object *obj)
{
    struct GroupVirtual *v = VX(data);
    WORD x = v->virt_offx, y = v->virt_offy;

    if (x + _mwidth(obj) > v->virt_mwidth)
        x = v->virt_mwidth - _mwidth(obj);
    if (x < 0)
        x = 0;
    if (y + _mheight(obj) > v->virt_mheight)
        y = v->virt_mheight - _mheight(obj);
    if (y < 0)
        y = 0;
    if (x != v->virt_offx)
        nfset(obj, MUIA_Virtgroup_Left, x);
    if (y != v->virt_offy)
        nfset(obj, MUIA_Virtgroup_Top, y);
    if (v->virt_mwidth != v->told_mwidth || v->virt_mheight != v->told_mheight)
    {
        v->told_mwidth = v->virt_mwidth;
        v->told_mheight = v->virt_mheight;
        SetAttrs(obj, MUIA_Group_Forward, FALSE,
            MUIA_Virtgroup_Width, v->virt_mwidth,
            MUIA_Virtgroup_Height, v->virt_mheight, TAG_DONE);
    }
}

/**************************************************************************
 MUIM_Zune_Layout
 Either use a given layout hook, or the builtin method.
**************************************************************************/
/*
 * Select page, custom Hook or built-in layout. Return the Hook's result,
 * otherwise TRUE. Requires established child min/max and valid lifecycle.
 */
IPTR Group__MUIM_Layout(struct IClass *cl, Object *obj,
    struct MUIP_Zune_Layout *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    IPTR result = TRUE;

    if (data->flags & GROUP_PAGEMODE)
        group_layout_pagemode(data, obj);
    else if (data->layout_hook)
        result = group_layout_hook(data, obj);
    else if (!group_grid(data))
        group_layout_1d(data, obj);
    else
        group_layout_2d(data, obj);
    if (data->flags & GROUP_VIRTUAL)
        group_layout_virtual(data, obj);
    return result;
}

/**************************************************************************
 MUIM_Show, MUIM_Hide
**************************************************************************/
/*
 * Show (hide) with a message: Area first (last); a NULL message is the
 * group's own path for its children alone (a page switch, a scroll). A
 * page group shows the title group and the active page without ShowMe
 * filtering; others their ShowMe children that did not disappear (a
 * virtual group only those inside its box). Hide hides the title and the
 * active page, or every drawable child. Child results are ignored.
 */
IPTR Group__MUIM_Show(struct IClass *cl, Object *obj,
    struct MUIP_Show *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    if (msg)
        DoSuperMethodA(cl, obj, (Msg) msg);

    if (data->flags & GROUP_PAGEMODE)
    {
        Object *title = group_title(data);
        LONG page = data->active_page + 1;

        FOR_CHILD(&data->childlist, n, child)
            if (child == title)
                DoShowMethod(child);   /* not counted as a page */
            else if (!--page)
            {
                DoShowMethod(child);
                break;
            }
    }
    else
        FOR_CHILD(&data->childlist, n, child)
            if ((_flags(child) & (MADF_SHOWME | MADF_DISAPPEARED))
                    == MADF_SHOWME
                && (!(data->flags & GROUP_VIRTUAL)
                    || IsObjectVisible(child, MUIMasterBase)))
                DoShowMethod(child);
    return TRUE;
}

IPTR Group__MUIM_Hide(struct IClass *cl, Object *obj,
    struct MUIP_Hide *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    if (data->flags & GROUP_PAGEMODE)
    {
        Object *title = group_title(data);
        LONG page = data->active_page + 1;

        FOR_CHILD(&data->childlist, n, child)
            if (child == title)
                DoHideMethod(child);   /* not counted as a page */
            else if (!--page)
            {
                DoHideMethod(child);
                break;
            }
    }
    else
        FOR_CHILD(&data->childlist, n, child)
            if (_flags(child) & MADF_CANDRAW)
                DoHideMethod(child);

    if (msg)
        return DoSuperMethodA(cl, obj, (Msg) msg);
    return 1;
}

/*
 * MUIM_FindUData, MUIM_FindObject: this group when it matches, else the
 * first child's answer. MUIM_GetUData, MUIM_SetUDataOnce: this group when
 * its MUIA_UserData matches (the attribute got or set), else the
 * children until one answers. MUIM_SetUData: this group when it matches
 * (a set it may pass on to the children itself), then every child.
 */
IPTR Group__MUIM_FindUData(struct IClass *cl, Object *obj,
    struct MUIP_FindUData *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    if (ZUNE_FIND_MATCH(obj, msg))
        return (IPTR) obj;
    FOR_CHILD(&data->childlist, n, child)
    {
        IPTR found = DoMethodA(child, (Msg) msg);

        if (found)
            return found;
    }
    return 0;
}

IPTR Group__MUIM_GetUData(struct IClass *cl, Object *obj,
    struct MUIP_GetUData *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
    {
        get(obj, msg->attr, msg->storage);
        return TRUE;
    }
    FOR_CHILD(&data->childlist, n, child)
        if (DoMethodA(child, (Msg) msg))
            return TRUE;
    return FALSE;
}

IPTR Group__MUIM_SetUData(struct IClass *cl, Object *obj,
    struct MUIP_SetUData *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
        set(obj, msg->attr, msg->val);
    FOR_CHILD(&data->childlist, n, child)
        DoMethodA(child, (Msg) msg);
    return TRUE;
}

IPTR Group__MUIM_SetUDataOnce(struct IClass *cl, Object *obj,
    struct MUIP_SetUData *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    if (muiNotifyData(obj)->mnd_UserData == msg->udata)
    {
        set(obj, msg->attr, msg->val);
        return TRUE;
    }
    FOR_CHILD(&data->childlist, n, child)
        if (DoMethodA(child, (Msg) msg))
            return TRUE;
    return FALSE;
}

/**************************************************************************
 MUIM_DragQueryExtended
**************************************************************************/
/*
 * Ask drawable children in list order and return the first accepting
 * object/result; otherwise ask Area.
 */
IPTR Group__MUIM_DragQueryExtended(struct IClass *cl, Object *obj,
    struct MUIP_DragQueryExtended *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child, *found_obj;

    FOR_CHILD(&data->childlist, n, child)
    {
        if (!(_flags(child) & MADF_CANDRAW))
            continue;
        if ((found_obj = (Object *) DoMethodA(child, (Msg) msg)))
            return (IPTR) found_obj;
    }
    return DoSuperMethodA(cl, obj, (Msg) msg);
}

/**************************************************************************
 MUIM_HandleEvent
**************************************************************************/
/* Its event handler in a virtual group's event mask, the mask changed */
static void group_virt_events(Object *obj, struct GroupVirtual *v,
    ULONG events)
{
    DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR) &v->ehn);
    v->ehn.ehn_Events = events;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR) &v->ehn);
}

/* A virtual group's offset on axis a (0 x, 1 y) after an autoscroll tick:
 * four pixels towards the pointer when it is outside the box on that axis,
 * within the area */
static __attribute__((noinline)) LONG group_autoscroll(Object *obj,
    struct GroupVirtual *v, struct IntuiMessage *im, LONG a)
{
    struct MUI_AreaData *ad = muiAreaData(obj);
    WORD lo = (&ad->mad_Box.Left)[a] + (&ad->mad_addleft)[a];
    WORD size = (&ad->mad_Box.Width)[a] - (&ad->mad_subwidth)[a];
    WORD at = (&im->MouseX)[a];
    LONG off = (&v->virt_offx)[a];

    if (at < lo)
        off = off >= 4 ? off - 4 : 0;
    else if (at > lo + size - 1)
    {
        off += 4;
        if (off > (&v->virt_mwidth)[a] - size)
            off = (&v->virt_mwidth)[a] - size;
        if (off < 0)
            off = 0;
    }
    return off;
}

/*
 * Handle virtual-group mouse/tick autoscroll only; never delegate to Area
 * here (a superclass with IDCMP_MOUSEBUTTONS handlers may call it without
 * the group's handler). SELECTDOWN inside starts ticks; another button
 * event removes them. Ticks outside adjust/clamp offsets by four pixels
 * and set them with Forward FALSE. Return zero.
 */
IPTR Group__MUIM_HandleEvent(struct IClass *cl, Object *obj,
    struct MUIP_HandleEvent *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct IntuiMessage *im = msg->imsg;
    struct GroupVirtual *v;
    BOOL inside;

    if (!(data->flags & GROUP_VIRTUAL) || !im)
        return 0;
    v = VX(data);
    inside = _between(_mleft(obj), im->MouseX, _mright(obj))
        && _between(_mtop(obj), im->MouseY, _mbottom(obj));
    switch (im->Class)
    {
    case IDCMP_MOUSEBUTTONS:
        if (im->Code == SELECTDOWN)
        {
            if (inside)
                group_virt_events(obj, v,
                    v->ehn.ehn_Events | IDCMP_INTUITICKS);
        }
        else if (v->ehn.ehn_Events & IDCMP_INTUITICKS)
            group_virt_events(obj, v,
                v->ehn.ehn_Events & ~IDCMP_INTUITICKS);
        break;

    case IDCMP_INTUITICKS:
        if ((v->ehn.ehn_Events & IDCMP_INTUITICKS) && !inside)
        {
            LONG x = group_autoscroll(obj, v, im, 0);
            LONG y = group_autoscroll(obj, v, im, 1);

            if (x != v->virt_offx || y != v->virt_offy)
                SetAttrs(obj, MUIA_Virtgroup_Left, x,
                    MUIA_Virtgroup_Top, y, MUIA_Group_Forward, FALSE,
                    TAG_DONE);
        }
        break;
    }
    return 0;
}

/**************************************************************************
 MUIM_DrawBackground
**************************************************************************/
/*
 * A virtual group's background offsets moved by its virtual offsets;
 * Area's result.
 */
IPTR Group__MUIM_DrawBackground(struct IClass *cl, Object *obj,
    struct MUIP_DrawBackground *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    LONG dx, dy;
    IPTR r;

    if (!(data->flags & GROUP_VIRTUAL))
        return DoSuperMethodA(cl, obj, (Msg) msg);
    /* the offsets moved in the message for the call (no copy of it on the
     * stack) */
    msg->xoffset += (dx = VX(data)->virt_offx);
    msg->yoffset += (dy = VX(data)->virt_offy);
    r = DoSuperMethodA(cl, obj, (Msg) msg);
    msg->xoffset -= dx;
    msg->yoffset -= dy;
    return r;
}

/**************************************************************************
 MUIM_FindAreaObject
 Find the given object or return NULL
**************************************************************************/
/*
 * Find this Group, then immediate child identities, then recursive child
 * matches. Return a borrowed object or zero.
 */
IPTR Group__MUIM_FindAreaObject(struct IClass *cl, Object *obj,
    struct MUIP_FindAreaObject *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    // it's me ?
    if (msg->obj == obj)
        return (IPTR) obj;
    // it's one of my children ?
    if (group_contains(&data->childlist, msg->obj))
        return (IPTR) msg->obj;
    // let the children find it
    FOR_CHILD(&data->childlist, n, child)
    {
        Object *res = (Object *) DoMethodA(child, (Msg) msg);

        if (res != NULL)
            return (IPTR) res;
    }
    return 0;
}

/**************************************************************************
 MUIM_Export, MUIM_Import: to every child (an object's "contents" to and
 from a dataspace object), not to Area; zero.
**************************************************************************/
static IPTR Group__MUIM_Export(struct IClass *cl, Object *obj, Msg msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;

    FOR_CHILD(&data->childlist, n, child)
        DoMethodA(child, msg);
    return 0;
}

/* Notes about Group_Notify() and echo notification problem:
It was discovered that MUI seems to have some special handling for group class
which will drop notifications on the children which are found to not
understand the attribute.

This is done by checking if an OM_GET on the child returns TRUE.
There's a little problem here because it is not known how big the storage
needed for the attribute in question will be. Almost no class uses anything
bigger than one IPTR. For "big" attributes those return a pointer to the data,
not the data itself. Unfortuntely there are some exceptions like colorwheel
class which does not return a pointer, but the data itself. So it's not
enough to use one single IPTR variable (4 Bytes on 32bit machines, 8 bytes
on 64 bit machines) to store the result of the test-OM_Get.

There is no general way to query the size needed so if one wants to change
Zune to work like MUI one needs to choose a size which one hopes will be
big enough to hold all possible attributes of all classes, old, present
and future ones.
*/
/*
 * Probe trigger getter on Group with child-get forwarding disabled, then
 * register locally through Area if recognized. Otherwise probe each child
 * and register on supported getters, ignoring registration results and
 * returning TRUE even if none match. Getters may write at most 30 IPTRs.
 */
static IPTR __attribute__((noinline)) Group_Notify(struct IClass *cl,
    Object *obj, struct MUIP_Notify *msg)
{
    struct MUI_GroupData *data = GDATA(cl, obj);
    struct MinNode *n;
    Object *child;
    IPTR attr[30];

    data->flags |= GROUP_NOFWD_GET;
    if (GetAttr(msg->TrigAttr, obj, attr))
    {
        data->flags &= ~GROUP_NOFWD_GET;
        return DoSuperMethodA(cl, obj, (Msg) msg);
    }
    data->flags &= ~GROUP_NOFWD_GET;

    FOR_CHILD(&data->childlist, n, child)
        if (GetAttr(msg->TrigAttr, child, attr))
            DoMethodA(child, (Msg) msg);    /* No return here! */
    return TRUE;
}

/* A plain group's MUIM_DrawBackground only hands the request on to its
 * parent: Group_PassesBackground() (area.h) reads MADF2_PLAINGROUP, set at
 * OM_NEW for an object of the Group class itself that is not virtual. */

/*
 * The dispatcher. The methods Group answers, by MethodID (unsigned,
 * ascending: halved to find one; the compiler checks the order). Those
 * that belong to the group itself, not to every child, go to Area
 * (DoSuperMethodA: the same arguments); any other method goes to every
 * child, then to Area (Group_Forward). Return the handler's result, not
 * the children's.
 */
static const struct { ULONG id; APTR method; } group_methods[] =
{
    { OM_NEW, (APTR)Group__OM_NEW },
    { OM_DISPOSE, (APTR)Group__OM_DISPOSE },
    { OM_SET, (APTR)Group__OM_SET },
    { OM_GET, (APTR)Group__OM_GET },
    { OM_ADDTAIL, (APTR)DoSuperMethodA },
    { OM_REMOVE, (APTR)DoSuperMethodA },
    { OM_ADDMEMBER, (APTR)group_add },
    { OM_REMMEMBER, (APTR)Group__MUIM_Remove },
    { MUIM_DragQuery, (APTR)DoSuperMethodA },
    { MUIM_FindObject, (APTR)Group__MUIM_FindUData },
    { MUIM_Group_InitChange, (APTR)Group__MUIM_InitChange },
    { MUIM_Export, (APTR)Group__MUIM_Export },
    { MUIM_DeleteBubble, (APTR)DoSuperMethodA },
    { MUIM_DoDrag, (APTR)DoSuperMethodA },
    { MUIM_CreateBubble, (APTR)DoSuperMethodA },
    { MUIM_GoInactive, (APTR)DoSuperMethodA },
    { MUIM_DeleteDragImage, (APTR)DoSuperMethodA },
    { MUIM_AskMinMax, (APTR)Group__MUIM_AskMinMax },
    { MUIM_DrawBackground, (APTR)Group__MUIM_DrawBackground },
    { MUIM_GoActive, (APTR)DoSuperMethodA },
    { MUIM_Group_Insert, (APTR)group_add },
    { MUIM_DragFinish, (APTR)DoSuperMethodA },
    { MUIM_Set, (APTR)DoSuperMethodA },
    { MUIM_Virtgroup_BeginPaint, (APTR)Group__MUIM_Virtgroup_BeginPaint },
    { MUIM_Group_Reorder, (APTR)GroupReorder },
    { MUIM_HandleEvent, (APTR)Group__MUIM_HandleEvent },
    { MUIM_Draw, (APTR)Group__MUIM_Draw },
    { MUIM_Group_Sort, (APTR)Group__MUIM_Sort },
    { MUIM_Setup, (APTR)Group__MUIM_Setup },
    { MUIM_Layout, (APTR)DoSuperMethodA },
    { MUIM_CreateShortHelp, (APTR)DoSuperMethodA },
    { MUIM_CallHook, (APTR)DoSuperMethodA },
    { MUIM_DragBegin, (APTR)DoSuperMethodA },
    { MUIM_FindUData, (APTR)Group__MUIM_FindUData },
    { MUIM_DragDrop, (APTR)DoSuperMethodA },
    { MUIM_Family_GetChild, (APTR)Group__MUIM_Family_GetChild },
    { MUIM_SetUData, (APTR)Group__MUIM_SetUData },
    { MUIM_Notify, (APTR)Group_Notify },
    { MUIM_SetUDataOnce, (APTR)Group__MUIM_SetUDataOnce },
    { MUIM_Show, (APTR)Group__MUIM_Show },
    { MUIM_Import, (APTR)Group__MUIM_Export },
    { MUIM_Group_ExitChange, (APTR)Group__MUIM_ExitChange },
    { MUIM_MultiSet, (APTR)DoSuperMethodA },
    { MUIM_DeleteShortHelp, (APTR)DoSuperMethodA },
    { MUIM_GlobalSetup, (APTR)Group__MUIM_GlobalSetup },
    { MUIM_Group_AddTail, (APTR)group_add },
    { MUIM_Cleanup, (APTR)Group__MUIM_Cleanup },
    { MUIM_Virtgroup_EndPaint, (APTR)Group__MUIM_Virtgroup_EndPaint },
    { MUIM_Group_AddHead, (APTR)group_add },
    { MUIM_Group_ExitChange2, (APTR)Group__MUIM_ExitChange2 },
    { MUIM_CreateDragImage, (APTR)DoSuperMethodA },
    { MUIM_GetUData, (APTR)Group__MUIM_GetUData },
    { MUIM_Hide, (APTR)Group__MUIM_Hide },
    { MUIM_Group_Remove, (APTR)Group__MUIM_Remove },
    { MUIM_Group_MoveMember, (APTR)GroupMoveMember },
    { MUIM_Zune_Layout, (APTR)Group__MUIM_Layout },
    { MUIM_DrawParentBackground, (APTR)DoSuperMethodA },
    { MUIM_DragQueryExtended, (APTR)Group__MUIM_DragQueryExtended },
    { MUIM_FindAreaObject, (APTR)Group__MUIM_FindAreaObject },
    { MUIM_Group_DoMethodNoForward, (APTR)Group__MUIM_DoMethodNoForward },
    { MUIM_ConnectParent, (APTR)Group__MUIM_ConnectParent },
    { MUIM_DisconnectParent, (APTR)Group__MUIM_DisconnectParent },
};
#define GM_LT(a, b) _Static_assert((ULONG)(a) < (ULONG)(b), "group_methods order")
GM_LT(OM_NEW, OM_DISPOSE); GM_LT(OM_DISPOSE, OM_SET); GM_LT(OM_SET, OM_GET);
GM_LT(OM_GET, OM_ADDTAIL); GM_LT(OM_ADDTAIL, OM_REMOVE);
GM_LT(OM_REMOVE, OM_ADDMEMBER); GM_LT(OM_ADDMEMBER, OM_REMMEMBER);
GM_LT(OM_REMMEMBER, MUIM_DragQuery); GM_LT(MUIM_DragQuery, MUIM_FindObject);
GM_LT(MUIM_FindObject, MUIM_Group_InitChange);
GM_LT(MUIM_Group_InitChange, MUIM_Export);
GM_LT(MUIM_Export, MUIM_DeleteBubble); GM_LT(MUIM_DeleteBubble, MUIM_DoDrag);
GM_LT(MUIM_DoDrag, MUIM_CreateBubble); GM_LT(MUIM_CreateBubble, MUIM_GoInactive);
GM_LT(MUIM_GoInactive, MUIM_DeleteDragImage);
GM_LT(MUIM_DeleteDragImage, MUIM_AskMinMax);
GM_LT(MUIM_AskMinMax, MUIM_DrawBackground);
GM_LT(MUIM_DrawBackground, MUIM_GoActive);
GM_LT(MUIM_GoActive, MUIM_Group_Insert); GM_LT(MUIM_Group_Insert, MUIM_DragFinish);
GM_LT(MUIM_DragFinish, MUIM_Set); GM_LT(MUIM_Set, MUIM_Virtgroup_BeginPaint);
GM_LT(MUIM_Virtgroup_BeginPaint, MUIM_Group_Reorder);
GM_LT(MUIM_Group_Reorder, MUIM_HandleEvent); GM_LT(MUIM_HandleEvent, MUIM_Draw);
GM_LT(MUIM_Draw, MUIM_Group_Sort); GM_LT(MUIM_Group_Sort, MUIM_Setup);
GM_LT(MUIM_Setup, MUIM_Layout); GM_LT(MUIM_Layout, MUIM_CreateShortHelp);
GM_LT(MUIM_CreateShortHelp, MUIM_CallHook); GM_LT(MUIM_CallHook, MUIM_DragBegin);
GM_LT(MUIM_DragBegin, MUIM_FindUData); GM_LT(MUIM_FindUData, MUIM_DragDrop);
GM_LT(MUIM_DragDrop, MUIM_Family_GetChild);
GM_LT(MUIM_Family_GetChild, MUIM_SetUData); GM_LT(MUIM_SetUData, MUIM_Notify);
GM_LT(MUIM_Notify, MUIM_SetUDataOnce); GM_LT(MUIM_SetUDataOnce, MUIM_Show);
GM_LT(MUIM_Show, MUIM_Import); GM_LT(MUIM_Import, MUIM_Group_ExitChange);
GM_LT(MUIM_Group_ExitChange, MUIM_MultiSet);
GM_LT(MUIM_MultiSet, MUIM_DeleteShortHelp);
GM_LT(MUIM_DeleteShortHelp, MUIM_GlobalSetup);
GM_LT(MUIM_GlobalSetup, MUIM_Group_AddTail);
GM_LT(MUIM_Group_AddTail, MUIM_Cleanup);
GM_LT(MUIM_Cleanup, MUIM_Virtgroup_EndPaint);
GM_LT(MUIM_Virtgroup_EndPaint, MUIM_Group_AddHead);
GM_LT(MUIM_Group_AddHead, MUIM_Group_ExitChange2);
GM_LT(MUIM_Group_ExitChange2, MUIM_CreateDragImage);
GM_LT(MUIM_CreateDragImage, MUIM_GetUData); GM_LT(MUIM_GetUData, MUIM_Hide);
GM_LT(MUIM_Hide, MUIM_Group_Remove); GM_LT(MUIM_Group_Remove, MUIM_Group_MoveMember);
GM_LT(MUIM_Group_MoveMember, MUIM_Zune_Layout);
GM_LT(MUIM_Zune_Layout, MUIM_DrawParentBackground);
GM_LT(MUIM_DrawParentBackground, MUIM_DragQueryExtended);
GM_LT(MUIM_DragQueryExtended, MUIM_FindAreaObject);
GM_LT(MUIM_FindAreaObject, MUIM_Group_DoMethodNoForward);
GM_LT(MUIM_Group_DoMethodNoForward, MUIM_ConnectParent);
GM_LT(MUIM_ConnectParent, MUIM_DisconnectParent);

/* The function of a method: group_methods' or Group_Forward (a function of
 * its own: its search is off the stack when the method runs) */
static __attribute__((noinline)) APTR GroupMethod(ULONG id)
{
    const ULONG *lo = &group_methods[0].id,
        *hi = &group_methods[sizeof(group_methods)
            / sizeof(group_methods[0]) - 1].id, *p;

    while (lo <= hi)
    {
        ULONG m;

        p = lo + (((hi - lo) >> 2) << 1);
        if ((m = *p) == id)
            return (APTR)p[1];
        if (m < id)
            lo = p + 2;
        else
            hi = p - 2;
    }
    return (APTR)Group_Forward;
}

BOOPSI_DISPATCHER(IPTR, Group_Dispatcher, cl, obj, msg)
{
    IPTR (*method)(struct IClass *, Object *, Msg);

    /* the methods sent most, without the search (on a 68000 a table
     * search costs more than GCC's compare tree): a draw of every group
     * on every redraw, the sets and gets of every subclass (List, Cycle,
     * Radio, ...), a List's row backgrounds, the layout's and the
     * visibility's of every group */
    switch (msg->MethodID)
    {
    case MUIM_Draw:
        return Group__MUIM_Draw(cl, obj, (struct MUIP_Draw *)msg);
    case OM_SET:
        return Group__OM_SET(cl, obj, (struct opSet *)msg);
    case OM_GET:
        return Group__OM_GET(cl, obj, (struct opGet *)msg);
    case MUIM_DrawBackground:
        return Group__MUIM_DrawBackground(cl, obj,
            (struct MUIP_DrawBackground *)msg);
    case MUIM_AskMinMax:
        return Group__MUIM_AskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
    case MUIM_Zune_Layout:
        return Group__MUIM_Layout(cl, obj, (struct MUIP_Zune_Layout *)msg);
    case MUIM_Layout:
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_HandleEvent:
        return Group__MUIM_HandleEvent(cl, obj,
            (struct MUIP_HandleEvent *)msg);
    case MUIM_Show:
        return Group__MUIM_Show(cl, obj, (struct MUIP_Show *)msg);
    case MUIM_Hide:
        return Group__MUIM_Hide(cl, obj, (struct MUIP_Hide *)msg);
    }
    method = GroupMethod(msg->MethodID);
    return method(cl, obj, msg);
}
BOOPSI_DISPATCHER_END

/*
 * Class descriptor.
 */
const struct __MUIBuiltinClass _MUI_Group_desc =
{
    MUIC_Group,
    MUIC_Area,
    sizeof(struct MUI_GroupData),
    (void *) Group_Dispatcher
};
