/* Optional CyberGraphX V41 rendering, also provided by Picasso96 on OS3.
 * Use the original pixel-array API: the newer alpha/processing vectors
 * belong to other platforms and cannot be called on classic systems. */
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>
#include "muimaster_intern.h"
#include "support.h"

BOOL ZuneOpenCyberGfx(void)
{
    ObtainSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    if (!CyberGfxBase) CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
    ReleaseSemaphore(&MUIMB(MUIMasterBase)->ZuneSemaphore);
    return CyberGfxBase != NULL;
}
/* This primitive is also used by the linked 68000 arithmetic tests. */
ULONG ZuneBlendARGB(ULONG source, ULONG destination, ULONG opacity)
{
    ULONG alpha = ((source >> 24) * opacity + 127) / 255;
    if (!alpha) return destination;
    if (alpha == 255) return source;
    ULONG result = 0xff000000;
    for (ULONG shift = 0; shift <= 16; shift += 8)
    {
        ULONG a = (source >> shift) & 255, b = (destination >> shift) & 255;
        result |= ((a * alpha + b * (255 - alpha) + 127) / 255) << shift;
    }
    return result;
}
void ZuneWritePixelArrayAlpha(APTR source, LONG sx, LONG sy, ULONG stride,
    struct RastPort *rp, LONG x, LONG y, LONG width, LONG height, ULONG opacity)
{
    if (!CyberGfxBase || !source || !rp || !rp->BitMap || width <= 0 || height <= 0 ||
        !GetCyberMapAttr(rp->BitMap, CYBRMATTR_ISCYBERGFX)) return;
    if (x < 0) { sx -= x; width += x; x = 0; }
    if (y < 0) { sy -= y; height += y; y = 0; }
    if (width <= 0 || height <= 0 || width > 16383 || sx < 0 || sy < 0) return;
    ULONG *row = AllocVec(width * 4, MEMF_ANY);
    if (!row) return;
    ULONG alpha = opacity >> 24;
    for (LONG iy = 0; iy < height; iy++)
    {
        const ULONG *src = (APTR)((UBYTE *)source + (sy + iy) * stride + sx * 4);
        if (!ReadPixelArray(row, 0, 0, width * 4, rp, x, y + iy, width, 1, RECTFMT_ARGB)) continue;
        for (LONG ix = 0; ix < width; ix++) row[ix] = ZuneBlendARGB(src[ix], row[ix], alpha);
        WritePixelArray(row, 0, 0, width * 4, rp, x, y + iy, width, 1, RECTFMT_ARGB);
    }
    FreeVec(row);
}
