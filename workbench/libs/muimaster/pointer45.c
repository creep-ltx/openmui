/* MUI 4/5's pointer shapes (MUIA_PointerType, V20): classic two-plane
 * images drawn here, the normal and busy pointers are intuition's.
 * Built into the module Zmui45.mui (classes/zmui45.c, modules.h): a MUI 3
 * program never asks for a shape, so it never loads this; the library
 * keeps the normal, busy and borrowed pointers and the freeing
 * (pointer.c). No RTG or FPU is required. */
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/pointerclass.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <string.h>
#include "mui.h"
#include "modules.h"
#include "pointer.h"

static void pixel(UWORD *bits, int x, int y)
{
    if (x >= 0 && x < 16 && y >= 0 && y < 16)
        bits[y] |= 0x8000U >> x;
}

static void line(UWORD *bits, int x, int y, int dx, int dy, int n)
{
    while (n-- > 0)
    {
        pixel(bits, x, y);
        x += dx;
        y += dy;
    }
}

static void arrow(UWORD *bits, int dx, int dy)
{
    int x = 7 + dx * 5, y = 7 + dy * 5;
    line(bits, 7, 7, dx, dy, 6);
    if (dx && dy)
    {
        line(bits, x, y, -dx, 0, 4);
        line(bits, x, y, 0, -dy, 4);
    }
    else
    {
        line(bits, x, y, -dx + dy, -dy + dx, 4);
        line(bits, x, y, -dx - dy, -dy - dx, 4);
    }
}

static void box(UWORD *bits, int x, int y, int w, int h)
{
    line(bits, x, y, 1, 0, w);
    line(bits, x, y + h - 1, 1, 0, w);
    line(bits, x, y, 0, 1, h);
    line(bits, x + w - 1, y, 0, 1, h);
}

static void shape(struct ZunePointer *p, ULONG type)
{
    UWORD *bits = p->planes[0];
    int x, y;

    memset(p->planes, 0, sizeof(p->planes));
    switch (type)
    {
    case MUIV_PointerType_None:
        break;
    case MUIV_PointerType_Text:
        line(bits, 7, 2, 0, 1, 12);
        line(bits, 4, 2, 1, 0, 7);
        line(bits, 4, 13, 1, 0, 7);
        break;
    case MUIV_PointerType_VerticalText:
        line(bits, 2, 7, 1, 0, 12);
        line(bits, 2, 4, 0, 1, 7);
        line(bits, 13, 4, 0, 1, 7);
        break;
    case MUIV_PointerType_EastResize: arrow(bits, 1, 0); break;
    case MUIV_PointerType_WestResize: arrow(bits, -1, 0); break;
    case MUIV_PointerType_NorthResize: arrow(bits, 0, -1); break;
    case MUIV_PointerType_SouthResize: arrow(bits, 0, 1); break;
    case MUIV_PointerType_NorthEastResize: arrow(bits, 1, -1); break;
    case MUIV_PointerType_NorthWestResize: arrow(bits, -1, -1); break;
    case MUIV_PointerType_SouthEastResize: arrow(bits, 1, 1); break;
    case MUIV_PointerType_SouthWestResize: arrow(bits, -1, 1); break;
    case MUIV_PointerType_ColumnResize:
    case MUIV_PointerType_EastWestResize:
        arrow(bits, 1, 0); arrow(bits, -1, 0); break;
    case MUIV_PointerType_RowResize:
    case MUIV_PointerType_NorthSouthResize:
        arrow(bits, 0, 1); arrow(bits, 0, -1); break;
    case MUIV_PointerType_NorthEastSouthWestResize:
        arrow(bits, 1, -1); arrow(bits, -1, 1); break;
    case MUIV_PointerType_NorthWestSouthEastResize:
        arrow(bits, -1, -1); arrow(bits, 1, 1); break;
    case MUIV_PointerType_ScrollAll:
        arrow(bits, 1, 0); arrow(bits, -1, 0);
        arrow(bits, 0, 1); arrow(bits, 0, -1); break;
    case MUIV_PointerType_Cross:
    case MUIV_PointerType_Cell:
        line(bits, 7, 1, 0, 1, 14);
        line(bits, 1, 7, 1, 0, 14);
        if (type == MUIV_PointerType_Cell) box(bits, 4, 4, 7, 7);
        break;
    case MUIV_PointerType_Hand:
    case MUIV_PointerType_Link:
    case MUIV_PointerType_DragAndDrop:
        box(bits, 5, 1, 3, 8);
        line(bits, 8, 5, 1, 0, 5);
        line(bits, 12, 5, 0, 1, 6);
        line(bits, 12, 10, -1, 1, 4);
        line(bits, 5, 13, 1, 0, 5);
        line(bits, 2, 9, 1, 1, 4);
        line(bits, 2, 7, 0, 1, 3);
        line(bits, 2, 7, 1, 0, 4);
        break;
    case MUIV_PointerType_NoDrop:
    case MUIV_PointerType_NotAllowed:
        box(bits, 3, 3, 10, 10);
        line(bits, 3, 12, 1, -1, 10);
        break;
    case MUIV_PointerType_ZoomIn:
    case MUIV_PointerType_ZoomOut:
        box(bits, 2, 2, 9, 9);
        line(bits, 10, 10, 1, 1, 5);
        line(bits, 4, 6, 1, 0, 5);
        if (type == MUIV_PointerType_ZoomIn)
            line(bits, 6, 4, 0, 1, 5);
        break;
    case MUIV_PointerType_Help:
        line(bits, 4, 2, 1, 0, 6);
        line(bits, 10, 3, 0, 1, 3);
        line(bits, 10, 5, -1, 1, 4);
        line(bits, 7, 8, 0, 1, 2);
        pixel(bits, 7, 12);
        break;
    case MUIV_PointerType_Pen:
    case MUIV_PointerType_Smudge:
        line(bits, 3, 12, 1, -1, 10);
        line(bits, 4, 13, 1, -1, 10);
        line(bits, 3, 13, 1, 0, 3);
        if (type == MUIV_PointerType_Smudge) box(bits, 2, 11, 5, 4);
        break;
    case MUIV_PointerType_Rotate:
        box(bits, 3, 3, 10, 10);
        line(bits, 12, 3, -1, -1, 3);
        line(bits, 12, 3, -1, 1, 3);
        break;
    case MUIV_PointerType_Rubber:
        box(bits, 3, 5, 10, 7);
        line(bits, 8, 5, 0, 1, 7);
        break;
    case MUIV_PointerType_Select:
        for (x = 2; x <= 12; x += 2)
        {
            pixel(bits, x, 2); pixel(bits, x, 12);
            pixel(bits, 2, x); pixel(bits, 12, x);
        }
        break;
    case MUIV_PointerType_Progress:
        box(bits, 4, 2, 8, 12);
        line(bits, 4, 3, 1, 1, 7);
        line(bits, 11, 3, -1, 1, 7);
        line(bits, 5, 12, 1, 0, 6);
        break;
    case MUIV_PointerType_Copy:
        box(bits, 2, 2, 8, 9); box(bits, 6, 6, 8, 9); break;
    case MUIV_PointerType_Alias:
        arrow(bits, 1, -1); box(bits, 2, 8, 6, 6); break;
    case MUIV_PointerType_Menu:
    case MUIV_PointerType_ContextMenu:
        box(bits, 2, 2, 12, 12);
        for (y = 5; y <= 11; y += 3) line(bits, 5, y, 1, 0, 6);
        break;
    }

    /* Colour 1 for the strokes, colour 2 for a one-pixel outline. */
    for (y = 0; y < 16; y++)
    {
        UWORD edge = bits[y] | (bits[y] << 1) | (bits[y] >> 1);
        if (y) edge |= bits[y - 1];
        if (y < 15) edge |= bits[y + 1];
        p->planes[1][y] = edge & ~bits[y];
    }
}

/* The image of a shape for the window (the old one detached and
 * disposed first), kept in *storage (allocated here the first time;
 * ZuneFreePointer frees it); NULL when there is no memory or no image */
Object *zune_pointer_image(struct Window *window, ULONG type,
    struct ZunePointer **storage)
{
    struct ZunePointer *p = *storage;

    if (!p)
    {
        p = AllocVec(sizeof(*p), MEMF_PUBLIC | MEMF_CLEAR);
        if (!p)
            return NULL;
        *storage = p;
    }
    if (p->image)
    {
        SetWindowPointerA(window, NULL);
        DisposeObject(p->image);
        p->image = NULL;
    }
    shape(p, type);
    p->bitmap.BytesPerRow = 2;
    p->bitmap.Rows = 16;
    p->bitmap.Depth = 2;
    p->bitmap.Planes[0] = (PLANEPTR)p->planes[0];
    p->bitmap.Planes[1] = (PLANEPTR)p->planes[1];
    {
        struct TagItem ptags[] = {
            {POINTERA_BitMap, (IPTR)&p->bitmap},
            {POINTERA_WordWidth, 1},
            {POINTERA_XOffset, -7}, {POINTERA_YOffset, -7},
            {POINTERA_XResolution, POINTERXRESN_SCREENRES},
            {POINTERA_YResolution, POINTERYRESN_SCREENRESASPECT},
            {TAG_DONE, 0}
        };
        p->image = NewObjectA(NULL, "pointerclass", ptags);
    }
    return p->image;
}
