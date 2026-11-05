#ifndef ZUNE_TITLEGLYPHS_H
#define ZUNE_TITLEGLYPHS_H
/*
    Copyright (C) 2026, The AROS Development Team.
    All rights reserved.

    The artwork of the extra window title bar gadgets (Popup, Prefs,
    Snapshot, Iconify), shared by Window.mui and the Zune prefs program.
    Our own drawings in the style of Intuition 3.1's zoom and depth
    gadgets: 13 pixels wide, lines in SHADOW, details in SHINE, areas in
    FILL. Each gadget has a normal and a pressed drawing.

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
    /* Popup: a menu title with an arrow pointing down */
    TGR(TG_SHADOW, 0, 0, 12, 2), TGR(TG_SHINE, 1, 1, 11, 1),
    TGR(TG_SHADOW, 3, 4, 9, 4), TGR(TG_SHADOW, 4, 5, 8, 5),
    TGR(TG_SHADOW, 5, 6, 7, 6), TGR(TG_SHADOW, 6, 7, 6, 7),
    /* pressed: the menu has dropped down, one item highlighted */
    TGR(TG_SHADOW, 0, 0, 12, 2), TGR(TG_SHINE, 1, 1, 11, 1),
    TGR(TG_SHADOW, 2, 2, 10, 7), TGR(TG_SHINE, 3, 3, 9, 6),
    TGR(TG_SHADOW, 4, 4, 8, 4), TGR(TG_FILL, 3, 6, 9, 6),
    /* Prefs: two sliders */
    TGR(TG_SHADOW, 0, 1, 12, 1), TGR(TG_SHADOW, 2, 0, 5, 2),
    TGR(TG_SHINE, 3, 1, 4, 1),
    TGR(TG_SHADOW, 0, 6, 12, 6), TGR(TG_SHADOW, 7, 5, 10, 7),
    TGR(TG_SHINE, 8, 6, 9, 6),
    /* pressed: both knobs moved */
    TGR(TG_SHADOW, 0, 1, 12, 1), TGR(TG_SHADOW, 7, 0, 10, 2),
    TGR(TG_SHINE, 8, 1, 9, 1),
    TGR(TG_SHADOW, 0, 6, 12, 6), TGR(TG_SHADOW, 2, 5, 5, 7),
    TGR(TG_SHINE, 3, 6, 4, 6),
    /* Snapshot: a window with four corner marks around it */
    TGR(TG_SHADOW, 3, 2, 9, 5), TGR(TG_SHINE, 4, 3, 8, 4),
    TGR(TG_SHADOW, 0, 0, 2, 0), TGR(TG_SHADOW, 0, 1, 0, 1),
    TGR(TG_SHADOW, 10, 0, 12, 0), TGR(TG_SHADOW, 12, 1, 12, 1),
    TGR(TG_SHADOW, 0, 7, 2, 7), TGR(TG_SHADOW, 0, 6, 0, 6),
    TGR(TG_SHADOW, 10, 7, 12, 7), TGR(TG_SHADOW, 12, 6, 12, 6),
    /* pressed: the marks closed in on the window */
    TGR(TG_SHADOW, 3, 2, 9, 5), TGR(TG_SHINE, 4, 3, 8, 4),
    TGR(TG_SHADOW, 1, 1, 3, 1), TGR(TG_SHADOW, 1, 2, 1, 2),
    TGR(TG_SHADOW, 9, 1, 11, 1), TGR(TG_SHADOW, 11, 2, 11, 2),
    TGR(TG_SHADOW, 1, 6, 3, 6), TGR(TG_SHADOW, 1, 5, 1, 5),
    TGR(TG_SHADOW, 9, 6, 11, 6), TGR(TG_SHADOW, 11, 5, 11, 5),
    /* Iconify: a window with an icon in its lower left corner */
    TGR(TG_SHADOW, 0, 0, 12, TGB(0)), TGR(TG_FILL, 1, 1, 11, TGB(1)),
    TGR(TG_SHADOW, 0, TGB(3), 4, TGB(0)), TGR(TG_SHINE, 1, TGB(2), 3, TGB(1)),
    /* pressed: only the icon is left */
    TGR(TG_SHADOW, 0, TGB(3), 6, TGB(0)), TGR(TG_SHINE, 1, TGB(2), 5, TGB(1)),
};

/* Start of each drawing in rectangles: gadget * 2 + pressed */
static const unsigned char TitleGlyphStart[9] = {
    0, 6, 12, 18, 24, 34, 44, 48, 50
};

/* Draw a glyph with its left column at x, its rows top..bottom, clipped
 * to the rows clip0..clip1. pens[] maps TG_SHADOW..TG_BACK to pens. */
static void DrawTitleGlyph(struct RastPort *rp, const UWORD *pens,
    int glyph, WORD x, WORD top, WORD bottom, WORD clip0, WORD clip1)
{
    const unsigned char *r = TitleGlyphData + 3 * TitleGlyphStart[glyph];
    const unsigned char *end = TitleGlyphData + 3 * TitleGlyphStart[glyph + 1];

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
