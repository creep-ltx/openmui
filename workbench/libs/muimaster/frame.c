/*
    Copyright  2002-2026, The AROS Development Team.
    Copyright  1999, David Le Corfec.
    All rights reserved.

*/

#include <exec/types.h>
#include <proto/graphics.h>
#include <string.h>

#include "datatypescache.h"
#include "frame.h"
#include "mui.h"
#include "muimaster_intern.h"
#include "modules.h"

extern struct Library *MUIMasterBase;

/**************************************************************************
 The built-in frames, rewritten for the 68000 (2026-11, rw-draw): one draw
 function per shape, the pens in the frame's table entry (zf->ul, zf->lr)
 instead of a wrapper function per pen pair, and no "global" box (only the
 custom frames' tiles needed it, and they draw nothing: see the custom
 entry below). Every safety return and every primitive is lean's, so the
 pixels are (tests/lean/framepix.c).

 Measured on a 68020 planar screen (tests/native/gfxprims.c): RectFill is
 fastest for horizontal lines, Move/Draw for vertical ones, and repeating
 the current pen still costs a library call.
**************************************************************************/

static inline void hline(struct RastPort *rp, int x1, int x2, int y, ULONG pen)
{
    if (x1 > x2)
        return;
    if (rp->FgPen != (BYTE)pen)
        SetAPen(rp, pen);
    RectFill(rp, x1, y, x2, y);
}

static inline void vline(struct RastPort *rp, int x, int y1, int y2, ULONG pen)
{
    if (y1 > y2)
        return;
    if (rp->FgPen != (BYTE)pen)
        SetAPen(rp, pen);
    Move(rp, x, y1);
    Draw(rp, x, y2);
}

/* 0 : FST_NONE (and the round types without their module) */
static void frame_none(const struct ZuneFrameGfx *zf,
    struct dt_frame_image *fi, struct MUI_RenderInfo *mri, int left,
    int top, int width, int height)
{
}

/* The drawing functions read their pens from zf (ul, lr); a shape made of
 * others hands them its opposite state's entry (the pens swapped: the up
 * and down entries of a type are neighbours) or one of these */
#define PENS(ul, lr) {frame_none, 0, 0, 0, 0, 0, 0, ul, lr}
static const struct ZuneFrameGfx pens_bg = PENS(MPEN_BACKGROUND, 0);
static const struct ZuneFrameGfx pens_shadow = PENS(MPEN_SHADOW, 0);
static const struct ZuneFrameGfx pens_sh_hsd = PENS(MPEN_SHINE, MPEN_HALFSHADOW);
static const struct ZuneFrameGfx pens_hsd_sd = PENS(MPEN_HALFSHADOW, MPEN_SHADOW);
static const struct ZuneFrameGfx pens_bg_sd = PENS(MPEN_BACKGROUND, MPEN_SHADOW);

static const struct ZuneFrameGfx builtin_gfx[2 * (FST_ROUNDED + 1)];
#define OPPOSITE(zf) ((zf) + (((zf) - builtin_gfx) & 1 ? -1 : 1))

/* 1 : FST_RECT: a one pixel rectangle in zf->ul (width and height become
 * the right and bottom edges: fewer registers to keep) */
static void rect_draw(const struct ZuneFrameGfx *zf,
    struct dt_frame_image *fi, struct MUI_RenderInfo *mri, int left,
    int top, int width, int height)
{
    struct RastPort *rp;

    if (width <= 0 || height <= 0)
        return;
    rp = mri->mri_RastPort;
    SetAPen(rp, mri->mri_Pens[zf->ul]);
    width += left - 1;
    height += top - 1;
    if (width == left || height == top)
    {
        RectFill(rp, left, top, width, height);
        return;
    }
    RectFill(rp, left, top, width, top);
    RectFill(rp, left, height, width, height);
    RectFill(rp, left, top + 1, left, height - 1);
    RectFill(rp, width, top + 1, width, height - 1);
}

/* 2 : FST_BEVEL: ul on the left column and the top row, lr on the right
 * column and the bottom row, short of the top-right and bottom-left
 * corners. Those two corners are MPEN_BACKGROUND in MUI 3.9 even where
 * another background lies beneath them (a pressed button's bottom-left
 * corner, a white parent's top-right one: tests/native/cornerbg.c). Area
 * tells when its background fill has just painted them with that pen. */
static void button_draw(const struct ZuneFrameGfx *zf,
    struct dt_frame_image *fi, struct MUI_RenderInfo *mri, int left,
    int top, int width, int height)
{
    struct RastPort *rp = mri->mri_RastPort;
    int right = left + width - 1, bottom = top + height - 1;
    UBYTE known;

    if (!rp || width <= 2 || height <= 2)
        return;
    vline(rp, left, top, bottom - 1, mri->mri_Pens[zf->ul]);
    hline(rp, left, right - 1, top, mri->mri_Pens[zf->ul]);
    vline(rp, right, top + 1, bottom, mri->mri_Pens[zf->lr]);
    hline(rp, left + 1, right, bottom, mri->mri_Pens[zf->lr]);
    known = fi ? fi->corners_bg : 0;
    if ((known & (FRAME_CORNER_TR | FRAME_CORNER_BL))
        != (FRAME_CORNER_TR | FRAME_CORNER_BL))
        SetAPen(rp, mri->mri_Pens[MPEN_BACKGROUND]);
    if (!(known & FRAME_CORNER_BL))
        WritePixel(rp, left, bottom);
    if (!(known & FRAME_CORNER_TR))
        WritePixel(rp, right, top);
}

/* 3 : FST_THIN_BORDER, 9 : FST_GRAY_BORDER (in the half pens) */
/* no-function-cse: the callee's address in a register cost a saved
 * register (4 bytes of stack) */
static __attribute__((optimize("no-function-cse"))) void frame_thin(
    const struct ZuneFrameGfx *zf,
    struct dt_frame_image *fi, struct MUI_RenderInfo *mri, int left,
    int top, int width, int height)
{
    struct RastPort *rp = mri->mri_RastPort;
    ULONG ul = mri->mri_Pens[zf->ul];

    if (width <= 3 || height <= 3)
        return;
    SetAPen(rp, ul);
    vline(rp, left, top, top + height - 1, ul);
    hline(rp, left, left + width - 1, top, ul);
    vline(rp, left + width - 2, top + 2, top + height - 2, ul);
    hline(rp, left + 2, left + width - 2, top + height - 2, ul);
    /* the inner rectangle in lr: the opposite entry's ul */
    rect_draw(OPPOSITE(zf), fi, mri, left + 1, top + 1, width - 1,
        height - 1);
}

/* 4 : FST_THICK_BORDER: two bevels of opposite states, the ring between
 * them in the background pen */
/* no-function-cse: the callee's address in a register cost a saved
 * register (4 bytes of stack) */
static __attribute__((optimize("no-function-cse"))) void frame_thick(
    const struct ZuneFrameGfx *zf,
    struct dt_frame_image *fi, struct MUI_RenderInfo *mri, int left,
    int top, int width, int height)
{
    if (width <= 4 || height <= 4)
        return;
    button_draw(zf, fi, mri, left, top, width, height);
    button_draw(OPPOSITE(zf), fi, mri, left + 2, top + 2, width - 4,
        height - 4);
    rect_draw(&pens_bg, fi, mri, left + 1, top + 1, width - 2, height - 2);
}

/* 6 : FST_WIN_BEVEL raised */
/* no-function-cse: the callee's address in a register cost a saved
 * register (4 bytes of stack) */
static __attribute__((optimize("no-function-cse"))) void frame_win_up(
    const struct ZuneFrameGfx *zf,
    struct dt_frame_image *fi, struct MUI_RenderInfo *mri, int left,
    int top, int width, int height)
{
    if (width <= 2 || height <= 2)
        return;
    rect_draw(&pens_shadow, fi, mri, left, top, width, height);
    button_draw(&pens_sh_hsd, fi, mri, left + 1, top + 1, width - 2,
        height - 2);
}

/* 6 : FST_WIN_BEVEL pressed */
/* no-function-cse: the callee's address in a register cost a saved
 * register (4 bytes of stack) */
static __attribute__((optimize("no-function-cse"))) void frame_win_down(
    const struct ZuneFrameGfx *zf,
    struct dt_frame_image *fi, struct MUI_RenderInfo *mri, int left,
    int top, int width, int height)
{
    if (width <= 4 || height <= 4)
        return;
    rect_draw(&pens_shadow, fi, mri, left, top, width, height);
    button_draw(&pens_hsd_sd, fi, mri, left + 1, top + 1, width - 1,
        height - 1);
    button_draw(&pens_bg_sd, fi, mri, left + 2, top + 2, width - 2,
        height - 2);
}

/**************************************************************************
 Thick frames (MUI 3.8's "Frame thickness: thick", the default where the
 screen is at least twice as wide as tall): types 1, 2, 3 and 9 draw their
 left and right lines 2 pixels wide (and are 2 or 4 pixels wider for it,
 see thick_gfx); type 4 keeps its size and fills its gap ring. Top and
 bottom lines stay 1 pixel. The pixels are MUI 3.8's, measured.

 Zune numbers the types 5 to 10 as MUI 4 and 5 do (TheBar's default
 button frame "602222" is the xen look), MUI 3.8 numbers them otherwise:
 its 5 is Zune's 6 raised, its 6 is Zune's 9 (type 3 in the half pens, the
 one of them that is thick), its 7 is Zune's 10, its 9 is close to Zune's
 7 and its 10 to Zune's 5; 8 is 8 in both. The numbers stay Zune's: MUI
 3.8 takes no frame spec string in MUIA_Frame, MUI 4 and 5 programs do.
**************************************************************************/

/* RectFill only what is a rectangle: tiny frames must not hand
 * graphics.library an inverted one */
static void fill_box(struct RastPort *rp, int x0, int y0, int x1, int y1)
{
    if (x0 <= x1 && y0 <= y1)
        RectFill(rp, x0, y0, x1, y1);
}

#define TR_LR 1 /* the top-right corner pixel in the lower right pen */
#define BL_LR 2 /* the bottom-left one too (else both upper left) */

/* One bevel line ring around the box l..r x t..b: the ul pen on the top row
 * and the left column(s), the lr pen on the bottom row and the right
 * column(s), cw (1 or 2) pixels wide; the top-right and bottom-left corner
 * pixels as corners says. */
static __attribute__((noinline)) void bevel_ring(struct MUI_RenderInfo *mri,
    int l, int t, int r, int b, int cw, MPen ul, MPen lr, UBYTE corners)
{
    struct RastPort *rp = mri->mri_RastPort;

    SetAPen(rp, mri->mri_Pens[ul]);
    fill_box(rp, l, t, (corners & TR_LR) ? r - 1 : r, t);
    fill_box(rp, l, t + 1, l + cw - 1, (corners & BL_LR) ? b - 1 : b);
    SetAPen(rp, mri->mri_Pens[lr]);
    fill_box(rp, l + 1, b, r, b);
    fill_box(rp, r - cw + 1, t + 1, r, b - 1);
    if (corners & TR_LR)
        WritePixel(rp, r, t);
    if (corners & BL_LR)
        WritePixel(rp, l, b);
}

/* 1 (ul == lr) and 2: the outer column takes the off-pen corner, top
 * right in the lower right pen, bottom left in the upper left one */
static void frame_bevel2(const struct ZuneFrameGfx *zf,
    struct dt_frame_image *fi, struct MUI_RenderInfo *mri, int left,
    int top, int width, int height)
{
    bevel_ring(mri, left, top, left + width - 1, top + height - 1, 2, zf->ul,
        zf->lr, TR_LR);
}

/* 3 and 9: two thick bevels, the inner one of the opposite state at
 * x + 2, y + 1 */
static void frame_thin2(const struct ZuneFrameGfx *zf,
    struct dt_frame_image *fi, struct MUI_RenderInfo *mri, int left,
    int top, int width, int height)
{
    int right = left + width - 1, bottom = top + height - 1;

    if (width < 8 || height < 4)
        return;
    bevel_ring(mri, left, top, right, bottom, 2, zf->ul, zf->lr, TR_LR);
    bevel_ring(mri, left + 2, top + 1, right - 2, bottom - 1, 2, zf->lr,
        zf->ul, TR_LR);
}

/* 4: same size as thin; the gap ring is filled with the outer lower right
 * pen, and every bevel corner pixel takes its bevel's lower right pen */
static void frame_thick2(const struct ZuneFrameGfx *zf,
    struct dt_frame_image *fi, struct MUI_RenderInfo *mri, int left,
    int top, int width, int height)
{
    int right = left + width - 1, bottom = top + height - 1;
    MPen ul = zf->ul, lr = zf->lr;

    if (width < 6 || height < 6)
        return;
    bevel_ring(mri, left, top, right, bottom, 1, ul, lr, TR_LR | BL_LR);
    bevel_ring(mri, left + 1, top + 1, right - 1, bottom - 1, 1, lr, lr,
        TR_LR | BL_LR);
    bevel_ring(mri, left + 2, top + 2, right - 2, bottom - 2, 1, lr, ul,
        TR_LR | BL_LR);
}

/* Types 5, 7, 8, A and B, the rounded frames: frame_round.c (the round
 * frame module in the native build, see zune_zframe_get_with_state) */
#if !defined(ZUNE68_GCC_NATIVE)
#include "frame_round.c"
#define ROUND(f) f
#else
/* The rounded types' sizes stay here, so layout never needs the module;
 * their drawing comes from it. Without the module they draw nothing. */
#define ROUND(f) frame_none
#endif

#define SH MPEN_SHINE
#define SD MPEN_SHADOW
#define HSH MPEN_HALFSHINE
#define HSD MPEN_HALFSHADOW

/* up and down for the types 0 to B: draw, inner left, right, top, bottom,
 * border width and radius, pens */
static const struct ZuneFrameGfx builtin_gfx[2 * (FST_ROUNDED + 1)] = {
    /* 0 : FST_NONE */
    {frame_none, 0, 0, 0, 0, 0, 0},
    {frame_none, 0, 0, 0, 0, 0, 0},
    /* 1 : FST_RECT */
    {rect_draw, 1, 1, 1, 1, 1, 0, SH, 0},
    {rect_draw, 1, 1, 1, 1, 1, 0, SD, 0},
    /* 2 : FST_BEVEL */
    {button_draw, 1, 1, 1, 1, 1, 0, SH, SD},
    {button_draw, 1, 1, 1, 1, 1, 0, SD, SH},
    /* 3 : FST_THIN_BORDER */
    {frame_thin, 2, 2, 2, 2, 2, 0, SH, SD},
    {frame_thin, 2, 2, 2, 2, 2, 0, SD, SH},
    /* 4 : FST_THICK_BORDER */
    {frame_thick, 3, 3, 3, 3, 3, 0, SH, SD},
    {frame_thick, 3, 3, 3, 3, 3, 0, SD, SH},
    /* 5 : FST_ROUND_BEVEL */
    {ROUND(frame_round_bevel_up_draw), 4, 4, 1, 1, 2, 2},
    {ROUND(frame_round_bevel_down_draw), 4, 4, 1, 1, 2, 2},
    /* 6 : FST_WIN_BEVEL (zin31/xen look) */
    {frame_win_up, 2, 2, 2, 2, 2, 0},
    {frame_win_down, 3, 1, 3, 1, 2, 0},
    /* 7 : FST_ROUND_THICK_BORDER */
    {ROUND(frame_round_thick_border_up_draw), 4, 4, 4, 4, 4, 3},
    {ROUND(frame_round_thick_border_down_draw), 4, 4, 4, 4, 4, 3},
    /* 8 : FST_ROUND_THIN_BORDER, 3 pixels as in MUI 3.8 */
    {ROUND(frame_round_thin_border_up_draw), 3, 3, 3, 3, 2, 2},
    {ROUND(frame_round_thin_border_down_draw), 3, 3, 3, 3, 2, 2},
    /* 9 : FST_GRAY_BORDER */
    {frame_thin, 2, 2, 2, 2, 2, 0, HSH, HSD},
    {frame_thin, 2, 2, 2, 2, 2, 0, HSD, HSH},
    /* A : FST_SEMIROUND_BEVEL */
    {ROUND(frame_semiround_bevel_up_draw), 2, 2, 2, 2, 2, 1},
    {ROUND(frame_semiround_bevel_down_draw), 2, 2, 2, 2, 2, 1},
    /* B : FST_ROUNDED */
    {ROUND(frame_rounded_up_draw), 2, 2, 2, 2, 1, 8},
    {ROUND(frame_rounded_down_draw), 2, 2, 2, 2, 1, 8},
};

/* Thick frames, for render infos without MUIMRI_THINFRAMES: types 1, 2, 3,
 * 4 and 9, raised and recessed, in that order */
static const struct ZuneFrameGfx thick_gfx[10] = {
    {frame_bevel2, 2, 2, 1, 1, 1, 0, SH, SH},
    {frame_bevel2, 2, 2, 1, 1, 1, 0, SD, SD},
    {frame_bevel2, 2, 2, 1, 1, 1, 0, SH, SD},
    {frame_bevel2, 2, 2, 1, 1, 1, 0, SD, SH},
    {frame_thin2, 4, 4, 2, 2, 2, 0, SH, SD},
    {frame_thin2, 4, 4, 2, 2, 2, 0, SD, SH},
    {frame_thick2, 3, 3, 3, 3, 3, 0, SH, SD},
    {frame_thick2, 3, 3, 3, 3, 3, 0, SD, SH},
    {frame_thin2, 4, 4, 2, 2, 2, 0, HSH, HSD},
    {frame_thin2, 4, 4, 2, 2, 2, 0, HSD, HSH},
};

/**************************************************************************
 The frame of a spec. obj is a set up Area object or NULL; its render info
 decides thick or thin (MUIMRI_THINFRAMES, set by Window at setup from the
 "Frame thickness" setting and the screen). Without one, thin. A custom
 type without its picture is a white rectangle.
**************************************************************************/
const struct ZuneFrameGfx *zune_zframe_get_with_state(
    Object *obj, const struct MUI_FrameSpec_intern *frameSpec, UWORD state)
{
    UWORD type = frameSpec->type;
    struct MUI_RenderInfo *mri = obj ? muiRenderInfo(obj) : NULL;

    if (type >= FST_CUSTOM1)
    {
        /* the custom frames (types 12 to 27) of a window whose settings
         * load their pictures (mri_Tail->FrameImage): the picture
         * module's entries, which tile the picture (frame_tile.c) */
        struct dt_frame_image *fi;

        if (type < FST_COUNT && mri
            && (fi = mri->mri_Tail->FrameImage[type - FST_CUSTOM1]))
            return &((struct dt_custom_frame *)fi)->gfx[state & 1];
        return &builtin_gfx[2 * FST_RECT];
    }
    if (((type >= FST_RECT && type <= FST_THICK_BORDER)
        || type == FST_GRAY_BORDER) && mri
        && !(mri->mri_Flags & MUIMRI_THINFRAMES))
        return &thick_gfx[2 * (type == FST_GRAY_BORDER ? 4 : type - 1)
            + state];
#if defined(ZUNE68_GCC_NATIVE)
    /* 5, 7, 8, A, B: the round frame module's entries (same sizes) */
    else if ((1 << type) & (1 << FST_ROUND_BEVEL | 1 << FST_ROUND_THICK_BORDER
        | 1 << FST_ROUND_THIN_BORDER | 1 << FST_SEMIROUND_BEVEL
        | 1 << FST_ROUNDED))
    {
        const struct ZuneFrameGfx *round = zune_round_frames_get();

        if (round)
            return &round[2 * (type == FST_ROUND_BEVEL ? 0 :
                type <= FST_ROUND_THIN_BORDER ? type - 6 : type - 7) + state];
    }
#endif
    return &builtin_gfx[2 * type + state];
}

/* A built-in type's thin entry, for its sizes only: never loads the round
 * frame module (muigfx.library's frame table, made at library init) */
const struct ZuneFrameGfx *zune_zframe_get_sizes(UWORD type)
{
    return &builtin_gfx[2 * type];
}

const struct ZuneFrameGfx *
zune_zframe_get(Object *obj, const struct MUI_FrameSpec_intern *frameSpec)
{
    return zune_zframe_get_with_state(obj, frameSpec, frameSpec->state);
}

/*------------------------------------------------------------------------*/

/* A frame spec string: the type and the state, the four spacings and, when
 * either is set, the radius and the width, one hexadecimal digit each
 * (the first digit of a value above 15, as lean's "%lx" gave) */
BOOL zune_frame_intern_to_spec(const struct MUI_FrameSpec_intern *intern,
    STRPTR spec)
{
    static const char hex[] = "0123456789abcdef";
    const UBYTE *v;
    int i, n;

    if (!intern || !spec)
        return FALSE;
    n = intern->border_radius || intern->border_width ? 7 : 5;
    spec[0] = intern->type < 10 ? intern->type + '0' : intern->type - 10 + 'a';
    for (v = &intern->state, i = 0; i < n; i++, v++)
        spec[i + 1] = hex[*v >= 16 ? *v >> 4 : *v];
    spec[n + 1] = 0;
    return TRUE;
}

/* The frame image a built-in frame is drawn with: its radius and width
 * (a rounded type B spec's own, else the type's) */
struct dt_frame_image *
zune_frame_prepare_for_drawing(const struct ZuneFrameGfx *zframe,
    const struct MUI_FrameSpec_intern *framespec,
    struct dt_frame_image *temp_storage)
{
    memset(temp_storage, 0, sizeof(struct dt_frame_image));
    if (framespec && framespec->type == FST_ROUNDED)
    {
        temp_storage->border_radius = framespec->border_radius;
        temp_storage->frame_width = framespec->border_width;
    }
    else
    {
        temp_storage->border_radius = zframe->border_radius;
        temp_storage->frame_width = zframe->border_width;
    }
    return temp_storage;
}

/*------------------------------------------------------------------------*/

/* "TSLRTB" (6 characters), + radius, + width: a type below FST_COUNT, a
 * state 0 or 1, values to 15, digits 0-9 and letters of either case. The
 * fields are written in order up to the first bad one, as lean did. */
BOOL zune_frame_spec_to_intern(CONST_STRPTR spec,
    struct MUI_FrameSpec_intern *intern)
{
    int len, i, val;
    char c;

    if (!intern || !spec)
        return FALSE;
    len = strlen(spec);
    if (len < 6 || len > 8)
        return FALSE;
    for (i = 0; i < len; i++)
    {
        c = spec[i];
        if (c >= '0' && c <= '9')
            val = c - '0';
        else if (c >= 'a' && c <= 'z')
            val = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z')
            val = c - 'A' + 10;
        else
            return FALSE;
        if (val > (i == 0 ? FST_COUNT - 1 : i == 1 ? 1 : 15))
            return FALSE;
        if (i == 0)
            intern->type = val;
        else
            (&intern->state)[i - 1] = val;
        if (i == 5)
            intern->border_radius = intern->border_width = 0;
    }
    return TRUE;
}
