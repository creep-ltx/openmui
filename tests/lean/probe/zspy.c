/* zspy: remember every MUI Window and Application object made after it
 * starts (a patch on intuition's NewObjectA and DisposeObject), so that
 * objdump can find another program's windows.  Run in the background;
 * Ctrl-C removes the patches.  Black-box: only public OS calls. */
#include <exec/ports.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <string.h>
#include <clib/alib_protos.h>
#include "zspy.h"

struct Library *MUIMasterBase;
extern struct IntuitionBase *IntuitionBase;
static struct ZSpy *spy;
static APTR oldnew, olddispose;

static int isa(Object *o, struct IClass *k)
{
    struct IClass *c = OCLASS(o);
    for (; c; c = c->cl_Super)
        if (c == k) return 1;
    return 0;
}

APTR callnew(APTR f __asm("a3"), struct IClass *c __asm("a0"), UBYTE *id __asm("a1"),
    struct TagItem *t __asm("a2"), struct Library *b __asm("a6"));
__asm(".text\n.globl _callnew\n_callnew:\n\tjmp (a3)\n");
ULONG calldispose(APTR f __asm("a3"), Object *o __asm("a0"), struct Library *b __asm("a6"));
__asm(".text\n.globl _calldispose\n_calldispose:\n\tjmp (a3)\n");

static APTR mynew(struct IClass *c __asm("a0"), UBYTE *id __asm("a1"),
    struct TagItem *t __asm("a2"), struct Library *b __asm("a6"))
{
    APTR o = callnew(oldnew, c, id, t, b);
    if (o && spy)
    {
        int kind = isa(o, spy->wincl) ? 1 : isa(o, spy->appcl) ? 2 : 0;
        if (kind)
        {
            int i;
            Forbid();
            for (i = 0; i < ZSPY_MAX; i++)
                if (!spy->obj[i]) { spy->obj[i] = o; spy->kind[i] = kind; break; }
            Permit();
        }
    }
    return o;
}

static ULONG mydispose(Object *o __asm("a0"), struct Library *b __asm("a6"))
{
    if (o && spy)
    {
        int i;
        Forbid();
        for (i = 0; i < ZSPY_MAX; i++)
            if (spy->obj[i] == o) spy->obj[i] = NULL;
        Permit();
    }
    return calldispose(olddispose, o, b);
}

int main(void)
{
    struct IClass *w, *a;
    if (!(MUIMasterBase = OpenLibrary("muimaster.library", 19))) return 20;
    w = (struct IClass *)MUI_GetClass(MUIC_Window);
    a = (struct IClass *)MUI_GetClass(MUIC_Application);
    if (!w || !a || FindPort(ZSPY_NAME)) { CloseLibrary(MUIMasterBase); return 10; }
    spy = AllocMem(sizeof(*spy), MEMF_PUBLIC | MEMF_CLEAR);
    spy->port.mp_Node.ln_Name = ZSPY_NAME;
    spy->port.mp_Flags = PA_IGNORE;
    NewList(&spy->port.mp_MsgList);
    spy->wincl = w; spy->appcl = a;
    AddPort(&spy->port);
    Forbid();
    oldnew = SetFunction((struct Library *)IntuitionBase, -636, (APTR)mynew);
    olddispose = SetFunction((struct Library *)IntuitionBase, -642, (APTR)mydispose);
    Permit();
    Wait(SIGBREAKF_CTRL_C);
    Forbid();
    SetFunction((struct Library *)IntuitionBase, -636, oldnew);
    SetFunction((struct Library *)IntuitionBase, -642, olddispose);
    RemPort(&spy->port);
    Permit();
    /* the memory stays: a caller may still be inside the patch */
    CloseLibrary(MUIMasterBase);
    return 0;
}
