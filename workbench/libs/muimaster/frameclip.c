/*
    Copyright  2002-2026, The AROS Development Team.
    All rights reserved.
*/

/* The clip region of a rounded frame's inside: only MUI 4/5's
 * MUIM_CreateFrameClippingRegion (mui45.c) and the background under a
 * rounded frame (arearound.c) ask for it, so each of their modules
 * includes its own copy and the library has none. */
#include <proto/graphics.h>

/**
 * Create a clipping region for rounded frames
 * @param left Left coordinate of frame
 * @param top Top coordinate of frame
 * @param width Width of frame
 * @param height Height of frame
 * @param clipinfo Output Frame clipping information
 * @return Clipping region or NULL on error
 */
static __attribute__((noinline)) struct Region *
zune_frame_create_clip_region(int left, int top, int width,
        int height, UWORD border_radius)
{
    struct Region *region;
    struct Rectangle rect;

    if (width <= 0 || height <= 0)
        return NULL;

    /* For non-rounded frames, return full rectangular region */
    if (border_radius == 0) {
        region = NewRegion();
        if (region) {
            rect.MinX = left;
            rect.MinY = top;
            rect.MaxX = left + width - 1;
            rect.MaxY = top + height - 1;
            OrRectRegion(region, &rect);
        }
        return region;
    }

    /* Create clipping region for rounded corners */
    region = NewRegion();
    if (!region)
        return NULL;

    int radius = border_radius;

    /* Ensure radius doesn't exceed frame dimensions */
    if (radius * 2 > width)
        radius = width / 2;
    if (radius * 2 > height)
        radius = height / 2;

    /* Add main rectangular area (excluding corner areas) */

    /* Top rectangle (above corners) */
    if (radius < height / 2) {
        rect.MinX = left + radius;
        rect.MinY = top;
        rect.MaxX = left + width - radius - 1;
        rect.MaxY = top + radius - 1;
        if (rect.MinX <= rect.MaxX && rect.MinY <= rect.MaxY)
            OrRectRegion(region, &rect);
    }

    /* Middle rectangle (full width between corners) */
    rect.MinX = left;
    rect.MinY = top + radius;
    rect.MaxX = left + width - 1;
    rect.MaxY = top + height - radius - 1;
    if (rect.MinX <= rect.MaxX && rect.MinY <= rect.MaxY)
        OrRectRegion(region, &rect);

    /* Bottom rectangle (below corners) */
    if (radius < height / 2) {
        rect.MinX = left + radius;
        rect.MinY = top + height - radius;
        rect.MaxX = left + width - radius - 1;
        rect.MaxY = top + height - 1;
        if (rect.MinX <= rect.MaxX && rect.MinY <= rect.MaxY)
            OrRectRegion(region, &rect);
    }

    /* Add rounded corner pixels using circle algorithm */
    int x, y;

    /* For each corner, calculate which pixels are inside the rounded area */
    for (y = 0; y < radius; y++) {
        for (x = 0; x < radius; x++) {
            /* Check if pixel is inside circle using distance formula */
            int dx = radius - x - 1;
            int dy = radius - y - 1;
            /* Use a more conservative threshold to prevent background leaking */
            int threshold = (radius > 2) ? (radius - 2) * (radius - 2) : 1;
            if (dx * dx + dy * dy < threshold) {
                /* Top-left corner */
                rect.MinX = left + x;
                rect.MinY = top + y;
                rect.MaxX = left + x;
                rect.MaxY = top + y;
                OrRectRegion(region, &rect);

                /* Top-right corner */
                rect.MinX = left + width - x - 1;
                rect.MinY = top + y;
                rect.MaxX = left + width - x - 1;
                rect.MaxY = top + y;
                OrRectRegion(region, &rect);

                /* Bottom-left corner */
                rect.MinX = left + x;
                rect.MinY = top + height - y - 1;
                rect.MaxX = left + x;
                rect.MaxY = top + height - y - 1;
                OrRectRegion(region, &rect);

                /* Bottom-right corner */
                rect.MinX = left + width - x - 1;
                rect.MinY = top + height - y - 1;
                rect.MaxX = left + width - x - 1;
                rect.MaxY = top + height - y - 1;
                OrRectRegion(region, &rect);
            }
        }
    }

    return region;
}
