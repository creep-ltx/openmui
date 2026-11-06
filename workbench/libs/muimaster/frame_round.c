/*
    Copyright  2002-2026, The AROS Development Team.
    Copyright  1999, David Le Corfec.
    All rights reserved.

*/

/* The rounded frame types: 5 (round bevel), 7 (round thick border), 8
 * (round thin border), A (semi-round bevel) and B (rounded), split out of
 * frame.c. No default setting uses them; MUI 3.8 programs only get them
 * from a user's settings, MUI 4/5 programs also from frame specs. The
 * native build has them in the round frame module (modules.h,
 * classes/zframes.c), which zune_zframe_get_with_state() asks for their
 * drawing functions (zune_round_frames); other builds include this file
 * in frame.c. */

#ifdef ZUNE_FRAMES_MODULE
#include <exec/types.h>
#include <proto/graphics.h>
#include "mui.h"
#include "muimaster_intern.h"
#include "datatypescache.h"
#include "frame.h"
#include "modules.h"

static inline void draw_horizontal_line(struct RastPort *rp, int x1, int x2,
                                        int y, ULONG pen)
{
    if (x1 > x2)
        return;
    if (rp->FgPen != (BYTE)pen)
        SetAPen(rp, pen);
    RectFill(rp, x1, y, x2, y);
}

static inline void draw_vertical_line(struct RastPort *rp, int x, int y1,
                                      int y2, ULONG pen)
{
    if (y1 > y2)
        return;
    if (rp->FgPen != (BYTE)pen)
        SetAPen(rp, pen);
    Move(rp, x, y1);
    Draw(rp, x, y2);
}

/* copies of frame.c's helpers of the same names */
static void rect_draw(struct dt_frame_image *fi, struct MUI_RenderInfo *mri,
                      int left, int top, int width, int height,
                      MPen preset_color)
{
    struct RastPort *rp = mri->mri_RastPort;

    if (width <= 0 || height <= 0)
        return; /* Safety check */

    SetAPen(rp, mri->mri_Pens[preset_color]);

    if (width == 1 || height == 1) {
        /* For thin lines, fill the entire area */
        RectFill(rp, left, top, left + width - 1, top + height - 1);
    } else {
        /* For borders, draw individual edges efficiently */
        RectFill(rp, left, top, left + width - 1, top); /* Top edge */
        RectFill(rp, left, top + height - 1, left + width - 1,
                 top + height - 1);                          /* Bottom edge */
        RectFill(rp, left, top + 1, left, top + height - 2); /* Left edge */
        RectFill(rp, left + width - 1, top + 1, left + width - 1,
                 top + height - 2); /* Right edge */
    }
}

static void button_draw(struct dt_frame_image *fi, struct MUI_RenderInfo *mri,
                        int left, int top, int width, int height,
                        MPen ul_preset, MPen lr_preset)
{
    struct RastPort *rp = mri->mri_RastPort;

    if (!rp || width <= 2 || height <= 2)
        return; /* Safety check - need minimum size and valid rastport */

    /* Draw upper-left edges (the line helpers set the pen when it
     * differs from the rastport's) */
    draw_vertical_line(rp, left, top, top + height - 2, mri->mri_Pens[ul_preset]);
    draw_horizontal_line(rp, left, left + width - 2, top,
                         mri->mri_Pens[ul_preset]);

    /* Draw lower-right edges, short of the top-right and bottom-left
     * corners */
    draw_vertical_line(rp, left + width - 1, top + 1, top + height - 1,
                       mri->mri_Pens[lr_preset]);
    draw_horizontal_line(rp, left + 1, left + width - 1, top + height - 1,
                         mri->mri_Pens[lr_preset]);

    /* Those two corners are MPEN_BACKGROUND in MUI 3.9 even where another
     * background lies beneath them (a pressed button's bottom-left corner,
     * a white parent's top-right one: tests/native/cornerbg.c). Area tells
     * when its background fill has just painted them with that pen. */
    {
        UBYTE known = fi ? fi->corners_bg : 0;

        if ((known & (FRAME_CORNER_TR | FRAME_CORNER_BL))
            != (FRAME_CORNER_TR | FRAME_CORNER_BL))
            SetAPen(rp, mri->mri_Pens[MPEN_BACKGROUND]);
        if (!(known & FRAME_CORNER_BL))
            WritePixel(rp, left, top + height - 1);
        if (!(known & FRAME_CORNER_TR))
            WritePixel(rp, left + width - 1, top);
    }
}
#endif

/**************************************************************************
 5 : FST_ROUND_BEVEL
**************************************************************************/
/**
 * Draw a rounded bevel frame
 * @param ul Upper-left pen (light for raised, dark for recessed)
 * @param lr Lower-right pen (dark for raised, light for recessed)
 */
static void round_bevel_draw(struct dt_frame_image *fi,
                             struct MUI_RenderInfo *mri, int left, int top,
                             int width, int height, MPen ul, MPen lr)
{
    struct RastPort *rp = mri->mri_RastPort;

    if (width <= 8 || height <= 4)
        return; /* Safety check - need minimum size for rounded effect */

    /* Clear corner areas with background - grouped operations */
    SetAPen(rp, mri->mri_Pens[MPEN_BACKGROUND]);
    RectFill(rp, left, top, left + 3, top + height - 1);
    RectFill(rp, left + width - 4, top, left + width - 1, top + height - 1);

    /* Draw upper-left edges and corners - grouped by pen */
    SetAPen(rp, mri->mri_Pens[ul]);
    RectFill(rp, left, top + 2, left + 1, top + height - 3); /* Left edge */
    RectFill(rp, left + 1, top + 1, left + 2, top + 1);      /* Top-left corner */
    RectFill(rp, left + 1, top + height - 2, left + 2,
             top + height - 2); /* Bottom-left corner */
    RectFill(rp, left + 2, top + height - 1, left + 2,
             top + height - 1);                         /* Bottom-left pixel */
    RectFill(rp, left + 2, top, left + width - 6, top); /* Top edge */

    /* Draw lower-right edges and corners - grouped by pen */
    SetAPen(rp, mri->mri_Pens[lr]);
    RectFill(rp, left + width - 2, top + 2, left + width - 1,
             top + height - 3); /* Right edge */
    RectFill(rp, left + width - 3, top + 1, left + width - 2,
             top + 1); /* Top-right corner */
    RectFill(rp, left + width - 3, top + height - 2, left + width - 2,
             top + height - 2); /* Bottom-right corner */
    RectFill(rp, left + 3, top + height - 1, left + width - 6,
             top + height - 1); /* Bottom edge */
    RectFill(rp, left + width - 3, top, left + width - 3,
             top); /* Top-right pixel */
}

/**************************************************************************

**************************************************************************/
static void frame_round_bevel_up_draw(struct dt_frame_image *fi,
                                      struct MUI_RenderInfo *mri, int gl,
                                      int gt, int gw, int gh, int left, int top,
                                      int width, int height)
{
    round_bevel_draw(fi, mri, left, top, width, height, MPEN_SHINE, MPEN_SHADOW);
}

/**************************************************************************

**************************************************************************/
static void frame_round_bevel_down_draw(struct dt_frame_image *fi,
                                        struct MUI_RenderInfo *mri, int gl,
                                        int gt, int gw, int gh, int left,
                                        int top, int width, int height)
{
    round_bevel_draw(fi, mri, left, top, width, height, MPEN_SHADOW, MPEN_SHINE);
}

/**************************************************************************
 7 : FST_ROUND_THICK_BORDER
**************************************************************************/
static void round_thick_border_draw(struct dt_frame_image *fi,
                                    struct MUI_RenderInfo *mri, int left,
                                    int top, int width, int height, MPen pen1,
                                    MPen pen2, MPen pen3, MPen pen4,
                                    MPen pen5)
{
    struct RastPort *rp = mri->mri_RastPort;

    rect_draw(fi, mri, left, top, width - 2, height - 2, pen1);
    rect_draw(fi, mri, left + 2, top + 2, width - 2, height - 2, pen5);

    rect_draw(fi, mri, left, top, 2, 2, pen2);
    rect_draw(fi, mri, left, top + height - 3, 2, 2, pen2);
    rect_draw(fi, mri, left + width - 3, top, 2, 2, pen2);

    rect_draw(fi, mri, left + width - 2, top + height - 2, 2, 2, pen4);
    rect_draw(fi, mri, left + 1, top + height - 2, 2, 2, pen4);
    rect_draw(fi, mri, left + width - 2, top + 1, 2, 2, pen4);

    rect_draw(fi, mri, left + 1, top + 1, width - 2, height - 2, pen3);

    rect_draw(fi, mri, left + 2, top + 2, 2, 2, pen3);
    rect_draw(fi, mri, left + 2, top + height - 4, 2, 2, pen3);
    rect_draw(fi, mri, left + width - 4, top + height - 4, 2, 2, pen3);
    rect_draw(fi, mri, left + width - 4, top + 2, 2, 2, pen3);

    /* these points were not in the original frame.  -dlc */
    SetAPen(rp, mri->mri_Pens[pen5]);
    WritePixel(rp, left + 3, top + 3);

    SetAPen(rp, mri->mri_Pens[pen1]);
    WritePixel(rp, left + width - 4, top + height - 4);
}

/**************************************************************************

**************************************************************************/
static void frame_round_thick_border_up_draw(struct dt_frame_image *fi,
        struct MUI_RenderInfo *mri, int gl,
        int gt, int gw, int gh, int left,
        int top, int width, int height)
{
    round_thick_border_draw(fi, mri, left, top, width, height, MPEN_SHINE,
                            MPEN_HALFSHINE, MPEN_BACKGROUND, MPEN_HALFSHADOW,
                            MPEN_SHADOW);
}

/**************************************************************************

**************************************************************************/
static void frame_round_thick_border_down_draw(struct dt_frame_image *fi,
        struct MUI_RenderInfo *mri,
        int gl, int gt, int gw, int gh,
        int left, int top, int width,
        int height)
{
    round_thick_border_draw(fi, mri, left, top, width, height, MPEN_SHADOW,
                            MPEN_HALFSHADOW, MPEN_BACKGROUND, MPEN_HALFSHINE,
                            MPEN_SHINE);
}

/**************************************************************************
 8 : FST_ROUND_THIN_BORDER
**************************************************************************/
/**
 * Draw a rounded thin border - complex multi-pen effect
 * Uses 5 pens to create gradient-like rounded appearance. Pixels as MUI
 * 3.8's type 8 (3 pixels wide all round, thin and thick alike); the four
 * pixels of the top-left and the bottom-right corner diagonals, from
 * outside in, are in the pens diag.
 */
static void round_thin_border_draw(struct dt_frame_image *fi,
                                   struct MUI_RenderInfo *mri, int left,
                                   int top, int width, int height, MPen pen1,
                                   MPen pen2, MPen pen3, MPen pen4, MPen pen5,
                                   const UBYTE *diag)
{
    struct RastPort *rp = mri->mri_RastPort;
    int i;

    if (width <= 6 || height <= 6)
        return; /* Safety check */

    /* Draw main border lines */
    rect_draw(fi, mri, left, top, width - 1, height - 1, pen1);
    rect_draw(fi, mri, left + 1, top + 1, width - 1, height - 1, pen5);

    /* Draw outer corners with pen2 */
    SetAPen(rp, mri->mri_Pens[pen2]);
    RectFill(rp, left, top, left + 1, top + 1); /* Top-left outer */
    RectFill(rp, left + width - 4, top + height - 4, left + width - 3,
             top + height - 3); /* Bottom-right outer */
    WritePixel(rp, left, top + height - 2);
    WritePixel(rp, left + 2, top + height - 2);
    WritePixel(rp, left + width - 2, top);
    WritePixel(rp, left + width - 2, top + 2);

    /* Draw middle corners with pen3 */
    SetAPen(rp, mri->mri_Pens[pen3]);
    RectFill(rp, left + 1, top + 1, left + 2, top + 2); /* Top-left middle */
    RectFill(rp, left + width - 3, top + height - 3, left + width - 2,
             top + height - 2); /* Bottom-right middle */

    /* Draw inner corners and edges with pen4 */
    SetAPen(rp, mri->mri_Pens[pen4]);
    RectFill(rp, left + 2, top + 2, left + 3, top + 3); /* Top-left inner */
    RectFill(rp, left + width - 2, top + height - 2, left + width - 1,
             top + height - 1); /* Bottom-right inner */
    RectFill(rp, left + 1, top + height - 3, left + 1,
             top + height - 1); /* Left edge */
    RectFill(rp, left + width - 3, top + 1, left + width - 1,
             top + 1); /* Top edge */
    WritePixel(rp, left + 2, top + height - 3);
    WritePixel(rp, left + width - 3, top + 2);

    for (i = 0; i < 4; i++) {
        SetAPen(rp, mri->mri_Pens[diag[i]]);
        WritePixel(rp, left + i, top + i);
        WritePixel(rp, left + width - 4 + i, top + height - 4 + i);
    }
}

static const UBYTE round_thin_up_diag[4] = {
    MPEN_HALFSHINE, MPEN_BACKGROUND, MPEN_BACKGROUND, MPEN_HALFSHADOW
};
static const UBYTE round_thin_down_diag[4] = {
    MPEN_BACKGROUND, MPEN_SHADOW, MPEN_SHINE, MPEN_BACKGROUND
};

/**************************************************************************

**************************************************************************/
static void frame_round_thin_border_up_draw(struct dt_frame_image *fi,
        struct MUI_RenderInfo *mri, int gl,
        int gt, int gw, int gh, int left,
        int top, int width, int height)
{
    round_thin_border_draw(fi, mri, left, top, width, height, MPEN_SHINE,
                           MPEN_HALFSHINE, MPEN_BACKGROUND, MPEN_HALFSHADOW,
                           MPEN_SHADOW, round_thin_up_diag);
}

/**************************************************************************

**************************************************************************/
static void frame_round_thin_border_down_draw(struct dt_frame_image *fi,
        struct MUI_RenderInfo *mri,
        int gl, int gt, int gw, int gh,
        int left, int top, int width,
        int height)
{
    round_thin_border_draw(fi, mri, left, top, width, height, MPEN_SHADOW,
                           MPEN_HALFSHADOW, MPEN_BACKGROUND, MPEN_HALFSHINE,
                           MPEN_SHINE, round_thin_down_diag);
}

/**************************************************************************
 A : FST_SEMIROUND_BEVEL
**************************************************************************/
static void semiround_bevel_draw(struct dt_frame_image *fi,
                                 struct MUI_RenderInfo *mri, int left, int top,
                                 int width, int height, MPen pen1, MPen pen2,
                                 MPen pen3, MPen pen4, MPen pen5)
{
    struct RastPort *rp = mri->mri_RastPort;

    button_draw(fi, mri, left, top, width, height, pen1, pen5);
    button_draw(fi, mri, left + 1, top + 1, width - 2, height - 2, pen2, pen4);

    SetAPen(rp, mri->mri_Pens[pen2]);
    WritePixel(rp, left, top);

    SetAPen(rp, mri->mri_Pens[pen1]);
    WritePixel(rp, left + 1, top + 1);

    SetAPen(mri->mri_RastPort, mri->mri_Pens[pen5]);
    WritePixel(rp, left + width - 2, top + height - 2);

    SetAPen(mri->mri_RastPort, mri->mri_Pens[pen4]);
    WritePixel(rp, left + width - 1, top + height - 1);

    SetAPen(mri->mri_RastPort, mri->mri_Pens[pen3]);
    WritePixel(rp, left, top + height - 2);
    WritePixel(rp, left + 1, top + height - 1);
    WritePixel(rp, left + width - 2, top);
    WritePixel(rp, left + width - 1, top + 1);
}

/**************************************************************************

**************************************************************************/
static void frame_semiround_bevel_up_draw(struct dt_frame_image *fi,
        struct MUI_RenderInfo *mri, int gl,
        int gt, int gw, int gh, int left,
        int top, int width, int height)
{
    semiround_bevel_draw(fi, mri, left, top, width, height, MPEN_SHINE,
                         MPEN_HALFSHINE, MPEN_BACKGROUND, MPEN_HALFSHADOW,
                         MPEN_SHADOW);
}

/**************************************************************************

**************************************************************************/
static void frame_semiround_bevel_down_draw(struct dt_frame_image *fi,
        struct MUI_RenderInfo *mri, int gl,
        int gt, int gw, int gh, int left,
        int top, int width, int height)
{
    semiround_bevel_draw(fi, mri, left, top, width, height, MPEN_SHADOW,
                         MPEN_HALFSHADOW, MPEN_BACKGROUND, MPEN_HALFSHINE,
                         MPEN_SHINE);
}

/**************************************************************************
 Modern frame styles - contemporary flat and minimal designs
**************************************************************************/

/**
 * Draw a smooth circle corner using Bresenham's circle algorithm
 * @param rp RastPort to draw on
 * @param cx Center X coordinate
 * @param cy Center Y coordinate
 * @param radius Circle radius
 * @param pen Pen color to use
 * @param quadrant Which quadrant to draw:
 *                 1 = Northeast (top-right corner)
 *                 2 = Northwest (top-left corner)
 *                 4 = Southwest (bottom-left corner)
 *                 8 = Southeast (bottom-right corner)
 */
static void draw_smooth_corner(struct RastPort *rp, int cx, int cy, int radius,
                               ULONG pen, int quadrant)
{
    int x = 0, y = radius;
    int d = 3 - 2 * radius;

    SetAPen(rp, pen);

    while (y >= x) {
        /* Draw pixels in selected quadrant only */
        if (quadrant & 1) { /* Northeast (top-right corner) */
            WritePixel(rp, cx + x, cy - y);
            WritePixel(rp, cx + y, cy - x);
        }
        if (quadrant & 2) { /* Northwest (top-left corner) */
            WritePixel(rp, cx - x, cy - y);
            WritePixel(rp, cx - y, cy - x);
        }
        if (quadrant & 4) { /* Southwest (bottom-left corner) */
            WritePixel(rp, cx - x, cy + y);
            WritePixel(rp, cx - y, cy + x);
        }
        if (quadrant & 8) { /* Southeast (bottom-right corner) */
            WritePixel(rp, cx + x, cy + y);
            WritePixel(rp, cx + y, cy + x);
        }

        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
        x++;
    }
}

/**
 * Helper function to fill the connection pixels between circle and straight
 * edges This ensures no gaps appear between the curved corners and straight
 * lines
 */
static void fill_corner_connections(struct RastPort *rp, int left, int top,
                                    int width, int height, int radius,
                                    ULONG light_pen, ULONG dark_pen)
{
    /* Fill the exact pixels where curves should connect to straight lines */

    /* Top-left corner connections */
    SetAPen(rp, light_pen);
    WritePixel(rp, left + radius, top); /* Top edge connection */
    WritePixel(rp, left, top + radius); /* Left edge connection */

    /* Top-right corner connections */
    WritePixel(rp, left + width - radius - 1, top); /* Top edge connection */
    SetAPen(rp, dark_pen);
    WritePixel(rp, left + width - 1, top + radius); /* Right edge connection */

    /* Bottom-left corner connections */
    SetAPen(rp, light_pen);
    WritePixel(rp, left, top + height - radius - 1); /* Left edge connection */
    SetAPen(rp, dark_pen);
    WritePixel(rp, left + radius, top + height - 1); /* Bottom edge connection */

    /* Bottom-right corner connections */
    WritePixel(rp, left + width - radius - 1, top + height - 1); /* Bottom edge */
    WritePixel(rp, left + width - 1, top + height - radius - 1); /* Right edge */
}

/**
 * D: Rounded frame
 * Parameters moved from globals to struct dt_frame_image members
 */
/* Frame parameters are now part of the dt_frame_image struct */

static void frame_rounded_draw(struct dt_frame_image *fi,
                               struct MUI_RenderInfo *mri, int gl, int gt,
                               int gw, int gh, int left, int top, int width,
                               int height, MPen light_pen, MPen dark_pen)
{
    struct RastPort *rp = mri->mri_RastPort;

    /* Use the radius from the frame image struct */
    int radius = fi ? fi->border_radius : 8;

    if (width <= radius * 2 + 4 || height <= radius * 2 + 4)
        return; /* Need space for radius plus some margin */

    /* Use the frame width from the frame image struct */
    int frame_width = fi ? fi->frame_width : 1;

    SetAPen(rp, mri->mri_Pens[light_pen]);
    /* Top edge: from end of left curve to start of right curve */
    RectFill(rp, left + radius, top, left + width - radius - 1,
             top + frame_width - 1);
    /* Left edge: from end of top curve to start of bottom curve */
    RectFill(rp, left, top + radius, left + frame_width - 1,
             top + height - radius - 1);

    SetAPen(rp, mri->mri_Pens[dark_pen]);
    /* Bottom edge: from end of left curve to start of right curve */
    RectFill(rp, left + radius, top + height - frame_width,
             left + width - radius - 1, top + height - 1);
    /* Right edge: from end of top curve to start of bottom curve */
    RectFill(rp, left + width - frame_width, top + radius, left + width - 1,
             top + height - radius - 1);

    if (frame_width < 2) {
        /* Top-left corner: center at (left+radius, top+radius) */
        draw_smooth_corner(rp, left + radius, top + radius, radius,
                           mri->mri_Pens[light_pen], 2);

        /* Top-right corner: center at (left+width-radius-1, top+radius) */
        draw_smooth_corner(rp, left + width - radius - 1, top + radius, radius,
                           mri->mri_Pens[light_pen], 1);

        /* Bottom-left corner: center at (left+radius, top+height-radius-1) */
        draw_smooth_corner(rp, left + radius, top + height - radius - 1, radius,
                           mri->mri_Pens[dark_pen], 4);

        /* Bottom-right corner: center at (left+width-radius-1, top+height-radius-1)
         */
        draw_smooth_corner(rp, left + width - radius - 1, top + height - radius - 1,
                           radius, mri->mri_Pens[dark_pen], 8);
    } else {
        /* Draw solid rounded corners by filling the corner areas pixel by pixel */
        int x, y;

        for (y = 0; y < radius; y++) {
            for (x = 0; x < radius; x++) {
                int dx = radius - x - 1;
                int dy = radius - y - 1;
                int distance_sq = dx * dx + dy * dy;
                int outer_radius_sq = radius * radius;
                int inner_radius = radius - frame_width;
                int inner_radius_sq = inner_radius * inner_radius;

                /* Only draw if pixel is in the border area (between inner and outer
                 * radius) */
                if (distance_sq < outer_radius_sq &&
                        (inner_radius <= 0 || distance_sq >= inner_radius_sq)) {
                    /* Top-left corner */
                    SetAPen(rp, mri->mri_Pens[light_pen]);
                    WritePixel(rp, left + x, top + y);

                    /* Top-right corner - mirror x coordinate */
                    WritePixel(rp, left + width - x - 1, top + y);

                    /* Bottom-left corner - mirror y coordinate */
                    SetAPen(rp, mri->mri_Pens[dark_pen]);
                    WritePixel(rp, left + x, top + height - y - 1);

                    /* Bottom-right corner - mirror both x and y coordinates */
                    WritePixel(rp, left + width - x - 1, top + height - y - 1);
                }
            }
        }
    }
    // 1,
    //                    radius - 1, mri->mri_Pens[MPEN_HALFSHADOW], 8);

    /* Ensure perfect connections between curves and straight lines */
    fill_corner_connections(rp, left, top, width, height, radius,
                            mri->mri_Pens[light_pen], mri->mri_Pens[dark_pen]);
}

/**
 * Rounded frame (raised state)
 */
static void frame_rounded_up_draw(struct dt_frame_image *fi,
                                  struct MUI_RenderInfo *mri, int gl, int gt,
                                  int gw, int gh, int left, int top, int width,
                                  int height)
{
    frame_rounded_draw(fi, mri, gl, gt, gw, gh, left, top, width, height,
                       MPEN_HALFSHADOW, MPEN_HALFSHADOW);
}

/**
 * Rounded frame (pressed state)
 */
static void frame_rounded_down_draw(struct dt_frame_image *fi,
                                    struct MUI_RenderInfo *mri, int gl, int gt,
                                    int gw, int gh, int left, int top,
                                    int width, int height)
{
    frame_rounded_draw(fi, mri, gl, gt, gw, gh, left, top, width, height,
                       MPEN_HALFSHADOW, MPEN_HALFSHADOW);
}

#ifdef ZUNE_FRAMES_MODULE
/* The entries of these types, as frame.c's table has them: up, down for
 * 5, 7, 8, A, B */
const struct ZuneFrameGfx zune_round_frames[10] = {
    {frame_round_bevel_up_draw, 0, 4, 4, 1, 1, NULL, FALSE, 2, 2},
    {frame_round_bevel_down_draw, 0, 4, 4, 1, 1, NULL, FALSE, 2, 2},
    {frame_round_thick_border_up_draw, 0, 4, 4, 4, 4, NULL, FALSE, 4, 3},
    {frame_round_thick_border_down_draw, 0, 4, 4, 4, 4, NULL, FALSE, 4, 3},
    {frame_round_thin_border_up_draw, 0, 3, 3, 3, 3, NULL, FALSE, 2, 2},
    {frame_round_thin_border_down_draw, 0, 3, 3, 3, 3, NULL, FALSE, 2, 2},
    {frame_semiround_bevel_up_draw, 0, 2, 2, 2, 2, NULL, FALSE, 2, 1},
    {frame_semiround_bevel_down_draw, 0, 2, 2, 2, 2, NULL, FALSE, 2, 1},
    {frame_rounded_up_draw, 0, 2, 2, 2, 2, NULL, FALSE, 1, 8},
    {frame_rounded_down_draw, 0, 2, 2, 2, 2, NULL, FALSE, 1, 8},
};
#endif
