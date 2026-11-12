/* failn - fault injection: the Nth resource call OpenMUI makes fails (OpenMUI, stability round 1).
   Build: m68k-amigaos-gcc -Os -m68000 -noixemul -s -o failn failn.c failn.s callseg.s
   failn run PROG N [MASK]   : patch the hooked functions (below), run PROG in this process and
                               make its Nth counted call return the function's failure value
                               (NULL, or -1 for AllocSignal). A call counts when it is made on this
                               task and the first return address on the stack that lies in RAM
                               (not in ROM) is not in PROG's own code: that is, the call comes from
                               a library (OpenMUI, its modules and classes, a disk-loaded OS library)
                               or from ROM code a library called. N = 0 fails nothing and prints
                               the count: the sweep is 1..count. MASK (hex) chooses the hooked
                               functions (default all).
   failn read                : print the record of the last failed call (chip RAM at 0x1B8000, kept
                               across a reset) when the machine rebooted before PROG could report.
   The report (stdout) names the function, the counted N, the direct return address, the first
   three library return addresses found on the stack, and muimaster.library's code hunks, so that
   an address inside the library resolves to a function through the build's symbols. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/execbase.h>
#include <exec/memory.h>
#include <string.h>
#include <stdlib.h>

struct hook { const char *lib; WORD lvo; LONG fail; const char *name; };
static const struct hook hooks[] = {
    { "exec.library",      -198,  0, "AllocMem" },
    { "exec.library",      -684,  0, "AllocVec" },
    { "exec.library",      -708,  0, "AllocPooled" },
    { "exec.library",      -696,  0, "CreatePool" },
    { "exec.library",      -666,  0, "CreateMsgPort" },
    { "exec.library",      -330, -1, "AllocSignal" },
    { "exec.library",      -552,  0, "OpenLibrary" },
    { "graphics.library",  -918,  0, "AllocBitMap" },
    { "graphics.library",  -492,  0, "AllocRaster" },
    { "graphics.library",   -72,  0, "OpenFont" },
    { "graphics.library",  -516,  0, "NewRegion" },
    { "intuition.library", -606,  0, "OpenWindowTagList" },
    { "intuition.library", -204,  0, "OpenWindow" },
    { "intuition.library", -636,  0, "NewObjectA" },
    { "intuition.library", -678,  0, "MakeClass" },
    { "dos.library",       -150,  0, "LoadSeg" },
};
#define NHOOK (sizeof(hooks) / sizeof(hooks[0]))

extern ULONG fn_orig[20], fn_fail[20], fn_stubs[20];
void callseg(void *entry);

static struct Task *target;
static ULONG plo[16], phi[16]; static int np;
static ULONG mlo[16], mhi[16]; static int nm;
static ULONG counts[20], total, failat;
static ULONG failed_idx = ~0UL, failed_ret, failed_a[3];
static char failed_name[32];
static int done;

#define REC ((ULONG *)0x1B8000)
#define RECMAGIC 0xFA11A000
static int rec_ok;

static void ranges(BPTR seg, ULONG *lo, ULONG *hi, int *n)
{
    *n = 0;
    while (seg && *n < 16) {
        ULONG *s = (ULONG *)BADDR(seg);
        lo[*n] = (ULONG)s + 4; hi[*n] = (ULONG)s - 4 + s[-1]; (*n)++;
        seg = (BPTR)s[0];
    }
}
static int inr(ULONG v, ULONG *lo, ULONG *hi, int n)
{ int i; for (i = 0; i < n; i++) if (v >= lo[i] && v < hi[i]) return 1; return 0; }
static int inram(ULONG v)
{
    struct MemHeader *mh;
    if (v >= 0xF80000 && v < 0x1000000) return 0;     /* ROM */
    if (v & 1) return 0;
    for (mh = (struct MemHeader *)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ;
         mh = (struct MemHeader *)mh->mh_Node.ln_Succ)
        if (v >= (ULONG)mh->mh_Lower + 8 && v < (ULONG)mh->mh_Upper) return 1;
    return 0;
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
/* the end of the stack sp is on: the task's, or the end of the memory sp is in */
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

/* called by the stubs: 1 = fail this call */
LONG fn_hook(ULONG idx, ULONG *saved)
{
    ULONG ret = saved[5], *sp = saved + 6, *up, i, first = 0, a[3]; int k = 0;
    if (done || SysBase->ThisTask != target || idx >= NHOOK) return 0;
    if (inr(ret, plo, phi, np)) return 0;                  /* the program's own call */
    /* OpenLibrary from ROM code (dos.library opens one inside Open(): N=1 of every MUI-Demo
       sweep reset the machine) is the ROM's own, not a library's: not counted */
    if (hooks[idx].lvo == -552 && !inram(ret)) return 0;
    if (inram(ret)) first = ret;
    else {
        /* ROM code called by whom? the first RAM return address on the stack */
        up = stackend(sp);
        for (i = 0; sp + i < up && i < 512; i++)
            if (inram(sp[i]) && isret(sp[i])) { first = sp[i]; break; }
        if (!first) return 0;
        if (inr(first, plo, phi, np)) return 0;
    }
    counts[idx]++;
    if (++total != failat) return 0;
    /* the one that fails: record who */
    a[0] = a[1] = a[2] = 0;
    up = stackend(sp);
    if (inram(ret)) a[k++] = ret;
    for (i = 0; sp + i < up && i < 1024 && k < 3; i++)
        if (inram(sp[i]) && !inr(sp[i], plo, phi, np) && isret(sp[i]) && sp[i] != a[0]) a[k++] = sp[i];
    failed_idx = idx; failed_ret = ret; failed_a[0] = a[0]; failed_a[1] = a[1]; failed_a[2] = a[2];
    /* the name asked for: OpenLibrary's (a1), OpenFont's (a0 = TextAttr, its ta_Name) */
    failed_name[0] = 0;
    {
        const char *n = 0;
        if (!strcmp(hooks[idx].name, "OpenLibrary")) n = (const char *)saved[3];
        else if (!strcmp(hooks[idx].name, "OpenFont") && saved[2]) n = *(const char **)saved[2];
        if (n && !((ULONG)n & 1) && (inram((ULONG)n) || ((ULONG)n >= 0xF80000 && (ULONG)n < 0x1000000))) { strncpy(failed_name, n, 31); failed_name[31] = 0; }
    }
    if (rec_ok) {
        REC[1] = failat; REC[2] = idx; REC[3] = ret; REC[4] = a[0]; REC[5] = a[1]; REC[6] = a[2];
        REC[7] = nm ? mlo[0] : 0; REC[8] = nm ? mhi[0] : 0; REC[9] = nm > 1 ? mlo[1] : 0; REC[10] = nm > 1 ? mhi[1] : 0;
        CopyMem(failed_name, &REC[11], 32);
        REC[0] = RECMAGIC;
    }
    return 1;
}

static void report_rec(ULONG *r)
{
    Printf("failn: failed N=%ld %s ret=%08lx callers %08lx %08lx %08lx muimaster %08lx-%08lx %08lx-%08lx",
           r[1], r[2] < NHOOK ? (ULONG)hooks[r[2]].name : (ULONG)"?", r[3], r[4], r[5], r[6], r[7], r[8], r[9], r[10]);
    if (((char *)&r[11])[0]) Printf(" name=%s", (ULONG)&r[11]);
    PutStr("\n");
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "read")) {
        ULONG *r = REC;
        if (r[0] == RECMAGIC) { report_rec(r); r[0] = 0; }
        else PutStr("failn: no record\n");
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "run")) {
        struct Library *mb, *bases[NHOOK]; BPTR seg; ULONG i, mask = ~0UL; int j;
        failat = atoi(argv[3]);
        if (argc > 4) mask = strtoul(argv[4], 0, 16);
        if (!(seg = LoadSeg(argv[2]))) { Printf("failn: cannot load %s\n", (ULONG)argv[2]); return 20; }
        ranges(seg, plo, phi, &np);
        mb = OpenLibrary("muimaster.library", 0);
        if (mb) { struct Library *lb; Forbid();
            lb = (struct Library *)FindName(&SysBase->LibList, "muimaster.library");
            Permit(); if (lb) mb = lb; }
        if (mb && (strstr((char *)mb->lib_IdString, "OpenMUI") || strstr((char *)mb->lib_IdString, "Zune68")))
            ranges(*(BPTR *)((UBYTE *)mb + 38), mlo, mhi, &nm);   /* MUI 3.8: no ranges, a black box */
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
        rec_ok = AllocAbs(128, REC) != NULL;
        if (rec_ok) REC[0] = 0;
        target = FindTask(NULL);
        for (i = 0; i < NHOOK; i++) {
            bases[i] = 0;
            if (!(mask & (1UL << i))) continue;
            bases[i] = OpenLibrary(hooks[i].lib, 0);
            if (!bases[i]) continue;
            fn_fail[i] = hooks[i].fail;
        }
        Forbid();
        for (i = 0; i < NHOOK; i++)
            if (bases[i]) fn_orig[i] = (ULONG)SetFunction(bases[i], hooks[i].lvo, (APTR)fn_stubs[i]);
        Permit();
        callseg((void *)(((ULONG)BADDR(seg)) + 4));
        done = 1;
        Forbid();
        for (i = 0; i < NHOOK; i++)
            if (bases[i]) SetFunction(bases[i], hooks[i].lvo, (APTR)fn_orig[i]);
        Permit();
        UnLoadSeg(seg);
        for (i = 0; i < NHOOK; i++) if (bases[i]) CloseLibrary(bases[i]);
        if (mb) CloseLibrary(mb);
        Printf("failn: counted %ld", total);
        for (j = 0; j < (int)NHOOK; j++) if (counts[j]) Printf(" %s=%ld", (ULONG)hooks[j].name, counts[j]);
        PutStr("\n");
        if (failed_idx != ~0UL) {
            ULONG r[19] = { RECMAGIC, failat, failed_idx, failed_ret, failed_a[0], failed_a[1], failed_a[2],
                            nm ? mlo[0] : 0, nm ? mhi[0] : 0, nm > 1 ? mlo[1] : 0, nm > 1 ? mhi[1] : 0 };
            CopyMem(failed_name, &r[11], 32);
            report_rec(r);
        } else if (failat) Printf("failn: N=%ld never reached\n", failat);
        if (rec_ok) { REC[0] = 0; FreeMem(REC, 128); }
        return 0;
    }
    PutStr("failn run PROG N [MASK] | failn read\n");
    return 5;
}
