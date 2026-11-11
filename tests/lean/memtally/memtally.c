/* memtally - who holds a program's memory (from Zune68k; knows Zune68's and Zune68k's library base).
   Build: m68k-amigaos-gcc -Os -m68000 -noixemul -s -o memtally memtally.c patch.s callseg.s
   memtally run PROG [STACK] : patch AllocMem/FreeMem, run PROG in this process; every live
                               block allocated by this task is kept with up to 3 return
                               addresses found in muimaster's code (seglist in the base:
                               Zune68 offset 38, Zune68k 42) or the program.
   memtally runall PROG      : as run, every task's blocks (callers: any code in RAM, not ROM,
                               so that a library call inside ROM shows who made it).
   memtally dump FILE        : write the live blocks of a running "memtally run" to FILE:
                               "size direct a1 a2 a3 task addr" (hex) per line, plus the
                               ranges (M muimaster, P program), "L lo hi name" (each library's
                               and device's code: its jump table's targets around their
                               median) and "K task name". Read with tools/lean/mtlib.py.
   The callers are searched on the stack the allocation runs on: the task's, or (Zune runs
   programs with small stacks on one of its own) up to the end of the memory it is in.
   A second FreeMem of a block this task allocated and already freed (and not allocated
   again since) is reported through RawPutChar (wasabi debug) with both callers' return
   addresses, before exec's own alert.
   memtally guard PROG [STACK] : as run, and every block this task allocates gets 16 guard
                               bytes before and after; damaged guards are reported when the
                               block is freed (by any task), with the allocation's callers. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/execbase.h>
#include <exec/semaphores.h>
#include <exec/memory.h>
#include <string.h>
#include <stdlib.h>

#define NENT 8192
struct ent { ULONG addr, size, direct, a[3]; ULONG pad, task; };
struct mt {
    struct SignalSemaphore sem;
    struct Task *task;
    int all;                          /* lib/runall: every task's blocks */
    struct ent *tab;
    ULONG nlive, nover;
    ULONG mlo[16], mhi[16]; int nm;   /* muimaster hunks */
    ULONG plo[16], phi[16]; int np;   /* program hunks */
};
static struct mt *M;
static int inr(ULONG v, ULONG *lo, ULONG *hi, int n)
{ int i; for (i = 0; i < n; i++) if (v >= lo[i] && v < hi[i]) return 1; return 0; }

/* a code address in a memory header's range or in ROM (lib/runall mode,
   where the callers outside muimaster and the program count too) */
static int inram(ULONG v)
{
    struct MemHeader *mh;
    if (v >= 0xF80000 && v < 0x1000000) return 1;
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ)
        if (v >= (ULONG)mh->mh_Lower + 8 && v < (ULONG)mh->mh_Upper) return 1;
    return 0;
}

/* the end of the stack sp is on: the task's, or (on a stack of its own,
   which Zune switches to for a program with a small one) the end of the
   memory sp is in */
static ULONG *stackend(ULONG *sp)
{
    struct Task *t = SysBase->ThisTask;
    struct MemHeader *mh;
    if ((APTR)sp >= t->tc_SPLower && (APTR)sp < t->tc_SPUpper) return (ULONG *)t->tc_SPUpper;
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ)
        if ((APTR)sp >= mh->mh_Lower && (APTR)sp < mh->mh_Upper) return (ULONG *)mh->mh_Upper;
    return sp;
}

static int isret(ULONG v)
{
    UWORD w2, w4, w6;
    if (v & 1) return 0;
    w2 = *(UWORD *)(v - 2); w4 = *(UWORD *)(v - 4); w6 = *(UWORD *)(v - 6);
    if ((w2 & 0xFFF8) == 0x4E90) return 1;                 /* jsr (an) */
    if ((w4 & 0xFFF8) == 0x4EA8 || (w4 & 0xFFF8) == 0x4EB0 || w4 == 0x4EB8 || w4 == 0x4EBA || w4 == 0x4EBB) return 1;
    if (w4 == 0x6100) return 1;                            /* bsr.w */
    if ((w2 & 0xFF00) == 0x6100 && (w2 & 0xFF) != 0 && (w2 & 0xFF) != 0xFF) return 1;
    if (w6 == 0x4EB9 || w6 == 0x61FF) return 1;
    return 0;
}

#define NFREED 512
static struct ent freed[NFREED]; static int nfreed;

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
static void callers(ULONG *sp, ULONG *a)
{
    struct mt *m = M; ULONG i, *up = stackend(sp); int k = 0;
    a[0] = a[1] = a[2] = 0;
    for (i = 0; sp + i < up && i < 2048 && k < 3; i++) {
        ULONG v = sp[i];
        if ((inr(v, m->mlo, m->mhi, m->nm) || inr(v, m->plo, m->phi, m->np)) && isret(v)) a[k++] = v;
    }
}
extern APTR oldAllocMem, oldFreeMem;
APTR callAllocMem(ULONG size, ULONG flags);
void callFreeMem(APTR block, ULONG size);
static int guard;
#define GUARD 16
#define GBYTE 0xAB
void newAllocMem(void), newFreeMem(void);
void callseg(void *entry);

static void mt_record(ULONG addr, ULONG size, ULONG *sp, ULONG pad)
{
    struct mt *m = M; struct Task *t = SysBase->ThisTask; ULONG h, i, *up; int k = 0;
    struct ent e;
    if (!m || (!m->all && t != m->task)) return;
    e.addr = addr; e.size = size; e.direct = sp[0]; e.a[0] = e.a[1] = e.a[2] = 0; e.pad = pad;
    e.task = (ULONG)t;
    up = stackend(sp);
    for (i = 0; sp + i < up && i < 2048 && k < 3; i++) {
        ULONG v = sp[i];
        if ((inr(v, m->mlo, m->mhi, m->nm) || inr(v, m->plo, m->phi, m->np)
             || (m->all && v < 0xF80000 && inram(v))) && isret(v)) e.a[k++] = v;
    }
    Forbid();
    for (i = 0; i < NFREED; i++) if (freed[i].addr == addr) freed[i].addr = 0;
    h = (addr >> 3) & (NENT - 1);
    for (i = 0; i < NENT; i++, h = (h + 1) & (NENT - 1))
        if (m->tab[h].addr == 0 || m->tab[h].addr == 1) { m->tab[h] = e; m->nlive++; break; }
    if (i == NENT) m->nover++;
    Permit();
}

static ULONG mt_unrecord(ULONG addr, ULONG *sp, struct ent *out)
{
    struct mt *m = M; ULONG h, i, a[3];
    out->pad = 0;
    if (!m || !addr) return 0;
    Forbid();
    h = (addr >> 3) & (NENT - 1);
    for (i = 0; i < NENT; i++, h = (h + 1) & (NENT - 1)) {
        if (m->tab[h].addr == 0) break;
        if (m->tab[h].addr == addr) {
            *out = m->tab[h];
            m->tab[h].addr = 1; m->nlive--;
            if (SysBase->ThisTask == m->task) {
                struct ent *f = &freed[nfreed++ & (NFREED - 1)];
                *f = m->tab[h]; f->addr = addr; f->direct = sp[0];
                callers(sp, a); f->a[0] = a[0]; f->a[1] = a[1]; f->a[2] = a[2];
            }
            Permit();
            return 1;
        }
    }
    if (guard && SysBase->ThisTask == m->task) {
        /* Zune68k: guard mode logs frees of blocks it did not see allocated */
        callers(sp, a);
        kput("memtally: foreign free "); khex(addr); khex(sp[0]); khex(a[0]); khex(a[1]); khex(a[2]); kput("\n");
    }
    if (SysBase->ThisTask == m->task)
        for (i = 0; i < NFREED; i++)
            if (freed[i].addr == addr) {
                kput("memtally: double FreeMem "); khex(addr); khex(freed[i].size);
                kput("\nfirst free: "); khex(freed[i].direct); khex(freed[i].a[0]); khex(freed[i].a[1]); khex(freed[i].a[2]);
                callers(sp, a);
                kput("\nsecond free: "); khex(sp[0]); khex(a[0]); khex(a[1]); khex(a[2]);
                kput("\nranges M "); khex(m->mlo[0]); kput("P "); khex(m->plo[0]); kput("\n");
                break;
            }
    Permit();
    return 0;
}

APTR mt_alloc(ULONG size, ULONG flags, ULONG *sp)
{
    struct mt *m = M; UBYTE *p;
    if (m && guard && SysBase->ThisTask == m->task && m->nlive < NENT * 3 / 4) {
        int i;
        if (!(p = callAllocMem(size + 2 * GUARD, flags))) return 0;
        for (i = 0; i < GUARD; i++) p[i] = p[GUARD + size + i] = GBYTE;
        p += GUARD;
        mt_record((ULONG)p, size, sp, 1);
        return p;
    }
    p = callAllocMem(size, flags);
    if (p) mt_record((ULONG)p, size, sp, 0);
    return p;
}

/* Zune68k: does [addr, addr+size) overlap memory that is already free? */
static int overlaps_free(ULONG addr, ULONG size)
{
    struct MemHeader *mh; struct MemChunk *mc; int r = 0;
    ULONG end = addr + ((size + 7) & ~7);
    Forbid();
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ) {
        if (addr < (ULONG)mh->mh_Lower || addr >= (ULONG)mh->mh_Upper) continue;
        for (mc = mh->mh_First; mc; mc = mc->mc_Next)
            if ((ULONG)mc < end && (ULONG)mc + mc->mc_Bytes > (addr & ~7)) { r = 1; break; }
        break;
    }
    Permit();
    return r;
}

/* Zune68k: quarantine for use-after-free: freed blocks are filled with 0xDE and
 * held back; when one is released (ring full, or at exit) it is checked. */
#define NQ 96
static struct { ULONG addr, size, real, realsize, a[4]; } q[NQ]; static int nq;
static void q_check(int i)
{
    UBYTE *p = (UBYTE *)q[i].addr; ULONG k;
    for (k = 0; k < q[i].size; k++)
        if (p[k] != 0xDE) {
            kput("memtally: written after free "); khex(q[i].addr); khex(q[i].size); khex(k); khex(p[k]);
            kput("\n freed by: "); khex(q[i].a[0]); khex(q[i].a[1]); khex(q[i].a[2]); khex(q[i].a[3]); kput("\n");
            break;
        }
    callFreeMem((APTR)q[i].real, q[i].realsize);
    q[i].addr = 0;
}
static void q_put(ULONG addr, ULONG size, ULONG real, ULONG realsize, ULONG *sp)
{
    int i = nq++ % NQ; ULONG a[3], k;
    if (q[i].addr) q_check(i);
    callers(sp, a);
    for (k = 0; k < size; k++) ((UBYTE *)addr)[k] = 0xDE;
    q[i].addr = addr; q[i].size = size; q[i].real = real; q[i].realsize = realsize;
    q[i].a[0] = sp[0]; q[i].a[1] = a[0]; q[i].a[2] = a[1]; q[i].a[3] = a[2];
}
void q_flush(void)
{
    int i;
    for (i = 0; i < NQ; i++) if (q[i].addr) q_check(i);
}

void mt_free(ULONG addr, ULONG size, ULONG *sp)
{
    struct ent e;
    if (guard && addr && overlaps_free(addr, size)) {
        ULONG a[3];
        callers(sp, a);
        kput("memtally: FreeMem of free memory (skipped) "); khex(addr); khex(size);
        khex((ULONG)SysBase->ThisTask); khex(sp[0]); khex(a[0]); khex(a[1]); khex(a[2]); kput("\n");
        return;
    }
    if (mt_unrecord(addr, sp, &e) && e.pad) {
        UBYTE *p = (UBYTE *)addr; int i, bad = 0;
        for (i = 0; i < GUARD; i++) if (p[-1 - i] != GBYTE) bad |= 1;
        for (i = 0; i < GUARD; i++) if (p[e.size + i] != GBYTE) bad |= 2;
        if (size != e.size) bad |= 4;
        if (bad) {
            kput("memtally: guard damaged "); khex(bad); khex(addr); khex(e.size); khex(size);
            kput("\n before: "); for (i = GUARD; i > 0; i--) { khex(p[-i]); }
            kput("\n after: "); for (i = 0; i < GUARD; i++) { khex(p[e.size + i]); }
            kput("\n alloc: "); khex(e.direct); khex(e.a[0]); khex(e.a[1]); khex(e.a[2]);
            kput("\n ranges M "); khex(M->mlo[0]); kput("P "); khex(M->plo[0]); kput("\n");
        }
        if (SysBase->ThisTask == M->task) { Forbid(); q_put(addr, e.size, addr - GUARD, e.size + 2 * GUARD, sp); Permit(); }
        else callFreeMem((APTR)(addr - GUARD), e.size + 2 * GUARD);
        return;
    }
    if (guard && M && SysBase->ThisTask == M->task && addr && size >= 8) {
        Forbid(); q_put(addr, size, addr, size, sp); Permit();
        return;
    }
    callFreeMem((APTR)addr, size);
}

static void ranges(BPTR seg, ULONG *lo, ULONG *hi, int *n)
{
    *n = 0;
    while (seg && *n < 16) { ULONG *p = BADDR(seg); lo[*n] = (ULONG)(p + 1); hi[*n] = (ULONG)p + p[-1] - 4; (*n)++; seg = p[0]; }
}

/* text built under Forbid (dump) */
#define TXTMAX 65536
static char *tp, *te;
static void tadd(const char *t) { while (*t && tp < te) *tp++ = *t++; }
static void thex(ULONG v)
{
    char b[9]; int i = 8; b[8] = 0;
    do { b[--i] = "0123456789abcdef"[v & 15]; v >>= 4; } while (v && i);
    tadd(b + i);
}

int main(int argc, char **argv)
{
    if (argc >= 3 && !strcmp(argv[1], "dump")) {
        /* Everything is copied under Forbid first and written after it:
           writing a file waits for the file system, which breaks the
           Forbid while the lists are walked */
        struct SignalSemaphore *s; struct mt *m; BPTR f; ULONG i, n = 0, tot = 0, nlive, nover; int j;
        struct ent *snap; char *txt;
        Forbid(); s = FindSemaphore("memtally"); Permit();
        if (!s) { PutStr("no memtally running\n"); return 10; }
        m = (struct mt *)s;
        snap = AllocMem(NENT * sizeof(struct ent), MEMF_ANY);
        txt = AllocMem(TXTMAX, MEMF_ANY);
        if (!snap || !txt) return 20;
        if (!(f = Open(argv[2], MODE_NEWFILE))) return 10;
        tp = txt; te = txt + TXTMAX - 64;
        Forbid();
        for (i = 0; i < NENT; i++) if (m->tab[i].addr > 1) {
            if (m->all && m->tab[i].task == (ULONG)FindTask(NULL)) continue;
            snap[n++] = m->tab[i];
        }
        nlive = m->nlive; nover = m->nover;
        { /* each library's and device's code: the span of its jump table's
             targets around their median (patched vectors elsewhere are
             left out): "L lo hi name", to name callers outside muimaster
             and the program (class libraries, ROM) */
            static ULONG tg[400];
            struct List *ll[2]; struct Node *nd; int k;
            ll[0] = &SysBase->LibList; ll[1] = &SysBase->DeviceList;
            for (k = 0; k < 2; k++)
                for (nd = ll[k]->lh_Head; nd->ln_Succ; nd = nd->ln_Succ) {
                    struct Library *lb = (struct Library *)nd; ULONG lo = ~0UL, hi = 0, med; UWORD o; int c = 0, x, y;
                    for (o = 6; o <= lb->lib_NegSize && c < 400; o += 6) {
                        UWORD *jp = (UWORD *)((UBYTE *)lb - o);
                        if (*jp == 0x4EF9) tg[c++] = *(ULONG *)(jp + 1);
                    }
                    if (!c) continue;
                    for (x = 1; x < c; x++) for (y = x; y > 0 && tg[y - 1] > tg[y]; y--) { ULONG t = tg[y]; tg[y] = tg[y - 1]; tg[y - 1] = t; }
                    med = tg[c / 2];
                    for (x = 0; x < c; x++)
                        if (tg[x] + 0x30000 >= med && tg[x] <= med + 0x30000) { if (tg[x] < lo) lo = tg[x]; if (tg[x] > hi) hi = tg[x]; }
                    tadd("L "); thex(lo); tadd(" "); thex(hi); tadd(" "); tadd(nd->ln_Name ? nd->ln_Name : "?"); tadd("\n");
                }
        }
        if (m->all) { /* names of the tasks that still exist */
            struct Node *nd; struct List *ls[2]; int k;
            ls[0] = &SysBase->TaskReady; ls[1] = &SysBase->TaskWait;
            for (k = 0; k < 2; k++)
                for (nd = ls[k]->lh_Head; nd->ln_Succ; nd = nd->ln_Succ) {
                    tadd("K "); thex((ULONG)nd); tadd(" "); tadd(nd->ln_Name ? nd->ln_Name : "?"); tadd("\n");
                }
        }
        Permit();
        for (j = 0; j < m->nm; j++) FPrintf(f, "M %lx %lx\n", m->mlo[j], m->mhi[j]);
        for (j = 0; j < m->np; j++) FPrintf(f, "P %lx %lx\n", m->plo[j], m->phi[j]);
        for (i = 0; i < n; i++) {
            struct ent *e = &snap[i];
            tot += e->size;
            FPrintf(f, "%lx %lx %lx %lx %lx %lx %lx\n", e->size, e->direct, e->a[0], e->a[1], e->a[2], e->task, e->addr);
        }
        *tp = 0; FPuts(f, txt);
        FPrintf(f, "T %ld %ld %ld\n", tot, nlive, nover);
        Close(f);
        FreeMem(snap, NENT * sizeof(struct ent)); FreeMem(txt, TXTMAX);
        Printf("%ld bytes in %ld blocks (%ld not kept)\n", tot, nlive, nover);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "lib")) {
        /* memtally lib NAME [VERSION]: record every task's blocks while NAME
           is opened, keep it open until Ctrl-C; "dump" shows what it holds */
        struct Library *mb, *lb; struct mt *m;
        m = AllocMem(sizeof(*m), MEMF_PUBLIC | MEMF_CLEAR);
        m->tab = AllocMem(NENT * sizeof(struct ent), MEMF_PUBLIC | MEMF_CLEAR);
        if (!m || !m->tab) return 20;
        m->task = FindTask(NULL); m->all = 1;
        m->sem.ss_Link.ln_Name = "memtally"; m->sem.ss_Link.ln_Pri = 0;
        AddSemaphore(&m->sem);
        Forbid();
        oldAllocMem = SetFunction(SysBase, -198, (APTR)newAllocMem);
        oldFreeMem = SetFunction(SysBase, -210, (APTR)newFreeMem);
        M = m;
        Permit();
        mb = OpenLibrary(argv[2], argc > 3 ? atoi(argv[3]) : 0);
        if (mb) {
            Forbid(); lb = (struct Library *)FindName(&SysBase->LibList, argv[2]); Permit();
            if (lb && lb->lib_IdString && (strstr((char *)lb->lib_IdString, "OpenMUI") || strstr((char *)lb->lib_IdString, "Zune68")))
                ranges(*(BPTR *)((UBYTE *)lb + 38), m->mlo, m->mhi, &m->nm);
        }
        Printf("%s %s\n", argv[2], mb ? "open" : "not opened");
        Wait(SIGBREAKF_CTRL_C);
        Forbid();
        SetFunction(SysBase, -198, oldAllocMem);
        SetFunction(SysBase, -210, oldFreeMem);
        M = 0;
        Permit();
        RemSemaphore(&m->sem);
        if (mb) CloseLibrary(mb);
        return 0;
    }
    if (argc >= 3 && (!strcmp(argv[1], "run") || !strcmp(argv[1], "guard") || !strcmp(argv[1], "runall"))) {
        struct Library *mb; BPTR seg; struct mt *m;
        m = AllocMem(sizeof(*m), MEMF_PUBLIC | MEMF_CLEAR);
        m->tab = AllocMem(NENT * sizeof(struct ent), MEMF_PUBLIC | MEMF_CLEAR);
        if (!m || !m->tab) return 20;
        mb = OpenLibrary("muimaster.library", 0);
        /* Zune68 since phase 8 hands a MUI 3.8 program a copy of the base
           (jump table + struct Library only): the seglist is in the base
           in the library list */
        if (mb) { struct Library *lb; Forbid();
            lb = (struct Library *)FindName(&SysBase->LibList, "muimaster.library");
            Permit(); if (lb) mb = lb; }
        /* Zune68k (the separate port) keeps its seglist at 42; Zune68 and its
           lean edition ("Zune68k lean" since 2026-11-06, "OpenMUI" since 2026-11-11) at 38 */
        if (mb && strstr((char *)mb->lib_IdString, "Zune68k") && !strstr((char *)mb->lib_IdString, "lean")) ranges(*(BPTR *)((UBYTE *)mb + 42), m->mlo, m->mhi, &m->nm);
        else if (mb && (strstr((char *)mb->lib_IdString, "OpenMUI") || strstr((char *)mb->lib_IdString, "Zune68"))) ranges(*(BPTR *)((UBYTE *)mb + 38), m->mlo, m->mhi, &m->nm);
        if (!(seg = LoadSeg(argv[2]))) return 20;
        ranges(seg, m->plo, m->phi, &m->np);
        /* the program's PROGDIR: and name are its own, not memtally's
           (Voyager looks for MIMEPrefs through them) */
        {
            struct Process *pr = (struct Process *)FindTask(NULL);
            char dir[256]; BPTR l;
            strncpy(dir, argv[2], 255); dir[255] = 0;
            *(PathPart(dir)) = 0;
            if (dir[0] && (l = Lock(dir, SHARED_LOCK))) {
                if (pr->pr_HomeDir) UnLock(pr->pr_HomeDir);
                pr->pr_HomeDir = l;
            }
            SetProgramName(argv[2]);
        }
        m->task = FindTask(NULL);
        m->all = !strcmp(argv[1], "runall");
        guard = !strcmp(argv[1], "guard");
        if (guard) {
            int j;
            for (j = 0; j < m->nm; j++) { kput("memtally: M "); khex(m->mlo[j]); khex(m->mhi[j]); kput("\n"); }
            for (j = 0; j < m->np; j++) { kput("memtally: P "); khex(m->plo[j]); khex(m->phi[j]); kput("\n"); }
        }
        m->sem.ss_Link.ln_Name = "memtally"; m->sem.ss_Link.ln_Pri = 0;
        AddSemaphore(&m->sem);
        Forbid();
        oldAllocMem = SetFunction(SysBase, -198, (APTR)newAllocMem);
        oldFreeMem = SetFunction(SysBase, -210, (APTR)newFreeMem);
        M = m;
        Permit();
        callseg((void *)(((ULONG)BADDR(seg)) + 4));
        if (guard) { Forbid(); q_flush(); Permit(); }
        Forbid();
        SetFunction(SysBase, -198, oldAllocMem);
        SetFunction(SysBase, -210, oldFreeMem);
        M = 0;
        Permit();
        RemSemaphore(&m->sem);
        UnLoadSeg(seg);
        if (mb) CloseLibrary(mb);
        Printf("left: %ld blocks\n", m->nlive);
        FreeMem(m->tab, NENT * sizeof(struct ent));
        FreeMem(m, sizeof(*m));
        return 0;
    }
    PutStr("memtally run|guard|runall PROG | memtally lib NAME [VER] | memtally dump FILE\n");
    return 5;
}
