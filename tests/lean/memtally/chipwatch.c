/* chipwatch ADDR | chipwatch read: a VBlank interrupt server watches the long at ADDR (hex) for
   the value 0 once it has been non-zero (a free chunk's mc_Bytes); when it turns 0 the server
   records the task running at that moment, its name, 48 longs of the supervisor stack (the
   interrupted PC is among them) and the hunks of that task's program and of muimaster.library,
   into chip RAM at 0x1B0000 (survives a reset) and prints a line through RawPutChar.
   Build: m68k-amigaos-gcc -Os -m68000 -noixemul -s -o chipwatch chipwatch.c */
#include <proto/exec.h>
#include <proto/dos.h>
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <hardware/intbits.h>
#include <string.h>
#include <stdlib.h>

#define RECADDR 0x1D0000
#define MAGIC 0x43575431
struct rec { ULONG magic, caught, addr, armed, task, stack[48], rlo[16], rhi[16], nrange; char name[40], rname[16][12]; ULONG cad[8]; UBYTE code[8][16]; ULONG ncode; };
static struct rec *R;

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
static void ranges(BPTR seg, const char *name)
{
    while (seg && R->nrange < 16) {
        ULONG *p = (ULONG *)BADDR(seg); int i;
        R->rlo[R->nrange] = (ULONG)(p + 1); R->rhi[R->nrange] = (ULONG)p + p[-1] - 4;
        for (i = 0; i < 11 && name[i]; i++) R->rname[R->nrange][i] = name[i];
        R->rname[R->nrange][i] = 0; R->nrange++; seg = p[0];
    }
}

/* the server: a1 = R (is_Data); runs in supervisor mode on the supervisor stack */
static LONG server(struct rec *r __asm("a1"))
{
    ULONG v = *(volatile ULONG *)r->addr; ULONG *sp; int i;
    if (r->caught) return 0;
    if (!r->armed) { if (v) r->armed = 1; return 0; }
    if (v) return 0;
    r->caught = 1;
    {
        struct Task *t = SysBase->ThisTask; struct Process *pr = (struct Process *)t; struct Library *lb;
        const char *n = t->tc_Node.ln_Name ? t->tc_Node.ln_Name : "?";
        r->task = (ULONG)t;
        for (i = 0; i < 39 && n[i]; i++) r->name[i] = n[i];
        r->name[i] = 0;
        __asm volatile("move.l sp,%0" : "=r"(sp));
        for (i = 0; i < 48; i++) r->stack[i] = sp[i];
        for (i = 0; i < 48 && r->ncode < 8; i++) {
            ULONG v = sp[i]; int b;
            if (v < 0x1000 || v >= 0xF80000 || (v & 1) || v >= 0x1000000) continue;
            if (v < 0x200000 || v >= 0x800000) continue;   /* RAM code lives in fast RAM here */
            r->cad[r->ncode] = v;
            for (b = 0; b < 16; b++) r->code[r->ncode][b] = *(UBYTE *)(v - 12 + b);
            r->ncode++;
        }
        if (t->tc_Node.ln_Type == NT_PROCESS && pr->pr_CLI) {
            struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(pr->pr_CLI);
            if (cli->cli_Module) ranges(cli->cli_Module, "program");
        }
        lb = (struct Library *)FindName(&SysBase->LibList, "muimaster.library");
        if (lb) ranges(*(BPTR *)((UBYTE *)lb + 38), "muimaster");
    }
    kput("chipwatch: CAUGHT task "); khex(r->task); kput(r->name); kput("\n");
    return 0;
}

int main(int argc, char **argv)
{
    struct Interrupt is; ULONG i;
    R = (struct rec *)RECADDR;
    if (argc > 1 && !strcmp(argv[1], "read")) {
        if (R->magic != MAGIC) { PutStr("chipwatch: no record\n"); return 5; }
        if (!R->caught) { Printf("chipwatch: %08lx never turned 0 (armed %ld)\n", R->addr, R->armed); return 0; }
        Printf("chipwatch: %08lx turned 0 while task %08lx \"%s\" ran\n supervisor stack:", R->addr, R->task, (ULONG)R->name);
        for (i = 0; i < 48; i++) Printf("%s%08lx", (ULONG)(i % 8 ? " " : "\n  "), R->stack[i]);
        PutStr("\n hunks:\n");
        for (i = 0; i < R->nrange; i++) Printf("  %08lx-%08lx %s\n", R->rlo[i], R->rhi[i], (ULONG)R->rname[i]);
        PutStr(" code (12 bytes before, 4 after) at the fast-RAM stack longs:\n");
        for (i = 0; i < R->ncode; i++) { int b; Printf("  %08lx:", R->cad[i]); for (b = 0; b < 16; b++) Printf("%02lx", (ULONG)R->code[i][b]); PutStr("\n"); }
        return 0;
    }
    if (argc < 2) { PutStr("chipwatch ADDR | chipwatch read\n"); return 5; }
    if (!AllocAbs(sizeof(struct rec), (APTR)RECADDR)) { PutStr("chipwatch: chip RAM at 0x1D0000 not free\n"); return 10; }
    memset(R, 0, sizeof *R); R->magic = MAGIC; R->addr = strtoul(argv[1], NULL, 16);
    memset(&is, 0, sizeof is);
    is.is_Node.ln_Type = NT_INTERRUPT; is.is_Node.ln_Pri = 100; is.is_Node.ln_Name = "chipwatch";
    is.is_Data = R; is.is_Code = (void (*)())server;
    AddIntServer(INTB_VERTB, &is);
    kput("chipwatch: on\n");
    Wait(SIGBREAKF_CTRL_C);
    RemIntServer(INTB_VERTB, &is);
    return 0;
}
