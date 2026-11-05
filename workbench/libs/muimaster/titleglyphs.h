#ifndef ZUNE_TITLEGLYPHS_H
#define ZUNE_TITLEGLYPHS_H
/*
    Copyright (C) 2026, The AROS Development Team.
    All rights reserved.

    The artwork of the extra window title bar gadgets (Popup, Prefs,
    Snapshot, Iconify), shared by Window.mui and the Zune prefs program.
    Our own drawings, drawn from a description of MUI 3.8's concepts and
    proportions (no pixel map), in the style of Intuition 3.1's zoom and
    depth gadgets: 13 pixels wide, lines in SHADOW, details in SHINE,
    areas in FILL. Each gadget has a normal and a pressed drawing.

    A drawing is a list of filled rectangles, 3 bytes each: the pen, then
    x0 << 4 | x1, then y0 << 4 | y1. x counts 0..12 from the glyph's left
    column. A y below 8 counts down from the glyph's top row, 8..15 counts
    up from its bottom row (15 = the bottom row). Popup, Prefs and
    Snapshot are 8 rows tall (bottom = top + 7), Iconify stretches with
    the title bar.
*/

#include <graphics/rastport.h>

#define TG_SHADOW 0
#define TG_SHINE  1
#define TG_FILL   2
#define TG_BACK   3

/* The gadgets in title bar order, left to right */
#define TG_POPUP    0
#define TG_PREFS    1
#define TG_SNAPSHOT 2
#define TG_ICONIFY  3

#define TG_WIDTH 13

#define TGR(p, x0, y0, x1, y1) (p), ((x0) << 4 | (x1)), ((y0) << 4 | (y1))
#define TGB(n) (15 - (n))   /* n rows above the bottom row */

static const unsigned char TitleGlyphData[] = {
    /* Popup: a hollow triangle pointing down, its tip on a flat bar */
#define TG_TRIANGLE(d) \
    TGR(TG_SHADOW, 2, 0 + d, 10, 0 + d), \
    TGR(TG_SHADOW, 3, 1 + d, 3, 1 + d), TGR(TG_SHADOW, 9, 1 + d, 9, 1 + d), \
    TGR(TG_SHADOW, 4, 2 + d, 4, 2 + d), TGR(TG_SHADOW, 8, 2 + d, 8, 2 + d), \
    TGR(TG_SHADOW, 5, 3 + d, 5, 3 + d), TGR(TG_SHADOW, 7, 3 + d, 7, 3 + d), \
    TGR(TG_SHADOW, 6, 4 + d, 6, 4 + d)
    TG_TRIANGLE(0),
    TGR(TG_SHADOW, 1, 5, 11, 7), TGR(TG_SHINE, 2, 6, 10, 6),
    /* pressed: the triangle one row lower, the bar wider */
    TG_TRIANGLE(1),
    TGR(TG_SHADOW, 0, 5, 12, 7), TGR(TG_SHINE, 1, 6, 11, 6),
#undef TG_TRIANGLE
    /* Prefs: a panel leaning forward, a compartment at its right side, a
     * smaller leaning shape with a highlight inside, the slanted left
     * band dithered; pressed looks the same */
    TGR(TG_SHADOW, 2, 0, 12, 0), TGR(TG_SHADOW, 0, 7, 10, 7),
    TGR(TG_SHADOW, 2, 1, 2, 2), TGR(TG_SHADOW, 1, 3, 1, 4),
    TGR(TG_SHADOW, 0, 5, 0, 6), TGR(TG_SHADOW, 12, 1, 12, 2),
    TGR(TG_SHADOW, 11, 3, 11, 4), TGR(TG_SHADOW, 10, 5, 10, 6),
    TGR(TG_SHADOW, 9, 1, 9, 2), TGR(TG_SHADOW, 8, 3, 8, 4),
    TGR(TG_SHADOW, 7, 5, 7, 6),
    TGR(TG_FILL, 3, 1, 4, 2), TGR(TG_FILL, 2, 3, 3, 4),
    TGR(TG_FILL, 1, 5, 2, 6),
    TGR(TG_BACK, 3, 1, 3, 1), TGR(TG_BACK, 4, 2, 4, 2),
    TGR(TG_BACK, 3, 3, 3, 3), TGR(TG_BACK, 2, 4, 2, 4),
    TGR(TG_BACK, 1, 5, 1, 5), TGR(TG_BACK, 2, 6, 2, 6),
    TGR(TG_SHADOW, 5, 2, 7, 2), TGR(TG_SHADOW, 4, 3, 6, 4),
    TGR(TG_SHADOW, 3, 5, 5, 5), TGR(TG_SHINE, 5, 3, 5, 4),
    /* Snapshot: lines left and right, arrowheads pointing in at a small
     * hollow block */
#define TG_ARROWS(l, r) \
    TGR(TG_SHADOW, l, 0, l, 7), \
    TGR(TG_SHINE, l + 1, 2, l + 1, 5), TGR(TG_SHINE, l + 2, 3, l + 2, 4), \
    TGR(TG_SHADOW, l + 1, 1, l + 1, 1), TGR(TG_SHADOW, l + 2, 2, l + 2, 2), \
    TGR(TG_SHADOW, l + 3, 3, l + 3, 4), TGR(TG_SHADOW, l + 2, 5, l + 2, 5), \
    TGR(TG_SHADOW, l + 1, 6, l + 1, 6), \
    TGR(TG_SHADOW, r, 0, r, 7), \
    TGR(TG_SHINE, r - 1, 2, r - 1, 5), TGR(TG_SHINE, r - 2, 3, r - 2, 4), \
    TGR(TG_SHADOW, r - 1, 1, r - 1, 1), TGR(TG_SHADOW, r - 2, 2, r - 2, 2), \
    TGR(TG_SHADOW, r - 3, 3, r - 3, 4), TGR(TG_SHADOW, r - 2, 5, r - 2, 5), \
    TGR(TG_SHADOW, r - 1, 6, r - 1, 6)
    TG_ARROWS(0, 12),
    TGR(TG_SHADOW, 5, 2, 7, 5), TGR(TG_FILL, 6, 3, 6, 4),
    /* pressed: the arrows touch the block, which is full height */
    TG_ARROWS(1, 11),
    TGR(TG_SHADOW, 5, 0, 7, 7), TGR(TG_FILL, 6, 1, 6, 6),
#undef TG_ARROWS
    /* Iconify: a window with a tiny window in its lower left corner */
    TGR(TG_SHADOW, 0, 0, 12, TGB(0)), TGR(TG_FILL, 1, 1, 11, TGB(1)),
    TGR(TG_SHADOW, 2, TGB(4), 5, TGB(2)), TGR(TG_SHINE, 3, TGB(3), 4, TGB(3)),
    /* pressed: the outline has shrunk around the tiny window */
    TGR(TG_SHADOW, 0, TGB(6), 7, TGB(0)), TGR(TG_FILL, 1, TGB(5), 6, TGB(1)),
    TGR(TG_SHADOW, 2, TGB(4), 5, TGB(2)), TGR(TG_SHINE, 3, TGB(3), 4, TGB(3)),
};

/* First rectangle and end of each drawing: gadget * 2 + pressed */
static const unsigned char TitleGlyphRange[8][2] = {
    { 0, 10 }, { 10, 20 },   /* Popup */
    { 20, 44 }, { 20, 44 },  /* Prefs: pressed the same */
    { 44, 62 }, { 62, 80 },  /* Snapshot */
    { 80, 84 }, { 84, 88 }   /* Iconify */
};
typedef char TitleGlyphSizeCheck[sizeof(TitleGlyphData) == 3 * 88 ? 1 : -1];

/* Draw a glyph with its left column at x, its rows top..bottom, clipped
 * to the rows clip0..clip1. pens[] maps TG_SHADOW..TG_BACK to pens. */
static void DrawTitleGlyph(struct RastPort *rp, const UWORD *pens,
    int glyph, WORD x, WORD top, WORD bottom, WORD clip0, WORD clip1)
{
    const unsigned char *r = TitleGlyphData + 3 * TitleGlyphRange[glyph][0];
    const unsigned char *end = TitleGlyphData + 3 * TitleGlyphRange[glyph][1];

    for (; r < end; r += 3)
    {
        WORD y0 = r[2] >> 4, y1 = r[2] & 15;
        y0 = y0 < 8 ? top + y0 : bottom - (15 - y0);
        y1 = y1 < 8 ? top + y1 : bottom - (15 - y1);
        if (y0 < clip0) y0 = clip0;
        if (y1 > clip1) y1 = clip1;
        if (y0 > y1)
            continue;
        SetAPen(rp, pens[r[0]]);
        RectFill(rp, x + (r[1] >> 4), y0, x + (r[1] & 15), y1);
    }
}

#endif
