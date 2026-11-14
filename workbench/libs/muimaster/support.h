/*
    Copyright (C) 2002-2013, The AROS Development Team. All rights reserved.
*/

#ifndef _MUIMASTER_SUPPORT_H
#define _MUIMASTER_SUPPORT_H

#ifndef EXEC_TYPES_H
#   include <exec/types.h>
#endif
#ifndef INTUITION_CLASSUSR_H
#   include <intuition/classusr.h>
#endif
#ifndef INTUITION_CLASSES_H
#   include <intuition/classes.h>
#endif
#ifndef CLIB_MACROS_H
#   include <clib/macros.h>
#endif

#ifdef __AROS__
#   ifndef AROS_ASMCALL_H
#       include <aros/asmcall.h>
#   endif
#   include <aros/macros.h>
#   define IMSPEC_EXTERNAL_PREFIX "IMAGES:Zune/"
#   include "support_aros.h"
#else
#   include "support_amigaos.h"
#endif

struct MUI_RenderInfo;
struct IntuiMessage;
struct Region;
struct Rectangle;
struct Object;
struct Library;

/* support.c: a cleared block freed with its size (AllocMem/FreeMem with
 * register arguments: no pushed arguments on the callers' frames) */
APTR zune_mem_alloc(ULONG size __asm("d0"));
void zune_mem_free(APTR block __asm("a1"), ULONG size __asm("d0"));

#define mui_alloc(x) AllocVec(x,MEMF_CLEAR)
#define mui_alloc_struct(x) ((x *)AllocVec(sizeof(x),MEMF_CLEAR))
#define mui_free(x) FreeVec(x)


int isRegionWithinBounds(struct Region *r, int left, int top, int width,
    int height);
ULONG ConvertKey(struct IntuiMessage *imsg);

#define _between(a,x,b) ((x)>=(a) && (x)<=(b))
#define _isinobject(obj, x, y) (_between(_mleft(obj), (x), _mright(obj)) \
                          && _between(_mtop(obj), (y), _mbottom(obj)))

/* add mask in flags if tag is true, else sub mask */
#define _handle_bool_tag(flags, tag, mask) \
((tag != 0) ? ((flags) |= (mask)) : ((flags) &= ~(mask)))

#define CLAMP(x, low, high) \
    (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#ifndef __GNUC__
IPTR XGET(Object * obj, Tag attr);
#endif

IPTR DoSetupMethod(Object * obj, struct MUI_RenderInfo *info);
IPTR DoShowMethod(Object * obj);
IPTR DoHideMethod(Object * obj);
/* Stack-owned modal input guard, cancelled before a window is closed. */
struct ZuneWindowGuard
{
    struct ZuneWindowGuard *next;
    struct MUI_EventHandlerNode *handler;
    struct IntuiMessage *menu_message;
    BOOL closed;
};
void ZuneRememberWindowPosition(Object *window);
void ZuneWatchWindow(Object *window, struct ZuneWindowGuard *guard);
void ZuneUnwatchWindow(Object *window, struct ZuneWindowGuard *guard);

void ZuneResetWindowPointer(Object *window);
IPTR DoLayoutMethod(Object *obj);
/* Unsigned 64-by-32-bit division (d != 0), quotient returned, remainder
 * stored when rem is not NULL. Numeric, Prop and String need nothing
 * wider; libgcc's general 64-by-64 __udivdi3/__umoddi3 are 3 KB. */
unsigned long long ZuneDiv64(unsigned long long n, ULONG d, ULONG *rem);
IPTR ZuneLayout(Object *obj, LONG left, LONG top, LONG width, LONG height,
    ULONG flags);

/* returns next node of this node */
void *Node_Next(APTR node);
/* returns first node of this list */
void *List_First(APTR list);

WORD SubtractRectFromRect(struct Rectangle *a, struct Rectangle *b,
    struct Rectangle *destrectarray);
ULONG IsObjectVisible(Object * child, struct Library *MUIMasterBase);

#endif /* _MUIMASTER_SUPPORT_H */

APTR ZuneAddClipRegion(struct MUI_RenderInfo *, struct Region *);
void ZuneRemoveClipRegion(struct MUI_RenderInfo *, APTR);

void ZuneDrawDisabled(Object *obj);
struct MsgPort *ZuneCreateSharedPort(struct MsgPort *owner);
void ZuneDeleteSharedPort(struct MsgPort *port);

Object *ZuneNextObject(APTR iterator);
#undef NextObject
#define NextObject ZuneNextObject

#if !defined(ZUNE_CLASSFILE) && !defined(__AROS__)
/* NextTagItem from the library's own code, not utility.library's vector
 * (support.c: ZuneNextTagItem); the same registers as the vector. Class
 * files keep utility's. */
#include <proto/utility.h>
#undef NextTagItem
#define NextTagItem(___tagListPtr) ({\
  register struct TagItem * __zv_ret __asm("d0");\
  register struct TagItem ** __zv0 __asm("a0") = (struct TagItem **)(___tagListPtr);\
  __asm volatile ("jsr _ZuneNextTagItem"\
                   : "=d"(__zv_ret), "+a"(__zv0)\
                   :\
                   : "cc", "memory", "d1", "a1");\
  __zv_ret;})
/* A RastPort as InitRastPort() leaves it, made once at library start:
 * InitRastPort costs about 95 us on OS 3.1 (tests/native/drawcost.c) and
 * a window's setup measured its texts with about 90 fresh ones; a copy of
 * this one is a few moves (library only, not the class files) */
extern struct RastPort zune_blank_rp;
#define zune_init_rastport(rp) (*(rp) = zune_blank_rp)

#endif
