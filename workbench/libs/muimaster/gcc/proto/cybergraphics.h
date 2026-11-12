#ifndef ZUNE68_GCC_CYBERGRAPHICS_H
#define ZUNE68_GCC_CYBERGRAPHICS_H
#include_next <proto/cybergraphics.h>

/* Classic CyberGraphX has no MorphOS alpha or pixel-processing vectors. */
BOOL ZuneOpenCyberGfx(void);
BOOL ZuneTextARGB(struct RastPort *, CONST_STRPTR, ULONG, ULONG, ULONG);
void ZuneWritePixelArrayAlpha(APTR, LONG, LONG, ULONG, struct RastPort *,
    LONG, LONG, LONG, LONG, ULONG);
/* ZuneProcessPixelArray: classes/dtpic.c's own (its only user) */
#define WritePixelArrayAlpha ZuneWritePixelArrayAlpha
#define ProcessPixelArray ZuneProcessPixelArray
#define POP_DARKEN 1
#define POP_BRIGHTEN 2
#endif
