/* A window's pointer (MUIA_PointerType): intuition's normal and busy
 * pointers and a borrowed pointerclass object here; MUI 4/5's drawn
 * shapes come from the module Zmui45 (pointer45.c, modules.h), which a
 * MUI 3 program never loads. Without the module a shape is the normal
 * pointer, as on low memory. */
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include "mui.h"
#include "modules.h"
#include "pointer.h"

void ZuneFreePointer(struct Window *window, struct ZunePointer **storage)
{
    struct ZunePointer *p = *storage;

    if (window)
        SetWindowPointerA(window, NULL);
    if (p)
    {
        if (p->image) DisposeObject(p->image);
        FreeVec(p);
        *storage = NULL;
    }
}

IPTR ZuneSetPointer(struct Window *window, IPTR type,
    struct ZunePointer **storage)
{
    struct TagItem tags[] = {{WA_Pointer, 0}, {TAG_DONE, 0}};

    if (type == MUIV_PointerType_Normal || type == (IPTR)MUIV_PointerType_Parent)
    {
        SetWindowPointerA(window, NULL);
        return MUIV_PointerType_Normal;
    }
    if (type == MUIV_PointerType_Busy)
    {
        tags[0].ti_Tag = WA_BusyPointer;
        tags[0].ti_Data = TRUE;
    }
    else if (type < MUIV_PointerType_Count)
    {
        if (!(tags[0].ti_Data = (IPTR)zune_pointer_image(window, type,
            storage)))
            goto normal;
    }
    else
        tags[0].ti_Data = type; /* borrowed pointerclass object */
    SetWindowPointerA(window, tags);
    return type;
normal:
    SetWindowPointerA(window, NULL);
    return MUIV_PointerType_Normal;
}
