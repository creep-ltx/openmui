/*
    Copyright  2002-2026, The AROS Development Team.
    All rights reserved.

*/

/* The rarely used vector images, split out of imspec_vector.c: MUII_Drawer,
 * HardDisk, Disk, Chip, Volume, Network, Assign and the tape buttons
 * (vector images 10-23, "1:10" to "1:23"). The native build has them in
 * the image module (modules.h, classes/zimages.c), loaded when an image of
 * these is first set up; other builds include this file in
 * imspec_vector.c. */

#ifdef ZUNE_IMAGES_MODULE
#include <intuition/imageclass.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include "debug.h"
#include "mui.h"
#include "imspec_intern.h"
#include "support.h"
#include "modules.h"
#endif

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


void drawer_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
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

void harddisk_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
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

void disk_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
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

void ram_draw(struct MUI_RenderInfo *mri, LONG left, LONG top, LONG width,
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

void volume_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
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

void network_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
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

void assign_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
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

void tape_play_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int x, half = height / 2;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);

    for (x = 0; x < width; x++)
    {
        int y = (width > 1) ? half * x / (width - 1) : 1;

        RectFill(rport, left + width - 1 - x, top + half - y,
            left + width - 1 - x, top + half + y);
    }
}

void tape_playback_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int x, half = height / 2;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);

    for (x = 0; x < width; x++)
    {
        int y = (width > 1) ? half * x / (width - 1) : 1;

        RectFill(rport, left + x, top + half - y, left + x, top + half + y);
    }
}

void tape_pause_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);
    RectFill(rport, left, top, left + width / 4, top + height - 1);
    RectFill(rport, left + width - 1 - width / 4, top, left + width - 1,
        top + height - 1);
}

void tape_stop_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;

    SetAPen(rport, mri->mri_Pens[MPEN_TEXT]);
    RectFill(rport, left, top, left + width - 1, top + height - 1);
}

void tape_record_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int rx = (width - 1) / 2;
    int ry = (height - 1) / 2;
    int x = rx, y = 0;          /* ellipse points */

    /* intermediate terms to speed up loop */
    int t1 = rx * rx, t2 = t1 << 1, t3 = t2 << 1;
    int t4 = ry * ry, t5 = t4 << 1, t6 = t5 << 1;
    int t7 = rx * t5, t8 = t7 << 1, t9 = 0L;
    int d1 = t2 - t7 + (t4 >> 1);       /* error terms */
    int d2 = (t1 >> 1) - t8 + t5;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);

    while (d2 < 0)              /* til slope = -1 */
    {

        /* draw 4 points using symmetry */
        RectFill(rport, left + rx - x, top + ry + y, left + rx + x,
            top + ry + y);
        RectFill(rport, left + rx - x, top + ry - y, left + rx + x,
            top + ry - y);

        y++;                    /* always move up here */
        t9 = t9 + t3;
        if (d1 < 0)             /* move straight up */
        {
            d1 = d1 + t9 + t2;
            d2 = d2 + t9;
        }
        else                    /* move up and left */
        {
            x--;
            t8 = t8 - t6;
            d1 = d1 + t9 + t2 - t8;
            d2 = d2 + t9 + t5 - t8;
        }
    }

    do                          /* rest of top right quadrant */
    {
        /* draw 4 points using symmetry */
        RectFill(rport, left + rx - x, top + ry + y, left + rx + x,
            top + ry + y);
        RectFill(rport, left + rx - x, top + ry - y, left + rx + x,
            top + ry - y);

        x--;                    /* always move left here */
        t8 = t8 - t6;
        if (d2 < 0)             /* move up and left */
        {
            y++;
            t9 = t9 + t3;
            d2 = d2 + t9 + t5 - t8;
        }
        else                    /* move straight left */
        {
            d2 = d2 + t5 - t8;
        }

    }
    while (x >= 0);

}

void tape_up_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int y, half = width / 2;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);

    for (y = 0; y < height; y++)
    {
        int x = (height > 1) ? half * y / (height - 1) : 1;

        RectFill(rport, left + half - x, top + y, left + half + x, top + y);
    }
}

void tape_down_draw(struct MUI_RenderInfo *mri, LONG left, LONG top,
    LONG width, LONG height, LONG state)
{
    struct RastPort *rport = mri->mri_RastPort;
    int y, half = width / 2;

    SetAPen(rport, mri->mri_Pens[MPEN_SHADOW]);

    for (y = 0; y < height; y++)
    {
        int x = (height > 1) ? half * y / (height - 1) : 1;

        RectFill(rport, left + half - x, top + height - 1 - y,
            left + half + x, top + height - 1 - y);
    }
}


#ifdef ZUNE_IMAGES_MODULE
/* Vector images 10-23, in imspec_vector.c's order */
const VECTOR_DRAW_FUNC zune_rare_vectors[14] = {
    drawer_draw, harddisk_draw, disk_draw, ram_draw, volume_draw,
    network_draw, assign_draw, tape_play_draw, tape_playback_draw,
    tape_pause_draw, tape_stop_draw, tape_record_draw, tape_up_draw,
    tape_down_draw
};
#endif
