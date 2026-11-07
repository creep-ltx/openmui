/* gfxspy: log (KPrintF, read with wasabi debug) every TextLength() that
 * (or TextExtent()) measures more than 300 pixels, with the rastport's font (pointer, name,
 * sizes) and the calling task; and every SetFont() whose font has a size
 * above 100. Ctrl-C removes the patches. Black-box: graphics.library only.
 * Found for IBrowse 2.5.9's toolbar, whose labels measured 248 pixels a
 * character on Zune. */
#include <exec/tasks.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <clib/debug_protos.h>

extern struct GfxBase *GfxBase;
static APTR oldlen, oldset, oldext;

WORD calllen(APTR f __asm("a3"), struct RastPort *rp __asm("a1"), STRPTR s __asm("a0"),
    UWORD n __asm("d0"), struct Library *b __asm("a6"));
__asm(".text\n.globl _calllen\n_calllen:\n\tjmp (a3)\n");
void callset(APTR f __asm("a3"), struct RastPort *rp __asm("a1"), struct TextFont *tf __asm("a0"),
    struct Library *b __asm("a6"));
__asm(".text\n.globl _callset\n_callset:\n\tjmp (a3)\n");

void callext(APTR f __asm("a3"), struct RastPort *rp __asm("a1"), STRPTR s __asm("a0"),
    UWORD n __asm("d0"), struct TextExtent *te __asm("a2"), struct Library *b __asm("a6"));
__asm(".text\n.globl _callext\n_callext:\n\tjmp (a3)\n");

static const char *tname(void)
{
    struct Task *t = FindTask(NULL);
    return t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?";
}

static WORD mylen(struct RastPort *rp __asm("a1"), STRPTR s __asm("a0"), UWORD n __asm("d0"),
    struct Library *b __asm("a6"))
{
    WORD w = calllen(oldlen, rp, s, n, b);
    if (w > 300)
    {
        struct TextFont *f = rp->Font;
        KPrintF("GS len %ld n %ld task %s rp %lx font %lx", (LONG)w, (LONG)n, tname(), (ULONG)rp, (ULONG)f);
        if (f)
            KPrintF(" y %ld x %ld flags %lx name %lx", (LONG)f->tf_YSize, (LONG)f->tf_XSize,
                (ULONG)f->tf_Flags, (ULONG)f->tf_Message.mn_Node.ln_Name);
        KPrintF("\n");
    }
    return w;
}

static void myext(struct RastPort *rp __asm("a1"), STRPTR s __asm("a0"), UWORD n __asm("d0"),
    struct TextExtent *te __asm("a2"), struct Library *b __asm("a6"))
{
    callext(oldext, rp, s, n, te, b);
    if (te->te_Width > 300 || te->te_Height > 100)
        KPrintF("GS ext %ld x %ld n %ld task %s font %lx y %ld x %ld\n", (LONG)te->te_Width,
            (LONG)te->te_Height, (LONG)n, tname(), (ULONG)rp->Font,
            rp->Font ? (LONG)rp->Font->tf_YSize : 0L, rp->Font ? (LONG)rp->Font->tf_XSize : 0L);
}

static void myset(struct RastPort *rp __asm("a1"), struct TextFont *tf __asm("a0"),
    struct Library *b __asm("a6"))
{
    if (tf && tf->tf_YSize > 100)
        KPrintF("GS setfont task %s rp %lx font %lx y %ld\n", tname(), (ULONG)rp, (ULONG)tf,
            (LONG)tf->tf_YSize);
    callset(oldset, rp, tf, b);
}

int main(void)
{
    Forbid();
    oldlen = SetFunction((struct Library *)GfxBase, -54, (APTR)mylen);
    oldset = SetFunction((struct Library *)GfxBase, -66, (APTR)myset);
    oldext = SetFunction((struct Library *)GfxBase, -690, (APTR)myext);
    Permit();
    Wait(SIGBREAKF_CTRL_C);
    Forbid();
    SetFunction((struct Library *)GfxBase, -54, oldlen);
    SetFunction((struct Library *)GfxBase, -66, oldset);
    SetFunction((struct Library *)GfxBase, -690, oldext);
    Permit();
    return 0;
}
