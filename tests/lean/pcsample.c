/* pcsample TASKNAME FILE [SECONDS]: a poor man's profiler.
 *
 * Every tick (Delay(1), 20 ms) at priority 30 it looks at the named task;
 * when the task is ready (preempted, not waiting), its saved PC (exec 40/47,
 * 68020 without FPU: the first long at tc_SPReg) and the next 48 stack longs
 * are kept. At the end (task gone, Ctrl-C or SECONDS) FILE gets:
 *   SEG <name> <hunk> <start> <bytes>   hunks of the task's program,
 *                                       muimaster.library and its class files
 *   LIB <name> <lowest> <highest>       jump-table targets of every library
 *   S <pc> <tick> <stack longs...>      one line per sample (tick: 20 ms each)
 * tests/lean/scripts/pcsample.py maps it to functions with the link maps.
 * Build: m68k-amigaos-gcc -Os -noixemul -o pcsample pcsample.c
 */
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

#define STACKLONGS 48
#define MAXSAMPLES 8000
/* struct MUIMasterBase_intern (muimaster_intern.h): seglist, ClassFiles[] */
#define OFF_SEGLIST 38
#define OFF_CLASSFILES 334
#define NCLASSFILES 48

struct sample { ULONG state; ULONG pc; ULONG stack[STACKLONGS]; };

static BPTR out;

/* A process whose name or CLI command name ends in NAME (Forbid()den) */
static int matches(struct Task *t, const char *name)
{
    LONG ln = strlen(name), l;
    const char *tn = t->tc_Node.ln_Name;
    if (tn && (l = strlen(tn)) >= ln && !stricmp(tn + l - ln, name)) return 1;
    if (t->tc_Node.ln_Type == NT_PROCESS && ((struct Process *)t)->pr_CLI)
    {
        struct CommandLineInterface *cli = BADDR(((struct Process *)t)->pr_CLI);
        UBYTE *b = BADDR(cli->cli_CommandName);
        if (b && cli->cli_Module && b[0] >= ln && !strnicmp((char *)b + 1 + b[0] - ln, name, ln))
            return 1;
    }
    return 0;
}

static struct Task *find(const char *name)
{
    struct Node *n;
    if (matches(SysBase->ThisTask, name)) return SysBase->ThisTask;
    for (n = SysBase->TaskReady.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (matches((struct Task *)n, name)) return (struct Task *)n;
    for (n = SysBase->TaskWait.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (matches((struct Task *)n, name)) return (struct Task *)n;
    return NULL;
}

static void segs(const char *name, BPTR seg)
{
    LONG n = 0;
    while (seg)
    {
        ULONG *h = BADDR(seg);
        FPrintf(out, "SEG %s %ld %08lx %ld\n", (LONG)name, n++, (LONG)(h + 1), h[-1] - 8);
        seg = *h;
    }
}

int main(void)
{
    struct RDArgs *rda;
    LONG args[3] = {0, 0, 0};
    struct sample *s;
    LONG ns = 0, ticks = 0, maxticks, waiting = 0, gone = 0, seen = 0;
    BPTR module = 0;
    ULONG prog[8][2];
    LONG nprog = 0;
    struct Node *n;

    if (!(rda = ReadArgs("TASK/A,FILE/A,SECONDS/N", args, NULL))) return 20;
    maxticks = args[2] ? *(LONG *)args[2] * 50 : 600 * 50;
    if (!(s = AllocVec(sizeof(*s) * MAXSAMPLES, MEMF_ANY))) return 20;
    SetTaskPri(FindTask(NULL), 30);
    while (ticks++ < maxticks && ns < MAXSAMPLES)
    {
        struct Task *t;
        Delay(1);
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) break;
        Forbid();
        t = find((STRPTR)args[0]);
        if (t)
        {
            seen = 1;
            if (!module && t->tc_Node.ln_Type == NT_PROCESS && ((struct Process *)t)->pr_CLI)
            {
                /* kept now: the program is unloaded before the report */
                BPTR seg = module = ((struct CommandLineInterface *)BADDR(((struct Process *)t)->pr_CLI))->cli_Module;
                while (seg && nprog < 8)
                {
                    ULONG *h = BADDR(seg);
                    prog[nprog][0] = (ULONG)(h + 1);
                    prog[nprog++][1] = h[-1] - 8;
                    seg = *h;
                }
            }
            if (t->tc_State == TS_READY)
            {
                ULONG *sp = t->tc_SPReg;
                LONG i;
                s[ns].state = ticks;
                s[ns].pc = sp[0];
                /* the saved frame: PC (4), SR (2), d0-a6 (60): skip 66 bytes */
                sp = (ULONG *)((UBYTE *)sp + 66);
                for (i = 0; i < STACKLONGS; i++)
                    s[ns].stack[i] = ((APTR)(sp + i) < t->tc_SPUpper) ? sp[i] : 0;
                ns++;
            }
            else
                waiting++;
        }
        else if (seen)
            gone = 1;
        Permit();
        if (gone) break;
    }

    if ((out = Open((STRPTR)args[1], MODE_NEWFILE)))
    {
        LONG i, j;
        FPrintf(out, "INFO samples %ld waiting %ld ticks %ld\n", ns, waiting, ticks);
        Forbid();
        for (i = 0; i < nprog; i++)
            FPrintf(out, "SEG program %ld %08lx %ld\n", i, prog[i][0], prog[i][1]);
        for (n = SysBase->LibList.lh_Head; n->ln_Succ; n = n->ln_Succ)
        {
            struct Library *l = (struct Library *)n;
            UWORD *v;
            ULONG lo = ~0UL, hi = 0;
            if (!strcmp(n->ln_Name, "muimaster.library"))
            {
                BPTR *cf = (BPTR *)((UBYTE *)l + OFF_CLASSFILES);
                segs(n->ln_Name, *(BPTR *)((UBYTE *)l + OFF_SEGLIST));
                for (i = 0; i < NCLASSFILES; i++)
                    if (cf[i])
                    {
                        char nm[40];
                        nm[0] = 'c'; nm[1] = 'f'; nm[2] = '0' + i / 10; nm[3] = '0' + i % 10; nm[4] = 0;
                        segs(nm, cf[i]);
                    }
            }
            for (v = (UWORD *)((UBYTE *)l - 6); (UBYTE *)v >= (UBYTE *)l - l->lib_NegSize; v -= 3)
                if (v[0] == 0x4ef9)
                {
                    ULONG a = ((ULONG)v[1] << 16) | v[2];
                    if (a < lo) lo = a;
                    if (a > hi) hi = a;
                }
            FPrintf(out, "LIB %s %08lx %08lx\n", (LONG)n->ln_Name, lo, hi);
            /* a class library's seglist: right after struct Library in
             * mccinit.c (offset 36) or nearby; taken if its first hunk
             * holds the jump table's targets */
            if (hi && strcmp(n->ln_Name, "muimaster.library")
                && (ULONG)l->lib_PosSize >= 44 && TypeOfMem(l))
            {
                WORD o;
                for (o = 34; o <= 40; o += 2)
                {
                    BPTR seg = *(BPTR *)((UBYTE *)l + o);
                    ULONG *h = BADDR(seg);
                    if (seg && !(seg & 0xC0000000) && TypeOfMem(h) && h[-1] < 0x100000
                        && (ULONG)(h + 1) <= lo && hi < (ULONG)(h + 1) + h[-1])
                    {
                        segs(n->ln_Name, seg);
                        break;
                    }
                }
            }
        }
        Permit();
        for (i = 0; i < ns; i++)
        {
            FPrintf(out, "S %08lx %ld", s[i].pc, s[i].state);
            for (j = 0; j < STACKLONGS; j++)
                FPrintf(out, " %08lx", s[i].stack[j]);
            FPrintf(out, "\n");
        }
        Close(out);
    }
    FreeVec(s);
    FreeArgs(rda);
    return 0;
}
