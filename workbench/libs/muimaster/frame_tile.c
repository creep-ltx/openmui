/*
    Copyright  2002-2026, The AROS Development Team.
    Copyright  1999, David Le Corfec.
    All rights reserved.

*/

/* Drawing a custom frame (frame.config's tiled pictures): split out of
 * frame.c. The native build has it in the picture module (modules.h,
 * classes/zpicture.c), which made the frame's pictures in the first place,
 * so a custom frame never loads anything new; other builds include it in
 * frame.c. */

#include <cybergraphx/cybergraphics.h>
#include <exec/types.h>

#include <proto/cybergraphics.h>
#include <proto/graphics.h>
#include <proto/layers.h>

#include "datatypescache.h"
#include "frame.h"
#include "mui.h"
#include "muimaster_intern.h"

/**************************************************************************
 custom frames
**************************************************************************/

void DrawPartToImage(struct NewImage *src, struct NewImage *dest, UWORD sx,
                     UWORD sy, UWORD sw, UWORD sh, UWORD dx, UWORD dy)
{
    UWORD x, y;

    ULONG *s, *d;

    for (y = 0; y < sh; y++) {
        s = &src->data[sx + ((sy + y) * src->w)];
        d = &dest->data[dx + ((dy + y) * dest->w)];

        for (x = 0; x < sw; x++) {
            *d++ = *s++;
        }
    }
}

static void DrawTileToImage(BOOL alpha, BOOL tc, struct RastPort *rp,
                            struct NewImage *ni, struct NewImage *dest,
                            UWORD _sx, UWORD _sy, UWORD _sw, UWORD _sh,
                            UWORD _dx, UWORD _dy, UWORD _dw, UWORD _dh,
                            UWORD posx, UWORD posy)
{

    ULONG dy, dx;
    LONG dh, height, dw, width;

    if (ni == NULL)
        return;

    if ((_sh == 0) || (_dh == 0) || (_sw == 0) || (_dw == 0))
        return;

    dh = _sh;
    dy = _dy;
    height = _dh;
    while (height > 0) {
        if ((height - dh) < 0)
            dh = height;
        height -= dh;

        dw = _sw;
        width = _dw;
        dx = _dx;
        while (width > 0) {
            if ((width - dw) < 0)
                dw = width;
            width -= dw;

            if (tc) {
                if (dest != NULL) {
                    DrawPartToImage(ni, dest, _sx, _sy, dw, dh, dx, dy);
                } else {
                    if (alpha)
                        WritePixelArrayAlpha(ni->data, _sx, _sy, ni->w * 4, rp, dx + posx,
                                             dy + posy, dw, dh, 0xffffffff);
                    else
                        WritePixelArray(ni->data, _sx, _sy, ni->w * 4, rp, dx + posx,
                                        dy + posy, dw, dh, RECTFMT_ARGB);
                }
            } else {
                if (ni->bitmap != NULL) {
                    if (alpha) {
                        if (ni->mask)
                            BltMaskBitMapRastPort(ni->bitmap, _sx, _sy, rp, dx + posx,
                                                  dy + posy, dw, dh, 0xe0,
                                                  (PLANEPTR)ni->mask);
                        else
                            BltBitMapRastPort(ni->bitmap, _sx, _sy, rp, dx + posx, dy + posy,
                                              dw, dh, 0xc0);
                    } else {
                        BltBitMapRastPort(ni->bitmap, _sx, _sy, rp, dx + posx, dy + posy,
                                          dw, dh, 0xc0);
                    }
                }
            }
            dx += dw;
        }
        dy += dh;
    }
}

static void draw_tile_frame(struct RastPort *rport, BOOL tc, BOOL direct,
                            BOOL alpha, struct dt_frame_image *fi,
                            struct NewImage *src, struct NewImage *dest, int gl,
                            int gt, int gw, int gh, int left, int top,
                            int width, int height)
{
    int lw, rw, th, bh, right, bottom, gr, gb, lp, tp, rp, bp, mw, mh;

    right = left + width;
    bottom = top + height;

    gr = gl + gw;
    gb = gt + gh;

    /* calculate the left width of the frame */

    lw = left - gl;
    if (lw > fi->tile_left)
        lw = fi->tile_left;
    lw = fi->tile_left - lw;

    /* calculate the top height of the frame */

    th = top - gt;
    if (th > fi->tile_top)
        th = fi->tile_top;
    th = fi->tile_top - th;

    lp = fi->tile_left - lw; // left position
    tp = fi->tile_top - th;

    if (right < (fi->tile_left + gl))
        lw -= ((fi->tile_left + gl) - right);
    if (bottom < (fi->tile_top + gt))
        th -= ((fi->tile_top + gt) - bottom);

    bp = src->h - fi->tile_bottom;
    bh = fi->tile_bottom;

    if (top > (gb - fi->tile_bottom)) {
        bp += (top - (gb - fi->tile_bottom));
        bh -= (top - (gb - fi->tile_bottom));

        if (bottom < gb)
            bh -= (gb - bottom);
    } else {
        if (bottom < (gb - fi->tile_bottom))
            bh = 0;
        else
            bh -= (gb - bottom);
    }

    rp = src->w - fi->tile_right;
    rw = fi->tile_right;

    if (left > (gr - fi->tile_right)) {
        rp += (left - (gr - fi->tile_right));
        rw -= (left - (gr - fi->tile_right));

        if (right < gr)
            rw -= (gr - right);
    } else {
        if (right < (gr - fi->tile_right))
            rw = 0;
        else
            rw -= (gr - right);
    }

    mw = width - lw - rw;
    mh = height - th - bh;

    struct NewImage *d;

    if (direct)
        d = NULL;
    else
        d = dest;

    DrawTileToImage(alpha, tc, rport, src, d, lp, tp, lw, th, 0, 0, lw, th, left,
                    top);
    DrawTileToImage(alpha, tc, rport, src, d, lp, bp, lw, bh, 0, height - bh, lw,
                    bh, left, top);
    DrawTileToImage(alpha, tc, rport, src, d, rp, tp, rw, th, width - rw, 0, rw,
                    th, left, top);
    DrawTileToImage(alpha, tc, rport, src, d, rp, bp, rw, bh, width - rw,
                    height - bh, rw, bh, left, top);

    DrawTileToImage(alpha, tc, rport, src, d, fi->tile_left, tp,
                    src->w - fi->tile_left - fi->tile_right, th, lw, 0, mw, th,
                    left, top);
    DrawTileToImage(alpha, tc, rport, src, d, fi->tile_left, bp,
                    src->w - fi->tile_left - fi->tile_right, bh, lw, height - bh,
                    mw, bh, left, top);

    DrawTileToImage(alpha, tc, rport, src, d, lp, fi->tile_top, lw,
                    src->h - fi->tile_bottom - fi->tile_top, 0, th, lw, mh, left,
                    top);

    DrawTileToImage(alpha, tc, rport, src, d, rp, fi->tile_top, rw,
                    src->h - fi->tile_bottom - fi->tile_top, width - rw, th, rw,
                    mh, left, top);
    DrawTileToImage(alpha, tc, rport, src, d, fi->tile_left, fi->tile_top,
                    src->w - fi->tile_left - fi->tile_right,
                    src->h - fi->tile_bottom - fi->tile_top, lw, th, mw, mh, left,
                    top);
}

struct FrameFillInfo {
    struct Hook Hook;
    struct dt_frame_image *fi;
    struct NewImage *ni;
    WORD gl, gt, gw, gh, left, top, width, height;
};

struct FrameBackFillMsg {
    STACKED struct Layer *Layer;
    STACKED struct Rectangle Bounds;
    STACKED LONG OffsetX;
    STACKED LONG OffsetY;
};

AROS_UFH3S(void, FrameTileBackFillFunc, AROS_UFHA(struct Hook *, Hook, A0),
           AROS_UFHA(struct RastPort *, RP, A2),
           AROS_UFHA(struct FrameBackFillMsg *, BFM, A1))
{
    AROS_USERFUNC_INIT

    // get the data for our backfillhook
    struct FrameFillInfo *FFI = (struct FrameFillInfo *)Hook;

    ULONG depth = (ULONG)GetBitMapAttr(RP->BitMap, BMA_DEPTH);

    BOOL truecolor = TRUE;

    if (depth < 15)
        truecolor = FALSE;

    /* The area to paint in RP->BitMap ... */
    int bx = BFM->Bounds.MinX;
    int by = BFM->Bounds.MinY;
    int width = BFM->Bounds.MaxX - bx + 1;
    int height = BFM->Bounds.MaxY - by + 1;
    /* ... and where it lies in layer coordinates. Only the offsets tell
       that: for a backed up (hidden) ClipRect the bounds are relative to
       the ClipRect's own bitmap. */
    int left = BFM->OffsetX;
    int top = BFM->OffsetY;
    /* The hook has to render into RP->BitMap: use a layerless copy */
    struct RastPort temprp = *RP;

    temprp.Layer = NULL;

    BOOL alpha = !FFI->fi->noalpha;
    BOOL direct = FALSE;

    if (!truecolor)
        direct = TRUE;

    if (!direct) {
        struct NewImage *dest = NewImageContainer(width, height);
        if (dest != NULL) {
            draw_tile_frame(NULL, truecolor, FALSE, alpha, FFI->fi, FFI->ni, dest,
                            FFI->gl, FFI->gt, FFI->gw, FFI->gh, left, top, width,
                            height);
            if (FFI->fi->noalpha)
                WritePixelArray(dest->data, 0, 0, dest->w * 4, &temprp, bx, by, width,
                                height, RECTFMT_ARGB);
            else
                WritePixelArrayAlpha(dest->data, 0, 0, dest->w * 4, &temprp, bx, by,
                                     width, height, 0xffffffff);

            DisposeImageContainer(dest);
        } else
            direct = TRUE;
    }

    if (direct) {
        /* Everything shifted from layer coordinates into bitmap coordinates */
        draw_tile_frame(&temprp, truecolor, FALSE, alpha, FFI->fi, FFI->ni, NULL,
                        FFI->gl + bx - left, FFI->gt + by - top, FFI->gw, FFI->gh,
                        bx, by, width, height);
    }

    AROS_USERFUNC_EXIT
}

void dt_do_frame_rects(struct RastPort *rp, struct dt_frame_image *fi,
                       struct NewImage *ni, int gl, int gt, int gw, int gh,
                       int left, int top, int width, int height)
{
    struct Rectangle rect;
    struct FrameFillInfo ffi;

    rect.MinX = left;
    rect.MinY = top;
    rect.MaxX = left + width - 1;
    rect.MaxY = top + height - 1;

    ffi.left = left;
    ffi.top = top;
    ffi.width = width;
    ffi.height = height;

    ffi.gl = gl;
    ffi.gt = gt;
    ffi.gw = gw;
    ffi.gh = gh;

    ffi.ni = ni;

    ffi.fi = fi;

    ffi.Hook.h_Entry = (HOOKFUNC)FrameTileBackFillFunc;

    DoHookClipRects((struct Hook *)&ffi, rp, &rect);
}
