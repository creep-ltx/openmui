/* gfxwatch: patch graphics.library RectFill, SetRast, EraseRect, ScrollRaster, Text,
   BltBitMapRastPort (a1 = the RastPort drawn into), BltBitMap (a1 = the destination BitMap) and
   BltClear (a1 = memory) for every task; a call whose target plane or memory lies inside a free
   memory chunk is reported through RawPutChar (wasabi debug): the function, the target, the
   task, the return addresses on the stack (RAM first) and the hunks of the calling process's
   program and of muimaster.library. Runs until Ctrl-C.
   Build: m68k-amigaos-gcc -Os -m68000 -noixemul -s -o gfxwatch gfxwatch.c gfxwatch.s */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <graphics/rastport.h>
#include <string.h>

extern APTR gw_old[19];
void gw_stub0(void), gw_stub1(void), gw_stub2(void), gw_stub3(void), gw_stub4(void), gw_stub5(void), gw_stub6(void), gw_stub7(void), gw_stub8(void), gw_stub9(void), gw_stub10(void), gw_stub11(void), gw_stub12(void), gw_stub13(void), gw_stub14(void), gw_stub15(void), gw_stub16(void), gw_stub17(void), gw_stub18(void);
static const LONG lvos[19] = { -306, -234, -810, -396, -60, -606, -30, -300, -552, -36, -312, -48, -42, -636, -1098, -246, -786, -774, -1056 };
static const char *names[19] = { "RectFill", "SetRast", "EraseRect", "ScrollRaster", "Text", "BltBitMapRastPort", "BltBitMap", "BltClear", "ClipBlit", "BltTemplate", "BltPattern", "ClearScreen", "ClearEOL", "BltMaskBitMapRastPort", "ScrollRasterBF", "Draw", "WritePixelArray8", "WritePixelLine8", "WriteChunkyPixels" };
static int nrep;

static void kput(const char *t)
{
    while (*t) {
        register UBYTE d0 __asm("d0") = *t++;
        register struct ExecBase *a6 __asm("a6") = SysBase;
        __asm volatile("jsr -516(a6)" : "+r"(d0) : "r"(a6) : "d1", "a0", "a1", "cc", "memory");
    }
}
static void khex(ULONG v)
{
    char b[10]; int i;
    for (i = 7; i >= 0; i--) { b[i] = "0123456789abcdef"[v & 15]; v >>= 4; }
    b[8] = ' '; b[9] = 0; kput(b);
}
static int inram(ULONG v)
{
    struct MemHeader *mh;
    if (v >= 0xF80000 && v < 0x1000000) return 1;
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ)
        if (v >= (ULONG)mh->mh_Lower + 8 && v < (ULONG)mh->mh_Upper) return 1;
    return 0;
}
static int isret(ULONG v)
{
    UWORD w2, w4, w6;
    if (v & 1 || v < 0x400 || !inram(v)) return 0;
    w2 = *(UWORD *)(v - 2); w4 = *(UWORD *)(v - 4); w6 = *(UWORD *)(v - 6);
    if ((w2 & 0xFFF8) == 0x4E90) return 1;
    if ((w4 & 0xFFF8) == 0x4EA8 || (w4 & 0xFFF8) == 0x4EB0 || w4 == 0x4EB8 || w4 == 0x4EBA || w4 == 0x4EBB) return 1;
    if (w4 == 0x6100) return 1;
    if ((w2 & 0xFF00) == 0x6100 && (w2 & 0xFF) != 0 && (w2 & 0xFF) != 0xFF) return 1;
    if (w6 == 0x4EB9 || w6 == 0x61FF) return 1;
    return 0;
}
/* is address v inside a free chunk? */
static int isfree(ULONG v)
{
    struct MemHeader *mh; struct MemChunk *mc; int n = 0;
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ) {
        if (v < (ULONG)mh->mh_Lower || v >= (ULONG)mh->mh_Upper) continue;
        for (mc = mh->mh_First; mc && n < 100000; mc = mc->mc_Next, n++)
            if (v >= (ULONG)mc && v < (ULONG)mc + mc->mc_Bytes) return 1;
        return 0;
    }
    return 0;
}
static void ranges(BPTR seg, const char *name)
{
    while (seg) {
        ULONG *p = (ULONG *)BADDR(seg);
        kput("  "); khex((ULONG)(p + 1)); khex((ULONG)p + p[-1] - 4); kput(name); kput("\n");
        seg = p[0];
    }
}

void gw_check(ULONG fn, ULONG a0, ULONG a1, ULONG *sp)
{
    ULONG target = 0; const char *what = "";
    if (nrep >= 6) return;
    if (fn >= 16) { struct RastPort *rp = (struct RastPort *)a0; if (!rp || !rp->BitMap) return; target = (ULONG)rp->BitMap->Planes[0]; what = "plane 0 of the RastPort's BitMap (a0)"; goto have; }
    if (!a1) return;
    if (fn <= 5 || fn >= 9) { struct RastPort *rp = (struct RastPort *)(fn == 8 ? a1 : a1); if (fn == 8) rp = (struct RastPort *)a1; if (!rp->BitMap) return; target = (ULONG)rp->BitMap->Planes[0]; what = "plane 0 of the RastPort's BitMap"; }
    else if (fn == 6) { struct BitMap *bm = (struct BitMap *)a1; target = (ULONG)bm->Planes[0]; what = "plane 0 of the destination BitMap"; }
    else if (fn == 7) { target = a1; what = "the memory"; }
    else { struct RastPort *rp = (struct RastPort *)a1; if (!rp->BitMap) return; target = (ULONG)rp->BitMap->Planes[0]; what = "plane 0 of the dest RastPort's BitMap"; }
have:
    if (!target || !isfree(target)) return;
    {
        struct Task *t = SysBase->ThisTask; struct Process *pr = (struct Process *)t; struct Library *lb; ULONG i; int k = 0;
        nrep++;
        kput("gfxwatch: "); kput(names[fn]); kput(" into free memory: "); kput(what); kput(" at "); khex(target);
        kput(" a0 "); khex(a0); kput(" a1 "); khex(a1); kput(" task "); khex((ULONG)t); kput(t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?");
        kput("\n rets:");
        for (i = 0; i < 400 && k < 10 && (APTR)(sp + i) < t->tc_SPUpper; i++) if (sp[i] < 0xF80000 && isret(sp[i])) { khex(sp[i]); k++; }
        kput("\n code at the first three:");
        { int j; for (j = 0; j < 3 && j < k; j++) { ULONG r = 0; int q = 0; kput(" |"); /* 12 bytes before each return address */
            for (i = 0; i < 400 && (APTR)(sp + i) < t->tc_SPUpper; i++) if (sp[i] < 0xF80000 && isret(sp[i])) { if (q++ == j) { r = sp[i]; break; } }
            if (r) { int b; for (b = -12; b < 0; b++) { ULONG v = *(UBYTE *)(r + b); char h[3]; h[0] = "0123456789abcdef"[v >> 4]; h[1] = "0123456789abcdef"[v & 15]; h[2] = 0; kput(h); } } } }
        kput("\n hunks:\n");
        if (t->tc_Node.ln_Type == NT_PROCESS && pr->pr_CLI) {
            struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(pr->pr_CLI);
            if (cli->cli_Module) ranges(cli->cli_Module, "program");
        }
        lb = (struct Library *)FindName(&SysBase->LibList, "muimaster.library");
        if (lb) ranges(*(BPTR *)((UBYTE *)lb + 38), "muimaster");
    }
}

int main(void)
{
    static void (*stubs[19])(void) = { gw_stub0, gw_stub1, gw_stub2, gw_stub3, gw_stub4, gw_stub5, gw_stub6, gw_stub7, gw_stub8, gw_stub9, gw_stub10, gw_stub11, gw_stub12, gw_stub13, gw_stub14, gw_stub15, gw_stub16, gw_stub17, gw_stub18 };
    int i;
    Forbid();
    for (i = 0; i < 19; i++) gw_old[i] = SetFunction((struct Library *)GfxBase, lvos[i], (APTR)stubs[i]);
    Permit();
    kput("gfxwatch: on\n");
    Wait(SIGBREAKF_CTRL_C);
    Forbid();
    for (i = 0; i < 19; i++) SetFunction((struct Library *)GfxBase, lvos[i], gw_old[i]);
    Permit();
    Delay(10);
    return 0;
}
