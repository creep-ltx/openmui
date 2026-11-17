/*
    Copyright (C) 2002-2026, The AROS Development Team.
    All rights reserved.
*/

/* A text chunk with MUI 4/5's colours or effects (\33P[RRGGBB],
 * \33p[...], \33B, \33V, \33t, \33s, \33g): drawn by the module Zmui45
 * (classes/zmui45.c), which a text without them never loads. Moved out of
 * textengine.c (rw-draw, 2026-11) unchanged. */
#include <proto/exec.h>
#include <proto/graphics.h>
#include "mui.h"
#include "textengine.h"
#include "muimaster_intern.h"
#include "support.h"

void zune_text_effect45(ZText *text, ZTextChunk *chunk_node,
    Object *obj, struct RastPort *rp, LONG x, LONG right)
{
    LONG foreground = -1, background = -1;
    struct ColorMap *cm = _screen(obj)->ViewPort.ColorMap;
    if ((chunk_node->effects & ZTEXT_RGBPEN) && (muiRenderInfo(obj)->mri_Flags & MUIMRI_TRUECOLOR))
    {
        ULONG rgb = chunk_node->pen;
        foreground = ObtainBestPen(cm, ((rgb >> 16) & 255) * 0x01010101UL,
            ((rgb >> 8) & 255) * 0x01010101UL, (rgb & 255) * 0x01010101UL, TAG_DONE);
        if (foreground >= 0) SetAPen(rp, foreground);
    }
    else if (chunk_node->dripen != -1)
        SetAPen(rp, _dri(obj)->dri_Pens[chunk_node->dripen]);
    else if (chunk_node->pen != -1 && !(chunk_node->effects & ZTEXT_RGBPEN)) SetAPen(rp, chunk_node->pen);
    if ((chunk_node->effects & ZTEXT_RGBBACK) && (muiRenderInfo(obj)->mri_Flags & MUIMRI_TRUECOLOR))
    {
        ULONG rgb = chunk_node->backpen;
        background = ObtainBestPen(cm, ((rgb >> 16) & 255) * 0x01010101UL,
            ((rgb >> 8) & 255) * 0x01010101UL, (rgb & 255) * 0x01010101UL, TAG_DONE);
    }
    ULONG mode = (chunk_node->effects & ZTEXT_JAM2) ? JAM2 : JAM1;
    if (chunk_node->effects & ZTEXT_INVERSE) mode |= INVERSVID;
    if (mode & JAM2) SetBPen(rp, background >= 0 ? background :
        (chunk_node->effects & ZTEXT_RGBBACK) ? 0 : (chunk_node->backpen & 0xffffff));
    if (rp->DrawMode != mode) SetDrMd(rp, mode);
    {
        LONG len = chunk_node->clen;

        if (text->truncate && x + chunk_node->cwidth > right + 1)
        {
            struct TextExtent te;

            len = (right + 1 > x) ? TextFit(rp, chunk_node->str,
                len, &te, NULL, 1, right + 1 - x, 32767) : 0;
        }
        if (len > 0)
        {
            LONG tx = rp->cp_x, ty = rp->cp_y;
            if (chunk_node->style & ZTEXT_STYLE_OUTLINE)
            { tx++; ty++; Move(rp, tx, ty); }
            if (chunk_node->style & (ZTEXT_STYLE_OUTLINE | ZTEXT_STYLE_SHADOW))
            {
                ULONG pen = GetAPen(rp);
                SetAPen(rp, _dri(obj)->dri_Pens[SHADOWPEN]);
                SetDrMd(rp, JAM1);
                for (LONG dy = -1; dy <= 1; dy++)
                    for (LONG dx = -1; dx <= 1; dx++)
                        if ((dx || dy) && ((chunk_node->style & ZTEXT_STYLE_OUTLINE) || (dx == 1 && dy == 1)))
                        { Move(rp, tx + dx, ty + dy); Text(rp, chunk_node->str, len); }
                SetAPen(rp, pen); SetDrMd(rp, mode); Move(rp, tx, ty);
            }
            BOOL drawn = FALSE;
#ifdef ZUNE68_GCC_NATIVE
            if ((chunk_node->effects & (ZTEXT_RGBPEN | ZTEXT_ALPHA)) &&
                (muiRenderInfo(obj)->mri_Flags & MUIMRI_TRUECOLOR))
            {
                ULONG fg = chunk_node->pen, bg = chunk_node->backpen;
                ULONG rgb[3];
                if (!(chunk_node->effects & ZTEXT_RGBPEN))
                {
                    GetRGB32(cm, GetAPen(rp), 1, rgb);
                    fg = (rgb[0] & 0xff000000) >> 8 |
                        (rgb[1] & 0xff000000) >> 16 | rgb[2] >> 24;
                }
                if (!(chunk_node->effects & ZTEXT_RGBBACK))
                {
                    GetRGB32(cm, GetBPen(rp), 1, rgb);
                    bg = (rgb[0] & 0xff000000) >> 8 |
                        (rgb[1] & 0xff000000) >> 16 | rgb[2] >> 24;
                }
                drawn = ZuneTextARGB(rp, chunk_node->str, len,
                    (fg & 0xffffff) | (chunk_node->backpen & 0xff000000),
                    (bg & 0xffffff) | 0xff000000);
            }
#endif
            if (!drawn) Text(rp, chunk_node->str, len);
            if (chunk_node->style & ZTEXT_STYLE_GHOST)
            {
                UWORD pattern[] = {0x5555, 0xaaaa};
                ULONG pen = GetAPen(rp);
                SetAPen(rp, _dri(obj)->dri_Pens[BACKGROUNDPEN]);
                UWORD *oldpattern = rp->AreaPtrn; BYTE oldsize = rp->AreaPtSz;
                rp->AreaPtrn = pattern; rp->AreaPtSz = 1; SetDrMd(rp, JAM1);
                RectFill(rp, tx, ty - rp->Font->tf_Baseline,
                    rp->cp_x - 1, ty - rp->Font->tf_Baseline + rp->Font->tf_YSize - 1);
                rp->AreaPtrn = oldpattern; rp->AreaPtSz = oldsize;
                SetAPen(rp, pen); SetDrMd(rp, mode);
            }
        }
        /* MUI 3.9 underlines a shortcut one pixel short of
         * its advance, MUI 3.8 the whole advance (demopix), two rows
         * below the baseline but within the font
         * (tests/native/ulprobe.c) */
        if (len > 0 && (chunk_node->style & ZTEXT_STYLE_HICHAR))
        {
            struct TextFont *font = _font(obj);
            WORD y = rp->cp_y - font->tf_Baseline;

            y += MIN(font->tf_Baseline + 2, font->tf_YSize - 1);
            RectFill(rp, x, y, x + MAX(chunk_node->cwidth -
                (ZUNE_APP_MUI38(obj) ? 1 : 2), 0), y);
        }
    }
    if (foreground >= 0) ReleasePen(cm, foreground);
    if (background >= 0) ReleasePen(cm, background);
}

