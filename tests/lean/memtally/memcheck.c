/* memcheck: patch exec AllocMem/FreeMem for every task and check every memory header's
   free-chunk list before and after each call. The first damage found is reported through
   RawPutChar (wasabi debug) and written into chip RAM at 0x1A0000 (survives the reset the
   damage usually ends in): the call, the task, its return addresses, the last 24 calls
   (op, address, size, task name, 3 return addresses) and the frees whose block covers the
   damaged chunk (a use-after-free's owner), from a ring of the last 2048 frees.
     memcheck        run in the background (Ctrl-C stops it)
     memcheck read   print the record after the reboot
   Build: m68k-amigaos-gcc -Os -m68000 -noixemul -s -o memcheck memcheck.c patch.s
   (patch.s from tests/lean/memtally: it calls mt_alloc(size, flags, sp) and
   mt_free(block, size, sp)). */
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <string.h>

extern APTR oldAllocMem, oldFreeMem;
APTR callAllocMem(ULONG size, ULONG flags);
void callFreeMem(APTR block, ULONG size);
void newAllocMem(void), newFreeMem(void);

#define RECADDR 0x1A0000
#define MAGIC 0x4D434B32
#define NCALL 16
#define NFREE 1024
#define NRANGE 24
struct call { ULONG op, a, b, task, ret[8]; char name[20]; ULONG n; };
struct rec {
    ULONG magic, damaged, code, where, what, ncalls, op, a, b, task; ULONG ret[8]; char name[32];
    ULONG when;                     /* 1 before alloc, 2 after alloc, 3 before free, 4 after free */
    struct call calls[NCALL]; ULONG ncall;
    struct call frees[NFREE]; ULONG nfree;
    struct call allocs[NFREE]; ULONG nalloc;
    ULONG rlo[NRANGE], rhi[NRANGE]; char rname[NRANGE][12]; ULONG nrange;
    ULONG dump[32]; ULONG zrun;
};
static struct rec *R;
static volatile int stopped;

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
/* the first n return addresses on the stack, ROM ones last (a library call inside ROM
   shows who made it) */
static void rets(ULONG *sp, ULONG *out, int n)
{
    struct Task *t = SysBase->ThisTask; ULONG i; int k = 0;
    for (i = 0; i < n; i++) out[i] = 0;
    for (i = 0; i < 400 && k < n && (APTR)(sp + i) < t->tc_SPUpper; i++)
        if (sp[i] < 0xF80000 && isret(sp[i])) out[k++] = sp[i];
    for (i = 0; i < 400 && k < n && (APTR)(sp + i) < t->tc_SPUpper; i++)
        if (sp[i] >= 0xF80000 && isret(sp[i])) out[k++] = sp[i];
}
static void tname(char *dst, int n)
{
    struct Task *t = SysBase->ThisTask; const char *s = t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?"; int i;
    for (i = 0; i < n - 1 && s[i]; i++) dst[i] = s[i];
    dst[i] = 0;
}

static int check(ULONG *where, ULONG *what)
{
    struct MemHeader *mh; struct MemChunk *mc, *prev;
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ) {
        ULONG sum = 0; int n = 0;
        for (prev = NULL, mc = mh->mh_First; mc; prev = mc, mc = mc->mc_Next) {
            if ((ULONG)mc < (ULONG)mh->mh_Lower || (ULONG)mc >= (ULONG)mh->mh_Upper || ((ULONG)mc & 7))
                { *where = (ULONG)mc; *what = (ULONG)prev; return 1; }
            if (prev && (ULONG)prev + prev->mc_Bytes > (ULONG)mc)
                { *where = (ULONG)mc; *what = (ULONG)prev; return 2; }
            if ((mc->mc_Bytes & 7) || mc->mc_Bytes == 0 || (ULONG)mc + mc->mc_Bytes > (ULONG)mh->mh_Upper)
                { *where = (ULONG)mc; *what = mc->mc_Bytes; return 3; }
            sum += mc->mc_Bytes;
            if (++n > 100000) { *where = (ULONG)mc; *what = n; return 2; }
        }
        if (sum != mh->mh_Free) { *where = (ULONG)mh; *what = sum; return 4; }
    }
    return 0;
}

static void record(ULONG op, ULONG a, ULONG b, ULONG *sp)
{
    struct call *c = &R->calls[R->ncall++ % NCALL];
    c->op = op; c->a = a; c->b = b; c->task = (ULONG)SysBase->ThisTask; c->n = R->ncalls;
    rets(sp, c->ret, 8); tname(c->name, sizeof c->name);
    if (op == 2) { struct call *f = &R->frees[R->nfree++ % NFREE]; *f = *c; }
    else { struct call *f = &R->allocs[R->nalloc++ % NFREE]; *f = *c; f->a = b; f->b = a; }
}

static void ranges(BPTR seg, const char *name)
{
    while (seg && R->nrange < NRANGE) {
        ULONG *p = (ULONG *)BADDR(seg); int i;
        R->rlo[R->nrange] = (ULONG)(p + 1); R->rhi[R->nrange] = (ULONG)p + p[-1] - 4;
        for (i = 0; i < 11 && name[i]; i++) R->rname[R->nrange][i] = name[i];
        R->rname[R->nrange][i] = 0;
        R->nrange++; seg = p[0];
    }
}
static void damage(ULONG when, ULONG a, ULONG b, ULONG *sp, int code, ULONG where, ULONG what)
{
    int i;
    stopped = 1;
    R->damaged = 1; R->code = code; R->where = where; R->what = what; R->when = when;
    R->a = a; R->b = b; R->task = (ULONG)SysBase->ThisTask; rets(sp, R->ret, 8); tname(R->name, sizeof R->name);
    {
        struct Process *pr = (struct Process *)SysBase->ThisTask;
        if (pr->pr_Task.tc_Node.ln_Type == NT_PROCESS && pr->pr_CLI) {
            struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(pr->pr_CLI);
            if (cli->cli_Module) ranges(cli->cli_Module, "program");
        }
        {
            struct Library *lb = (struct Library *)FindName(&SysBase->LibList, "muimaster.library");
            if (lb) ranges(*(BPTR *)((UBYTE *)lb + 38), "muimaster");
        }
    }
    { ULONG k; UBYTE *q = (UBYTE *)(where & ~15) - 16; for (k = 0; k < 32; k++) R->dump[k] = ((ULONG *)q)[k];
      q = (UBYTE *)where; for (k = 0; k < 0x10000 && q[k] == 0; k++) ; R->zrun = k; }
    kput("memcheck: INSANE when "); khex(when); kput("code "); khex(code); khex(where); khex(what);
    kput("\n call: "); khex(a); khex(b); kput(R->name); kput("\n rets:");
    for (i = 0; i < 8; i++) khex(R->ret[i]);
    kput("\n");
}

APTR mt_alloc(ULONG size, ULONG flags, ULONG *sp)
{
    ULONG where, what; int c; APTR p;
    if (!stopped) { Forbid(); R->ncalls++; c = check(&where, &what); if (c) damage(1, size, flags, sp, c, where, what); Permit(); }
    p = callAllocMem(size, flags);
    if (!stopped) { Forbid(); record(1, size, (ULONG)p, sp); c = check(&where, &what); if (c) damage(2, size, (ULONG)p, sp, c, where, what); Permit(); }
    return p;
}

void mt_free(ULONG addr, ULONG size, ULONG *sp)
{
    ULONG where, what; int c;
    if (!stopped) { Forbid(); R->ncalls++; c = check(&where, &what); if (c) damage(3, addr, size, sp, c, where, what); Permit(); }
    callFreeMem((APTR)addr, size);
    if (!stopped && addr < 0x200000 && size >= 0x1000) { Forbid(); memset((APTR)(addr + 8), 0xDB, size - 8); Permit(); }
    if (!stopped) { Forbid(); record(2, addr, size, sp); c = check(&where, &what); if (c) damage(4, addr, size, sp, c, where, what); Permit(); }
}

static void pcall(const char *pre, struct call *c)
{
    int i;
    Printf("%s%5ld %s %08lx %08lx %-16s", (ULONG)pre, c->n, (ULONG)(c->op == 1 ? "alloc" : "free "), c->a, c->b, (ULONG)c->name);
    for (i = 0; i < 8 && c->ret[i]; i++) Printf(" %08lx", c->ret[i]);
    PutStr("\n");
}

#include <dos/dosextens.h>
int main(int argc, char **argv)
{
    static const char *whens[] = { "?", "before alloc", "after alloc", "before free", "after free" };
    R = (struct rec *)RECADDR;
    if (argc > 1 && !strcmp(argv[1], "read")) {
        ULONG i, n;
        if (R->magic != MAGIC) { PutStr("memcheck: no record\n"); return 5; }
        if (!R->damaged) { Printf("memcheck: no damage seen in %ld calls\n", R->ncalls); return 0; }
        Printf("memcheck: damage %s (code %ld: 1 chunk outside the header, 2 overlap, 3 bad size, 4 free sum) at %08lx (%08lx), call %ld\n",
            (ULONG)whens[R->when < 5 ? R->when : 0], R->code, R->where, R->what, R->ncalls);
        Printf(" the call: %08lx %08lx by task %08lx %s\n rets:", R->a, R->b, R->task, (ULONG)R->name);
        for (i = 0; i < 8; i++) Printf(" %08lx", R->ret[i]);
        PutStr("\n the calls before it (oldest first):\n");
        n = R->ncall < NCALL ? R->ncall : NCALL;
        for (i = 0; i < n; i++) pcall("  ", &R->calls[(R->ncall - n + i) % NCALL]);
        Printf(" memory from %08lx (%ld zero bytes from the chunk on):\n", (R->where & ~15) - 16, R->zrun);
        for (i = 0; i < 32; i++) Printf("%s%08lx%s", (ULONG)(i % 8 ? " " : "  "), R->dump[i], (ULONG)(i % 8 == 7 ? "\n" : ""));
        PutStr(" code ranges (hunks):\n");
        for (i = 0; i < R->nrange; i++) Printf("  %08lx-%08lx %s\n", R->rlo[i], R->rhi[i], (ULONG)R->rname[i]);
        PutStr(" allocations whose block covers the damaged chunk (oldest first):\n");
        n = R->nalloc < NFREE ? R->nalloc : NFREE;
        for (i = 0; i < n; i++) {
            struct call *f = &R->allocs[(R->nalloc - n + i) % NFREE];
            if (R->where >= f->a && R->where < f->a + f->b) pcall("  ", f);
        }
        PutStr(" frees whose block covers the damaged chunk (oldest first):\n");
        n = R->nfree < NFREE ? R->nfree : NFREE;
        for (i = 0; i < n; i++) {
            struct call *f = &R->frees[(R->nfree - n + i) % NFREE];
            if (R->where >= f->a && R->where < f->a + f->b) pcall("  ", f);
        }
        return 0;
    }
    if (!AllocAbs(sizeof(struct rec), (APTR)RECADDR)) { PutStr("memcheck: chip RAM at 0x1A0000 not free\n"); return 10; }
    memset(R, 0, sizeof *R); R->magic = MAGIC;

    Forbid();
    oldAllocMem = SetFunction(SysBase, -198, (APTR)newAllocMem);
    oldFreeMem = SetFunction(SysBase, -210, (APTR)newFreeMem);
    Permit();
    kput("memcheck: on\n");
    Wait(SIGBREAKF_CTRL_C);
    stopped = 1;
    Forbid();
    SetFunction(SysBase, -198, oldAllocMem);
    SetFunction(SysBase, -210, oldFreeMem);
    Permit();
    Delay(10);
    kput("memcheck: off\n");
    return 0;
}
