/*
    Copyright (C) 2002, The AROS Development Team.
    All rights reserved.
    
*/

#include <string.h>

#include <intuition/classes.h>
#include <clib/alib_protos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/keymap.h>
#include <proto/utility.h>

#include "mui.h"
#include "support.h"
#include "muimaster_intern.h"

extern struct Library *MUIMasterBase;

/**************************************************************************
 check if region is entirely within given bounds
**************************************************************************/
int isRegionWithinBounds(struct Region *r, int left, int top, int width,
    int height)
{
    if ((left <= r->bounds.MinX) && (left + width - 1 >= r->bounds.MaxX)
        && (top <= r->bounds.MinY) && (top + height - 1 >= r->bounds.MaxY))
        return 1;

    return 0;
}


/**************************************************************************
 Converts a Rawkey to a vanillakey
**************************************************************************/
static __attribute__((noinline)) ULONG MapKey(struct IntuiMessage *message,
    APTR address)
{
    struct InputEvent event = { 0 };
    UBYTE character = 0;
    event.ie_Class = IECLASS_RAWKEY;
    event.ie_Code = message->Code;
    event.ie_Qualifier = message->Qualifier;
    event.ie_EventAddress = address;
    if (MapRawKey(&event, (STRPTR)&character, 1, NULL) <= 0) return 0;
    return character;
}

/* The last key converted: one key event is converted three to five times
 * (the window's control character check, Area's, String's, its edit hook,
 * every object a key nobody ate is offered to), and MapRawKey is a good
 * share of a key's time on a 68020. The answer depends only on the code,
 * the qualifiers, the dead keys before it and the keymap; the entry is
 * only for the same event (its time stamp), so a keymap changed between
 * two events is seen. Read and written in Forbid: programs share it.
 * The code no raw key has (0xffff) until the first conversion. */
static struct
{
    ULONG secs, micros;
    APTR address;
    UWORD code, qualifier;
    ULONG result;
} key_cache = { 0, 0, NULL, 0xffff, 0, 0 };

ULONG ConvertKey(struct IntuiMessage *message)
{
    APTR address;
    ULONG result;

    /* a key release maps to no character (keymap.library's MapRawKey
     * gives 0 for every up code, OS 3.1 and 3.2.1); the release of a key
     * cost two MapRawKey calls (opbench "count": the window converts the
     * press code, the active object the release code, and they replaced
     * each other in the cache) */
    if (message->Code & IECODE_UP_PREFIX)
        return 0;
    address = message->IAddress ? *(APTR *)message->IAddress : NULL;
    Forbid();
    if (key_cache.micros == message->Micros
        && key_cache.secs == message->Seconds
        && key_cache.code == message->Code
        && key_cache.qualifier == message->Qualifier
        && key_cache.address == address)
    {
        result = key_cache.result;
        Permit();
        return result;
    }
    Permit();
    result = MapKey(message, address);
    Forbid();
    key_cache.secs = message->Seconds;
    key_cache.micros = message->Micros;
    key_cache.code = message->Code;
    key_cache.qualifier = message->Qualifier;
    key_cache.address = address;
    key_cache.result = result;
    Permit();
    return result;
}

/**************************************************************************
 Convenient way to get an attribute of an object easily. If the object
 doesn't support the attribute this call returns an undefined value. So use
 this call only if the attribute is known to be known by the object.
 Implemented as a macro when compiling with GCC.
**************************************************************************/
#ifndef __GNUC__
IPTR XGET(Object * obj, Tag attr)
{
    IPTR storage = 0;
    GetAttr(attr, obj, &storage);
    return storage;
}
#endif /* __GNUC__ */

/**************************************************************************
 Call the Setup Method of an given object, but before set the renderinfo
**************************************************************************/
IPTR DoSetupMethod(Object * obj, struct MUI_RenderInfo * info)
{
    /* MUI set the correct render info *before* it calls MUIM_Setup so please
     * only use this function instead of DoMethodA() */
    muiRenderInfo(obj) = info;
    return DoMethod(obj, MUIM_Setup, (IPTR) info);
}

IPTR DoShowMethod(Object * obj)
{
    IPTR ret;

    ret = DoMethod(obj, MUIM_Show);
    if (ret)
        _flags(obj) |= MADF_CANDRAW;
    return ret;
}

IPTR DoHideMethod(Object * obj)
{
    _flags(obj) &= ~MADF_CANDRAW;
    return DoMethod(obj, MUIM_Hide);
}


void *Node_Next(APTR node)
{
    if (node == NULL)
        return NULL;
    if (((struct MinNode *)node)->mln_Succ == NULL)
        return NULL;
    if (((struct MinNode *)node)->mln_Succ->mln_Succ == NULL)
        return NULL;
    return ((struct MinNode *)node)->mln_Succ;
}

void *List_First(APTR list)
{
    if (!((struct MinList *)list)->mlh_Head)
        return NULL;
    if (((struct MinList *)list)->mlh_Head->mln_Succ == NULL)
        return NULL;
    return ((struct MinList *)list)->mlh_Head;
}

/* subtract rectangle b from rectangle b. resulting rectangles will be put into
   destrectarray which must have place for at least 4 rectangles. Returns number
   of resulting rectangles */

WORD SubtractRectFromRect(struct Rectangle *a, struct Rectangle *b,
    struct Rectangle *destrectarray)
{
    struct Rectangle intersect;
    BOOL intersecting = FALSE;
    WORD numrects = 0;

    /* calc. intersection between a and b */

    if (a->MinX <= b->MaxX)
    {
        if (a->MinY <= b->MaxY)
        {
            if (a->MaxX >= b->MinX)
            {
                if (a->MaxY >= b->MinY)
                {
                    intersect.MinX = MAX(a->MinX, b->MinX);
                    intersect.MinY = MAX(a->MinY, b->MinY);
                    intersect.MaxX = MIN(a->MaxX, b->MaxX);
                    intersect.MaxY = MIN(a->MaxY, b->MaxY);

                    intersecting = TRUE;
                }
            }
        }
    }

    if (!intersecting)
    {
        destrectarray[numrects++] = *a;

    }                           /* not intersecting */
    else
    {
        if (intersect.MinY > a->MinY)   /* upper */
        {
            destrectarray->MinX = a->MinX;
            destrectarray->MinY = a->MinY;
            destrectarray->MaxX = a->MaxX;
            destrectarray->MaxY = intersect.MinY - 1;

            numrects++;
            destrectarray++;
        }

        if (intersect.MaxY < a->MaxY)   /* lower */
        {
            destrectarray->MinX = a->MinX;
            destrectarray->MinY = intersect.MaxY + 1;
            destrectarray->MaxX = a->MaxX;
            destrectarray->MaxY = a->MaxY;

            numrects++;
            destrectarray++;
        }

        if (intersect.MinX > a->MinX)   /* left */
        {
            destrectarray->MinX = a->MinX;
            destrectarray->MinY = intersect.MinY;
            destrectarray->MaxX = intersect.MinX - 1;
            destrectarray->MaxY = intersect.MaxY;

            numrects++;
            destrectarray++;
        }

        if (intersect.MaxX < a->MaxX)   /* right */
        {
            destrectarray->MinX = intersect.MaxX + 1;
            destrectarray->MinY = intersect.MinY;
            destrectarray->MaxX = a->MaxX;
            destrectarray->MaxY = intersect.MaxY;

            numrects++;
            destrectarray++;
        }

    }                           /* intersecting */

    return numrects;

}

unsigned long long ZuneDiv64(unsigned long long n, ULONG d, ULONG *rem)
{
    ULONG hi = (ULONG)(n >> 32), lo = (ULONG)n, r = 0;

    if (d <= 0xffff)
    {
        /* Four 32-by-16 steps: the remainder stays below d, so each
         * quotient digit fits 16 bits (one divu.w on any 68k) */
        UWORD digit[4];
        int i;

        digit[0] = hi >> 16; digit[1] = hi; digit[2] = lo >> 16; digit[3] = lo;
        for (i = 0; i < 4; i++)
        {
            ULONG t = (r << 16) | digit[i];
#if defined(__mc68000__)
            __asm__("divu.w %1,%0" : "+d"(t) : "d"((UWORD)d));
            digit[i] = (UWORD)t;
            r = t >> 16;
#else
            digit[i] = (UWORD)(t / d);
            r = t % d;
#endif
        }
        hi = ((ULONG)digit[0] << 16) | digit[1];
        lo = ((ULONG)digit[2] << 16) | digit[3];
    }
    else
    {
        /* Shift and subtract: n shifts out of hi:lo into r while the
         * quotient bits shift in at the bottom */
        int i;

        for (i = 0; i < 64; i++)
        {
            ULONG carry = r >> 31;

            r = (r << 1) | (hi >> 31);
            hi = (hi << 1) | (lo >> 31);
            lo <<= 1;
            if (carry || r >= d)
            {
                r -= d;
                lo |= 1;
            }
        }
    }
    if (rem)
        *rem = r;
    return ((unsigned long long)hi << 32) | lo;
}

ULONG IsObjectVisible(Object * child, struct Library * MUIMasterBase)
{
    Object *wnd;
    Object *obj;

    wnd = _win(child);
    obj = child;

    while (get(obj, MUIA_Parent, &obj))
    {
        if (!obj)
            break;
        if (obj == wnd)
            break;

        if (_right(child) < _mleft(obj) || _left(child) > _mright(obj)
            || _bottom(child) < _mtop(obj) || _top(child) > _mbottom(obj))
            return FALSE;
    }
    return TRUE;
}

#if ZUNE68_TRACE
#define TRACE_FILE_LIMIT 65536L
struct TraceBuffer
{
    STRPTR next;
    ULONG left;
};

static ASM void TracePutChar(REG(d0, UBYTE chr),
    REG(a3, struct TraceBuffer *buffer))
{
    if (buffer->left)
    {
        *buffer->next++ = chr;
        buffer->left--;
    }
}

void ZuneTraceOutput(CONST_STRPTR fmt, ...)
{
    BPTR fh = Output();
    struct SignalSemaphore *sem;
    char text[512];
    struct TraceBuffer buffer;
    LONG end;

    if (fh)
    {
        VFPrintf(fh, fmt, (APTR)(&fmt + 1));
        Flush(fh);
        return; /* the caller owns its Shell output */
    }

    /* Diagnostic-only stack storage; no retained DOS handles or heap.
     * Drop a contended trace instead of blocking another application's GUI.
     * The library semaphore also serializes writers to the bounded file. */
    sem = &((struct MUIMasterBase_intern *)MUIMasterBase)->ZuneSemaphore;
    if (!AttemptSemaphore(sem)) return;
    buffer.next = text;
    buffer.left = sizeof(text) - 1;
    RawDoFmt(fmt, (APTR)(&fmt + 1), (VOID_FUNC)TracePutChar, &buffer);
    *buffer.next = 0;

    fh = Open("T:zune.log", MODE_READWRITE);
    if (fh)
    {
        if (Seek(fh, 0, OFFSET_END) < 0)
            end = -1;
        else
            end = Seek(fh, 0, OFFSET_CURRENT);
        if (end < 0 || end > TRACE_FILE_LIMIT - (LONG)strlen(text))
        {
            Close(fh);
            fh = end < 0 ? 0 : Open("T:zune.log", MODE_NEWFILE);
        }
    }
    if (fh)
    {
        Write(fh, text, strlen(text));
        Close(fh);
    }
    ReleaseSemaphore(sem);
}
#endif

/* The library's small blocks: every object (the rootclass part, which
 * Notify makes and frees itself) and the notification nodes come from
 * one exec pool shared by all programs, under Forbid (its calls never
 * wait). exec's own AllocMem/FreeMem walk the whole free list of a
 * fragmented machine for every block: about 30 % of an application's
 * creation on a 68020 (opbench prof), where MUI 3.8 pools its objects.
 * Sizes are rounded to longwords; the same size frees the block. */
static APTR zune_pool;

APTR zune_pool_alloc(register ULONG size __asm("d0"))
{
    ULONG *p, *q, n;

    size = (size + 3) & ~3UL;
    Forbid();
    if (!zune_pool)
        zune_pool = CreatePool(MEMF_ANY, ZUNE_POOL_PUDDLE, ZUNE_POOL_THRESH);
    p = zune_pool ? AllocPooled(zune_pool, size) : NULL;
    Permit();
    if ((q = p))
    {
        /* cleared four longwords a turn */
        n = size >> 2;
        switch (n & 3)
        {
        case 3: *q++ = 0;   /* fall through */
        case 2: *q++ = 0;   /* fall through */
        case 1: *q++ = 0;
        }
        for (n >>= 2; n; n--)
        {
            q[0] = 0; q[1] = 0; q[2] = 0; q[3] = 0;
            q += 4;
        }
    }
    return p;
}

void zune_pool_free(register APTR block __asm("a1"),
    register ULONG size __asm("d0"))
{
    if (!block)
        return;
    Forbid();
    FreePooled(zune_pool, block, (size + 3) & ~3UL);
    Permit();
}

/* At expunge: every object is gone by then */
void zune_pool_delete(void)
{
    if (zune_pool)
        DeletePool(zune_pool);
    zune_pool = NULL;
}

Object *ZuneNextObject(APTR iterator)
{
    struct _Object **cursor = iterator;
    struct _Object *header;
    if (!cursor || !(header = *cursor) || !header->o_Node.mln_Succ)
        return NULL;
    *cursor = (struct _Object *)header->o_Node.mln_Succ;
    return (Object *)BASEOBJECT(header);
}

/* utility.library's NextTagItem in the library's own code (support.h):
 * every OM_SET and OM_NEW walks its tags once per class, dozens of calls
 * per operation (a radio set 118, a list insert 142: opbench "count",
 * tests/native/syscount.h); MUI 3.8 makes none. The same interface as the
 * library vector: a0 = the list pointer's address, the tag in d0, d1/a1
 * used. TAG_DONE, TAG_IGNORE, TAG_MORE and TAG_SKIP as utility's; at the
 * end the list pointer is NULL. */
asm(
    "\t.text\n\t.even\n\t.globl _ZuneNextTagItem\n"
    "_ZuneNextTagItem:\n"
    "\tmove.l (%a0),%d0\n"
    "1:\tbeq.s 9f\n"                    /* no list (left) */
    "\tmove.l %d0,%a1\n"
    "\tmove.l (%a1),%d1\n"              /* ti_Tag */
    "\tcmp.l #3,%d1\n"
    "\tbhi.s 8f\n"                      /* an attribute */
    "\tsubq.l #1,%d1\n"
    "\tbmi.s 9f\n"                      /* TAG_DONE */
    "\tbeq.s 6f\n"                      /* TAG_IGNORE */
    "\tsubq.l #1,%d1\n"
    "\tbeq.s 5f\n"                      /* TAG_MORE */
    "\tmove.l 4(%a1),%d1\n"             /* TAG_SKIP: ti_Data + 1 items on */
    "\taddq.l #1,%d1\n"
    "\tlsl.l #3,%d1\n"
    "\tadd.l %d1,%d0\n"
    "\tbra.s 1b\n"
    "5:\tmove.l 4(%a1),%d0\n"           /* TAG_MORE: ti_Data, or the end */
    "\tbra.s 1b\n"
    "6:\taddq.l #8,%d0\n"
    "\tbra.s 1b\n"
    "8:\taddq.l #8,%a1\n"
    "\tmove.l %a1,(%a0)\n"
    "\trts\n"
    "9:\tclr.l (%a0)\n"
    "\tmoveq #0,%d0\n"
    "\trts\n");


/* A message port that signals with another port's signal bit. MUI 3.9
 * multiplexes an application's internal ports on one signal; a task such
 * as IBrowse otherwise runs out of signals for OpenClipboard(). Receivers
 * drain such ports with GetMsg() whenever the shared signal arrives. */
struct MsgPort *ZuneCreateSharedPort(struct MsgPort *owner)
{
    struct MsgPort *port;

    if (!owner)
        return CreateMsgPort();
    port = AllocMem(sizeof(*port), MEMF_PUBLIC | MEMF_CLEAR);
    if (port)
    {
        port->mp_Node.ln_Type = NT_MSGPORT;
        port->mp_Flags = PA_SIGNAL;
        port->mp_SigBit = owner->mp_SigBit;
        port->mp_SigTask = owner->mp_SigTask;
        NewList(&port->mp_MsgList);
    }
    return port;
}

void ZuneDeleteSharedPort(struct MsgPort *port)
{
    if (port)
        FreeMem(port, sizeof(*port));
}
