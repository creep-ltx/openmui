/* MUI 5's RGB and alpha text pens (\33P[rrggbb], \33P[aarrggbb]) drawn on a
 * true-colour screen: in the module Zmui45.mui (classes/zmui45.c,
 * modules.h). MUI 3.8 knows no such escape, so a MUI 3 program never loads
 * this; the library draws the text plainly when the module is missing. */
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>
#include "muimaster_intern.h"
#include "support.h"
#include "modules.h"

/* Render only explicit RGB/alpha text through a one-bit glyph mask. Plain
 * text keeps graphics.library's direct path and needs no scratch bitmap. */
BOOL ZuneTextARGB(struct RastPort *rp, CONST_STRPTR text, ULONG length,
    ULONG foreground, ULONG background)
{
    struct TextExtent extent;
    struct RastPort mask;
    struct BitMap *bitmap;
    ULONG *pixels;
    LONG x = rp->cp_x, y = rp->cp_y, width, height, advance;
    ZUNE_MODULE_REFRESH(); /* CyberGfxBase, opened after the load */
    if (!CyberGfxBase || !rp->BitMap ||
        !GetCyberMapAttr(rp->BitMap, CYBRMATTR_ISCYBERGFX)) return FALSE;
    TextExtent(rp, text, length, &extent);
    width = extent.te_Extent.MaxX - extent.te_Extent.MinX + 1;
    height = extent.te_Extent.MaxY - extent.te_Extent.MinY + 1;
    advance = extent.te_Width;
    if (width <= 0 || height <= 0) return TRUE;
    if (width > 16383 || height > 32767 ||
        (ULONG)width * height > 0x1fffffffUL) return FALSE;
    bitmap = AllocBitMap(width, height, 1, BMF_CLEAR, NULL);
    if (!bitmap) return FALSE;
    pixels = AllocVec(width * height * 4, MEMF_ANY);
    if (!pixels) { FreeBitMap(bitmap); return FALSE; }
    InitRastPort(&mask);
    mask.BitMap = bitmap;
    SetFont(&mask, rp->Font);
    SetSoftStyle(&mask, rp->AlgoStyle, 0xff);
    SetAPen(&mask, 1); SetBPen(&mask, 0);
    SetDrMd(&mask, JAM2 | (rp->DrawMode & INVERSVID));
    Move(&mask, -extent.te_Extent.MinX, -extent.te_Extent.MinY);
    Text(&mask, text, length);
    WaitBlit();
    for (LONG iy = 0; iy < height; iy++)
        for (LONG ix = 0; ix < width; ix++)
        {
            UBYTE bits = bitmap->Planes[0][iy * bitmap->BytesPerRow + ix / 8];
            pixels[iy * width + ix] = (bits & (0x80 >> (ix & 7))) ?
                foreground : (rp->DrawMode & JAM2) ? background : 0;
        }
    ZuneWritePixelArrayAlpha(pixels, 0, 0, width * 4, rp,
        x + extent.te_Extent.MinX, y + extent.te_Extent.MinY,
        width, height, 0xff000000);
    Move(rp, x + advance, y);
    FreeVec(pixels); FreeBitMap(bitmap);
    return TRUE;
}
