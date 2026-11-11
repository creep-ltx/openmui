/* stackmark NAME [ticks]: find the CLI process running command NAME (as the
 * Shell shows it, e.g. RAM:MUI-Demo), fill the unused part of its stack with
 * a pattern while it waits, poll until it exits and print the deepest stack
 * use seen (bytes from the top). Then it lists every even long word found in
 * the stack at the deepest moment, with its distance from the top: map the
 * ones inside the library with its link map (lvo30 = the address of the
 * library function at offset -30) to see which code went that deep. Build:
 * m68k-amigaos-gcc -Os -noixemul -m68000 -o stackmark stackmark.c */
#include <exec/execbase.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern struct ExecBase *SysBase;
#define PAT 0x5AA5C33CUL

static int namematch(struct Task *t, const char *name)
{
    struct Process *p = (struct Process *)t;
    struct CommandLineInterface *cli;
    UBYTE *b;
    if (t->tc_Node.ln_Type != NT_PROCESS || !p->pr_CLI) return 0;
    cli = BADDR(p->pr_CLI);
    if (!cli->cli_Module) return 0;
    b = BADDR(cli->cli_CommandName);
    return b && b[0] == strlen(name) && !strncmp((char *)b + 1, name, b[0]);
}

static struct Task *find(const char *name)
{
    struct Node *n;
    for (n = SysBase->TaskWait.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (namematch((struct Task *)n, name)) return (struct Task *)n;
    for (n = SysBase->TaskReady.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (namematch((struct Task *)n, name)) return (struct Task *)n;
    return NULL;
}

int main(int argc, char **argv)
{
    struct Task *t;
    ULONG *lo, *hi, *p, *deep;
    ULONG upper, size, used, worst = 0;
    static ULONG dump[12000];
    ULONG dumpat = 0, i;
    struct Library *lb;
    LONG ticks = argc > 2 ? atoi(argv[2]) : 1;
    if (argc < 2) return 20;
    Forbid();
    t = find(argv[1]);
    if (!t || t->tc_State != TS_WAIT) { Permit(); printf("not found or not waiting\n"); return 10; }
    lo = (ULONG *)(((ULONG)t->tc_SPLower + 3) & ~3UL) + 4;
    hi = (ULONG *)(((ULONG)t->tc_SPReg & ~3UL)) - 32;
    upper = (ULONG)t->tc_SPUpper;
    size = upper - (ULONG)t->tc_SPLower;
    for (p = lo; p < hi; p++) *p = PAT;
    Permit();
    printf("stack %lu bytes, in use now %lu, filled %lu\n", size, upper - (ULONG)t->tc_SPReg,
        (ULONG)((char *)hi - (char *)lo));
    fflush(stdout);
    for (;;)
    {
        Delay(ticks);
        Forbid();
        if (find(argv[1]) != t) { Permit(); break; }
        for (deep = lo; deep < hi && *deep == PAT; deep++) ;
        used = upper - (ULONG)deep;
        Permit();
        if (used > worst)
        {
            worst = used; dumpat = (ULONG)deep;
            Forbid();
            for (i = 0; i < 12000 && deep + i < (ULONG *)upper; i++) dump[i] = deep[i];
            Permit();
            printf("deepest %lu\n", worst); fflush(stdout);
        }
    }
    printf("exited; deepest %lu bytes\n", worst);
    {
        Forbid();
        lb = (struct Library *)FindName(&SysBase->LibList, (STRPTR)"muimaster.library");
        if (lb) printf("muimaster.library base %08lx lvo30 %08lx\n", (ULONG)lb,
            *(UWORD *)((char *)lb - 30) == 0x4EF9 ? *(ULONG *)((char *)lb - 28) : 0);
        Permit();
    }
    for (i = 0; i < 12000; i++)
        if (dump[i] >= 0x1000 && dump[i] < 0x1000000 && !(dump[i] & 1))
            printf("@%lu %08lx\n", (ULONG)(upper - (dumpat + 4 * i)), dump[i]);
    return 0;
}
