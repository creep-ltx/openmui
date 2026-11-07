/* wintags: log (KPrintF, read with wasabi debug) every NewObjectA and
 * SetAttrsA whose tags include a window size or position (MUIA_Window_Width,
 * _Height, _LeftEdge, _TopEdge), with the whole tag list. Ctrl-C removes the patches. */
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include <clib/debug_protos.h>

extern struct IntuitionBase *IntuitionBase;
struct Library *UtilityBase;
static APTR oldnew, oldset;

APTR callnew(APTR f __asm("a3"), struct IClass *c __asm("a0"), UBYTE *id __asm("a1"),
    struct TagItem *t __asm("a2"), struct Library *b __asm("a6"));
__asm(".text\n.globl _callnew\n_callnew:\n\tjmp (a3)\n");
ULONG callset(APTR f __asm("a3"), Object *o __asm("a0"), struct TagItem *t __asm("a1"), struct Library *b __asm("a6"));
__asm(".text\n.globl _callset\n_callset:\n\tjmp (a3)\n");

static int interesting(struct TagItem *t)
{
    struct TagItem *st = t, *x;
    while ((x = NextTagItem(&st)))
        if (x->ti_Tag == MUIA_Window_Width || x->ti_Tag == MUIA_Window_Height || x->ti_Tag == MUIA_Window_LeftEdge || x->ti_Tag == MUIA_Window_TopEdge)
            return 1;
    return 0;
}
static void dump(const char *what, APTR o, struct TagItem *t)
{
    struct TagItem *st = t, *x;
    KPrintF("TS %s %lx:", what, (ULONG)o);
    while ((x = NextTagItem(&st)))
        if ((x->ti_Tag & 0xffff0000) == 0x80420000 || (x->ti_Tag & 0xff000000) != 0x80000000)
            KPrintF(" %lx=%lx", x->ti_Tag, x->ti_Data);
    KPrintF("\n");
}
static APTR mynew(struct IClass *c __asm("a0"), UBYTE *id __asm("a1"),
    struct TagItem *t __asm("a2"), struct Library *b __asm("a6"))
{
    APTR o = callnew(oldnew, c, id, t, b);
    if (t && interesting(t)) dump("new", o, t);
    return o;
}
static ULONG myset(Object *o __asm("a0"), struct TagItem *t __asm("a1"), struct Library *b __asm("a6"))
{
    if (t && interesting(t)) dump("set", o, t);
    return callset(oldset, o, t, b);
}

int main(void)
{
    UtilityBase = OpenLibrary("utility.library", 37);
    Forbid();
    oldnew = SetFunction((struct Library *)IntuitionBase, -636, (APTR)mynew);
    oldset = SetFunction((struct Library *)IntuitionBase, -648, (APTR)myset);
    Permit();
    Wait(SIGBREAKF_CTRL_C);
    Forbid();
    SetFunction((struct Library *)IntuitionBase, -636, oldnew);
    SetFunction((struct Library *)IntuitionBase, -648, oldset);
    Permit();
    return 0;
}
