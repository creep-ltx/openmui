#ifndef _PIXMAP_PRIVATE_H_
#define _PIXMAP_PRIVATE_H_

/*
    Copyright © 2011, Thore Böckelmann. All rights reserved.
    Copyright © 2012, The AROS Development Team. All rights reserved.
    $Id$
*/

struct Pixmap_DATA
{
    LONG leftOffset;
    LONG topOffset;
    APTR data;
    LONG width;
    LONG height;
    ULONG format;
    APTR clut;
    ULONG alpha;
    ULONG compression;
    ULONG compressedSize;
    ULONG screenDepth;
    APTR uncompressedData;
    BOOL ownsData;
    BOOL setup;
    struct ColorMap *colorMap;
    APTR ditheredMask;
    LONG ditheredPenMap[256];
    struct BitMap *ditheredBitmap;
    /* the dithered image of the last setup, its pens given back: the
     * next setup uses it again if the same pens come back (KEEP_PIXELS) */
    BOOL kept;
};

#endif /* _PIXMAP_PRIVATE_H_ */
