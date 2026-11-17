/*
    Copyright (C) 2003-2026, The AROS Development Team.
    All rights reserved.

*/

#include <string.h>
#include <intuition/imageclass.h>
#include <proto/graphics.h>
#include <proto/exec.h>

#include "mui.h"
#include "imspec_intern.h"
#include "support.h"
#include "modules.h"

extern struct Library *MUIMasterBase;

/**************************************************************************
 The built-in vector images, rewritten for the 68000 (2026-11, rw-draw).
 The images every window uses (arrows, check mark, radio button, pop-up
 glyphs) are blitter templates: a radio image or an arrow drawn with
 RectFill/WritePixel took a dozen calls, each 0.1-0.3 ms on a 68020 whatever
 its size (tests/native/gfxprims.c); a BltTemplate paints a whole pen's
 pixels in one call. Each template is built once, from the shape's own
 geometry, in chip memory, and freed at expunge. Rows are a whole number of
 longs wide, so a shape of any size has one (lean drew arrows wider than 32
 pixels span by span). Without a template (32 kept, or no chip memory) the
 same bits are built in other memory and painted run by run: the same
 pixels. The tape button images are in the round frame module
 (imspec_rare.c); the cycle image and the drive images keep their lines.
**************************************************************************/

#define VTEMPLATES 32
static struct { ULONG id; ULONG *data; ULONG size; } vt[VTEMPLATES];
static UBYTE nvt;

void zune_vector_free_templates(void)
{
    while (nvt > 0)
    {
        nvt--;
        FreeMem(vt[nvt].data, vt[nvt].size);
    }
}

/* A shape's id holds all it is made from: the kind and a number */
/* (sub a signed word: an arrow box too small gives n <= 0, drawn as
 * nothing) */
#define VT_ID(kind, sub) ((ULONG)(kind) << 16 | (UWORD)(sub))
#define VT_ARROWV 1     /* n * 2 + up */
#define VT_ARROWH 2     /* n * 2 + leftwards */
#define VT_GLYPH 3      /* the glyph's number */
#define VT_MX 4         /* selected * 4 + the letter: T, B, F, I */

/* MUI 3.8/3.9 built-in glyphs at their natural size, one bit per pixel
 * with the most significant bit leftmost: width, height, the rows */
static const UWORD checkmark_glyph[] = {
    13, 7, 0x0038, 0x0060, 0x00c0, 0xe180, 0x7300, 0x3e00, 0x1c00
};
/* MUI 3.9's popup arrow, measured in a PopButton(MUII_PopUp) */
static const UWORD popup_glyph[] = {
    10, 10, 0x0000, 0xc0c0, 0x6180, 0x3300, 0x1e00, 0x0c00, 0x0000, 0xffc0,
    0xffc0, 0x0000
};
static const UWORD popfile_glyph[] = {
    12, 10, 0xff00, 0xc180, 0xc140, 0xc120, 0xc1f0, 0xc030, 0xc030, 0xc030,
    0xc030, 0xfff0
};
static const UWORD popdrawer_glyph[] = {
    12, 9, 0x03c0, 0x0420, 0xf810, 0xfc10, 0xc7f0, 0xc010, 0xc010, 0xc010,
    0xfff0
};
static const UWORD *const glyphs[] = {
    checkmark_glyph, popup_glyph, popfile_glyph, popdrawer_glyph
};

/* MUI 3.8/3.9 radio button: top-left edge (T), bottom-right edge (B),
 * the inside (I) and the selected fill (F), swapping the edge pens with
 * the state. The inside is painted with the background pen and the fill
 * only when selected; the corners outside the outline stay transparent,
 * as MUI 3.9 shows on a white IBrowse page. */
static const char mx_map[9][18] = {
    "..TTTTTTTTTTTTB..",
    ".TTIIIIIIIIIIIBB.",
    "TTIIIFFFFFFFIIIBB",
    "TTIIFFFFFFFFFIIBB",
    "TTIIFFFFFFFFFIIBB",
    "TTIIFFFFFFFFFIIBB",
    "TTIIIFFFFFFFIIIBB",
    ".TTIIIIIIIIIIIBB.",
    "..TBBBBBBBBBBBB..",
};
static const char mx_letters[] = "TBFI";
/* The pens of T, B, F, I: unselected, selected */
static const UBYTE mx_pens[2][4] = {
    {MPEN_SHINE, MPEN_SHADOW, MPEN_BACKGROUND, MPEN_BACKGROUND},
    {MPEN_SHADOW, MPEN_SHINE, MPEN_FILL, MPEN_BACKGROUND},
};

/* Set the bits [x0, x1] of a row, clipped to [0, max] */
static void vt_span(ULONG *row, LONG x0, LONG x1, LONG max)
{
    if (x0 < 0)
        x0 = 0;
    if (x1 > max)
        x1 = max;
    for (; x0 <= x1; x0++)
        row[x0 >> 5] |= 0x80000000UL >> (x0 & 31);
}

/* The bits of a shape (rows of stride longs, cleared) */
static void vt_build(ULONG *rows, LONG stride, ULONG id)
{
    LONG sub = (WORD)id, n = sub >> 1, r, x, y;

    switch (id >> 16)
    {
    case VT_ARROWV:
        /* MUI 3.8/3.9's arrows are chevrons with two pixel wide strokes:
         * n rows of two strokes in 2n columns */
        for (r = 0; r < n; r++)
        {
            ULONG *row = rows + ((sub & 1) ? r : n - 1 - r) * stride;

            vt_span(row, n - 1 - r, n - r, 2 * n - 1);
            vt_span(row, n - 1 + r, n + r, 2 * n - 1);
        }
        break;
    case VT_ARROWH:
        /* n columns in 2n rows */
        for (r = 0; r < n; r++)
        {
            x = (sub & 1) ? n - 1 - r : r - 1;
            vt_span(rows + r * stride, x, x + 1, n - 1);
            vt_span(rows + (2 * n - 1 - r) * stride, x, x + 1, n - 1);
        }
        break;
    case VT_GLYPH:
        {
            const UWORD *g = glyphs[sub];

            for (y = 0; y < g[1]; y++)
                rows[y] = (ULONG)g[2 + y] << 16;
        }
        break;
    default:
        /* the radio button's letters drawn in the pen of the letter
         * sub & 3 (letters sharing a pen are drawn together) */
        {
            const UBYTE *pens = mx_pens[sub >> 2];
            UBYTE pen = pens[sub & 3];

            for (y = 0; y < 9; y++, rows++)
                for (x = 0; x < 17; x++)
                {
                    const char *c = strchr(mx_letters, mx_map[y][x]);

                    if (c && pens[c - mx_letters] == pen)
                        *rows |= 0x80000000UL >> x;
                }
        }
        break;
    }
}

/**************************************************************************
 Paint the shape id centred in the box (left, top, width, height) and
 clipped to it, in its pen (MPEN_TEXT; the radio letters their own). The
 image drawing functions have the same arguments, so they jump here.
**************************************************************************/
static void vt_draw(struct MUI_RenderInfo *mri, ULONG id, LONG left,
    LONG top, LONG width, LONG height)
{
    struct RastPort *rp = mri->mri_RastPort;
    LONG sub = (WORD)id, n = sub >> 1, w, h, x, y, l, r, t, b, i, stride;
    ULONG size, *data = NULL;
    MPen pen = MPEN_TEXT;
    UBYTE mode;

    switch (id >> 16)
    {
    case VT_ARROWV:
        w = 2 * n;
        h = n;
        break;
    case VT_ARROWH:
        w = n;
        h = 2 * n;
        break;
    case VT_GLYPH:
        w = glyphs[sub][0];
        h = glyphs[sub][1];
        break;
    default:
        w = 17;
        h = 9;
        pen = mx_pens[sub >> 2][sub & 3];
        break;
    }
    x = left + (width - w) / 2;
    y = top + (height - h) / 2;
    l = MAX(x, left);
    r = MIN(x + w - 1, left + width - 1);
    t = MAX(y, top);
    b = MIN(y + h - 1, top + height - 1);
    if (l > r || t > b)
        return;
    stride = (w + 31) >> 5;
    size = h * stride * 4;
    for (i = 0; i < nvt; i++)
        if (vt[i].id == id)
        {
            data = vt[i].data;
            break;
        }
    if (!data)
    {
        Forbid();
        for (i = 0; i < nvt && vt[i].id != id; i++)
            ;
        if (i < nvt)
            data = vt[i].data;
        else if (nvt < VTEMPLATES
            && (data = AllocMem(size, MEMF_CHIP | MEMF_CLEAR)))
        {
            vt_build(data, stride, id);
            vt[nvt].id = id;
            vt[nvt].data = data;
            vt[nvt].size = size;
            nvt++;
        }
        Permit();
    }
    pen = mri->mri_Pens[pen];
    if (rp->FgPen != (BYTE)pen)
        SetAPen(rp, pen);
    mode = rp->DrawMode;
    if (mode != JAM1)
        SetDrMd(rp, JAM1);
    if (data)
    {
        /* BltTemplate takes a bit offset below 16 in the first word */
        BltTemplate((CONST PLANEPTR)((UWORD *)(data + (t - y) * stride)
            + ((l - x) >> 4)), (l - x) & 15, stride * 4, rp, l, t,
            r - l + 1, b - t + 1);
    }
    else if ((data = AllocMem(size, MEMF_ANY | MEMF_CLEAR)))
    {
        /* no template: the same bits painted run by run */
        vt_build(data, stride, id);
        for (; t <= b; t++)
        {
            const ULONG *row = data + (t - y) * stride;

            for (i = l; i <= r; i = n)
            {
#define VT_BIT(px) (row[((px) - x) >> 5] & (0x80000000UL >> (((px) - x) & 31)))
                while (i <= r && !VT_BIT(i))
                    i++;
                for (n = i; n <= r && VT_BIT(n); n++)
                    ;
                if (n > i)
                    RectFill(rp, i, t, n - 1, t);
#undef VT_BIT
            }
        }
        FreeMem(data, size);
    }
    if (mode != JAM1)
        SetDrMd(rp, mode);
}

/* The arrows: n rows (columns) as the box allows, centred; none when n is
 * 0 (vt_draw's box is then empty) */
static void arrowup_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    LONG n = MIN(height, width / 2);

    vt_draw(mri, VT_ID(VT_ARROWV, n * 2 + 1), left, top, width, height);
}

static void arrowdown_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    LONG n = MIN(height, width / 2);

    vt_draw(mri, VT_ID(VT_ARROWV, n * 2), left, top, width, height);
}

static void arrowleft_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    LONG n = MIN(height / 2, width);

    vt_draw(mri, VT_ID(VT_ARROWH, n * 2 + 1), left, top, width, height);
}

static void arrowright_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    LONG n = MIN(height / 2, width);

    vt_draw(mri, VT_ID(VT_ARROWH, n * 2), left, top, width, height);
}

static void checkbox_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    /* the check mark only in the selected state */
    if (state == IDS_SELECTED)
        vt_draw(mri, VT_ID(VT_GLYPH, 0), left, top, width, height);
}

static void popup_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    vt_draw(mri, VT_ID(VT_GLYPH, 1), left, top, width, height);
}

static void popfile_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    vt_draw(mri, VT_ID(VT_GLYPH, 2), left, top, width, height);
}

static void popdrawer_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    vt_draw(mri, VT_ID(VT_GLYPH, 3), left, top, width, height);
}

/* One template per pen of a state; I shares the background pen with F
 * when unselected */
static void mx_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    LONG selected = state == IDS_SELECTED, p;

    for (p = 0; p < 3 + selected; p++)
        vt_draw(mri, VT_ID(VT_MX, selected * 4 + p), left, top, width,
            height);
}

static void cycle_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
    LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int bottom = top + height - 1;
    int right = left + width - 1;

    SetAPen(rport, mri->mri_Pens[MPEN_TEXT]);

    Move(rport, left, top + 1);
    Draw(rport, left, bottom - 1);
    Move(rport, left + 1, top);
    Draw(rport, left + 1, bottom);
    Draw(rport, right - 7, bottom);
    Move(rport, right - 7, bottom - 1);
    Draw(rport, right - 6, bottom - 1);
    Move(rport, left + 2, top);
    Draw(rport, right - 7, top);
    Move(rport, right - 7, top + 1);
    Draw(rport, right - 6, top + 1);

    /* The small arrow */
    Move(rport, right - 6 - 3, top + 2);
    Draw(rport, right - 7 + 3, top + 2);
    Move(rport, right - 6 - 2, top + 3);
    Draw(rport, right - 7 + 2, top + 3);
    Move(rport, right - 6 - 1, top + 4);
    Draw(rport, right - 7 + 1, top + 4);

    /* The right bar */
    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    Move(rport, right - 1, top);
    Draw(rport, right - 1, bottom);
    SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
    Move(rport, right, top);
    Draw(rport, right, bottom);
}

/* The drive images 1:10 to 1:16 (Drawer, HardDisk, Disk, Chip, Volume,
 * Network, Assign: Volumelist shows them), drawn as lean drew them;
 * Network and Assign are Drawer (lean had its code three times) */
static void stddrawer_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right = left + width - 1;
    int bottom = top + height - 1;
    int x, y;

#define XX(x)   (x * (width - 1) / 100)
#define YY(y)   (y * (height - 1) / 100)
#define YOFF    45
#define XOFF    24

    for (y = 0; y < YY(YOFF); y++)
    {
        x = (YY(XOFF) > 1) ? XX(XOFF) * y / (YY(YOFF) - 1) : 1;
        SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
        RectFill(rport, left + XX(XOFF) - x, top + y, right - XX(XOFF) + x,
            top + y);
        SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
        WritePixel(rport, left + XX(XOFF) - x, top + y);
        WritePixel(rport, right - XX(XOFF) + x, top + y);
    }


    SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
    RectFill(rport, left, top + YY(YOFF), right, bottom);

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    Move(rport, left + XX(XOFF), top);
    Draw(rport, right - XX(XOFF), top);

    Move(rport, left, top + YY(YOFF));
    Draw(rport, left, bottom);
    Draw(rport, right, bottom);
    Draw(rport, right, top + YY(YOFF));
    Draw(rport, left, top + YY(YOFF));

#undef XX
#undef YY
#undef YOFF
#undef XOFF
}


static void drawer_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right = left + width - 1;

    stddrawer_draw(mri, left, top, width, height);

#define XX(x) x * (width - 1) / 100
#define YY(y) y * (height - 1) / 100

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    Move(rport, left + XX(40), top + YY(75));
    Draw(rport, right - XX(40), top + YY(75));

#undef XX
#undef YY

}

static void harddisk_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;

    stddrawer_draw(mri, left, top, width, height);

#define XX(x) x * (width - 1) / 100
#define YY(y) y * (height - 1) / 100

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    Move(rport, left + XX(20), top + YY(75));
    Draw(rport, left + XX(30), top + YY(75));

#undef XX
#undef YY
}

static void disk_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
    LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right = left + width - 1;
    int bottom = top + height - 1;
    int x, y;

#define XX(x)   (x * (width - 1) / 100)
#define YY(y)   (y * (height - 1) / 100)
#define YOFF    29
#define XOFF    25

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    for (y = 0; y < YY(YOFF); y++)
    {
        x = (YY(YOFF) > 1) ? XX(XOFF) * y / (YY(YOFF) - 1) : 1;
        RectFill(rport, left, top + y, right - XX(XOFF) + x, top + y);
    }
    RectFill(rport, left, top + YY(YOFF), right, bottom);

    SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
    RectFill(rport, left + XX(33), top, right - XX(33), top + YY(YOFF));
    RectFill(rport, left + XX(25), top + YY(60), right - XX(25), bottom);

#undef XX
#undef YY
#undef YOFF
#undef XOFF
}

static void ram_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
    LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right, bottom;

#define XX(x) left + x * (width - 1) / 100
#define YY(y) top  + y * (height - 1) / 100

    right = left + width - 1;
    bottom = top + height - 1;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    RectFill(rport, left, top, right, bottom);

    SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
    Move(rport, XX(33), YY(25));
    Draw(rport, XX(33), YY(75));

    Move(rport, XX(71), YY(25));
    Draw(rport, XX(61), YY(25));
    Draw(rport, XX(61), YY(75));
    Draw(rport, XX(71), YY(75));

#undef XX
#undef YY
}

static void volume_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int right, bottom;

#define XX(x) left + x * (width - 1) / 100
#define YY(y) top  + y * (height - 1) / 100

    right = left + width - 1;
    bottom = top + height - 1;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    RectFill(rport, left, top, right, bottom);

    SetAPen(rport, mri->mri_Pens[MPEN_SHINE]);
    Move(rport, XX(10), YY(25));
    Draw(rport, XX(10), YY(50));
    Draw(rport, XX(19), YY(75));
    Draw(rport, XX(28), YY(50));
    Draw(rport, XX(28), YY(25));

    Move(rport, XX(46), YY(25));
    Draw(rport, XX(64), YY(25));
    Draw(rport, XX(64), YY(75));
    Draw(rport, XX(46), YY(75));
    Draw(rport, XX(46), YY(25));

    Move(rport, XX(80), YY(25));
    Draw(rport, XX(80), YY(75));
    Draw(rport, XX(92), YY(75));

#undef XX
#undef YY
}



/* Sizes as MUI 3.9 reports them for the built-in images
 * (tests/native/imgsizes.c): width, height */
static const UBYTE vector_sizes[][2] = {
    {10, 5}, {10, 5}, {4, 8}, {4, 8}, {13, 7}, {17, 9}, {15, 8}, {10, 10},
    {12, 10}, {12, 9},
    {13, 8}, {13, 8}, {12, 8}, {12, 5}, {12, 5}, {13, 8}, {13, 8},
    {4, 7}, {4, 7}, {2, 7}, {6, 7}, {7, 7}, {7, 4}, {7, 4},
};
#define VECTOR_COUNT (sizeof(vector_sizes) / sizeof(vector_sizes[0]))

static const VECTOR_DRAW_FUNC vector_draws[17] = {
    arrowup_draw, arrowdown_draw, arrowleft_draw, arrowright_draw,
    checkbox_draw, mx_draw, cycle_draw, popup_draw, popfile_draw,
    popdrawer_draw, drawer_draw, harddisk_draw, disk_draw, ram_draw,
    volume_draw, drawer_draw, drawer_draw,
};

static struct MUI_ImageSpec_intern *vector_spec(LONG vect,
    VECTOR_DRAW_FUNC draw)
{
    struct MUI_ImageSpec_intern *spec;

    if ((spec = mui_alloc_struct(struct MUI_ImageSpec_intern)))
    {
        spec->type = IST_VECTOR;
        spec->u.vect.type = vect;
        spec->u.vect.draw = draw;
    }
    return spec;
}

/* The tape button images 1:17 to 1:23: the round frame module's (no
 * drawing without it) */
static __attribute__((noinline)) struct MUI_ImageSpec_intern *rare_vector(
    LONG vect)
{
    const struct ZuneFramesModule *m = zune_frames_module();

    return vector_spec(vect, m ? m->images[vect - 17] : NULL);
}

struct MUI_ImageSpec_intern *zune_imspec_create_vector(LONG vect)
{
    if (vect < 0 || vect >= VECTOR_COUNT)
        return NULL;
    if (vect >= 17)
        return rare_vector(vect);
    return vector_spec(vect, vector_draws[vect]);
}

BOOL zune_imspec_vector_get_minmax(struct MUI_ImageSpec_intern *spec,
    struct MUI_MinMax *minmax)
{
    ULONG v;

    if (!spec || spec->type != IST_VECTOR
        || (v = spec->u.vect.type) >= VECTOR_COUNT)
        return FALSE;
    minmax->MinWidth = minmax->DefWidth = vector_sizes[v][0];
    minmax->MinHeight = minmax->DefHeight = vector_sizes[v][1];
    minmax->MaxWidth = MUI_MAXMAX;
    minmax->MaxHeight = MUI_MAXMAX;
    return TRUE;
}
