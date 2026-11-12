#ifndef ZUNE_POINTER_H
#define ZUNE_POINTER_H

#include <exec/types.h>
#include <graphics/gfx.h>
#include <intuition/classusr.h>
struct Window;

/* A window's drawn pointer shape (pointer45.c) */
struct ZunePointer
{
    Object *image;
    struct BitMap bitmap;
    UWORD planes[2][16];
};

/* The storage owns only images created here, never application pointers. */
IPTR ZuneSetPointer(struct Window *window, IPTR type,
    struct ZunePointer **storage);
void ZuneFreePointer(struct Window *window, struct ZunePointer **storage);

#endif
