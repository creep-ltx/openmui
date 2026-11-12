/* mcfail: when MakeClass's own memory allocation fails, does intuition leave the superclass's
   cl_SubclassCount raised? (OS behaviour, black box: AllocMem patched to fail once) */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <intuition/classes.h>
APTR orig;
volatile ULONG failnext, failed;
__asm__(".globl _stub\n_stub:\n tst.l _failnext\n beq.s 1f\n clr.l _failnext\n addq.l #1,_failed\n moveq #0,d0\n rts\n1: move.l _orig,-(sp)\n rts\n");
extern void stub(void);
struct IntuitionBase *IntuitionBase;
int main(void)
{
    Class *s, *c;
    ULONG before, after, oc;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    if (!IntuitionBase) return 20;
    s = MakeClass(NULL, "rootclass", NULL, 4, 0);
    if (!s) return 20;
    c = MakeClass(NULL, NULL, s, 4, 0);
    Printf("normal subclass %s, count %ld\n", (ULONG)(c ? "made" : "failed"), (ULONG)s->cl_SubclassCount);
    if (c) FreeClass(c);
    Printf("after FreeClass count %ld\n", (ULONG)s->cl_SubclassCount);
    before = s->cl_SubclassCount; oc = s->cl_ObjectCount;
    Forbid();
    orig = SetFunction((struct Library *)SysBase, -198, (APTR)stub);
    failnext = 1;
    c = MakeClass(NULL, NULL, s, 4, 0);
    failnext = 0;
    SetFunction((struct Library *)SysBase, -198, orig);
    Permit();
    after = s->cl_SubclassCount;
    Printf("subclass with its AllocMem failed: %s (%ld failed), superclass count %ld -> %ld, objects %ld -> %ld\n",
           (ULONG)(c ? "made" : "failed"), failed, before, after, oc, (ULONG)s->cl_ObjectCount);
    if (c) FreeClass(c);
    if (s->cl_SubclassCount) s->cl_SubclassCount = 0;   /* so that the test's own class can go */
    Printf("FreeClass(super) %s\n", (ULONG)(FreeClass(s) ? "ok" : "refused"));
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
