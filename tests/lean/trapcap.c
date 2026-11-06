/* trapcap: record CPU exceptions (vectors 2-11) and exec Alert() calls into chip RAM at
 * 0x180000, which survives a reset, then pass them on. Moves the VBR to a copy of the vector
 * table (68010+); exec's own Supervisor() privilege violations and everything after a dead-end
 * alert (the reset itself) are not recorded. Stays running ("run trapcap"); Ctrl-C restores.
 * "trapcap read" prints the record after the reboot, "trapcap read clear" also clears it.
 * The technique of the crash hunt in docs/lean/README.md ("Crashes found by the real-program
 * suite"), written again. Test tool only.
 * Build: m68k-amigaos-gcc -m68020 -noixemul -Os -s -fno-delete-null-pointer-checks
 *        trapcap.c -o trapcap */
#include <exec/execbase.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

#define REC ((volatile ULONG *)0x180000)
#define RECSIZE 4096
/* layout (longs): 0 magic 'TRAP', 1 count, 2 vector, 3-18 d0-a7 (a7 = SSP after the frame),
 * 19 usp, 20-43 the frame (48 words), 44 alert, 45 alert task, 46 alert count,
 * 47-54 32 code bytes at PC-16, 55-174 120 longs of the user stack,
 * 175-... for the first 24 stack longs that look like code addresses: address + 12 bytes before */
#define R_MAGIC 0
#define R_COUNT 1
#define R_VEC 2
#define R_REGS 3
#define R_USP 19
#define R_FRAME 20
#define R_ALERT 44
#define R_ATASK 45
#define R_ACOUNT 46
#define R_CODE 47
#define R_STACK 55
#define R_RET 175

ULONG save_d0;
ULONG orig_vectors[64];
ULONG new_vbr_value;
ULONG orig_alert;

static int codeaddr(ULONG a)
{
    if (a & 1) return 0;
    if (a >= 0xF80000 && a < 0x1000000) return 1;      /* ROM */
    if (a >= 0x400 && a < 0x200000) return 1;          /* chip */
    if (a >= 0x200000 && a < 0x1000000) return 1;      /* 24-bit fast */
    if (a >= 0x08000000 && a < 0x10000000) return 1;   /* 32-bit fast */
    return 0;
}

static void copybytes(volatile UBYTE *d, const UBYTE *s, int n)
{
    while (n--) *d++ = *s++;
}

/* called from the exception stub, supervisor mode: regs[0] = vector, regs[0..14] d0-a6 */
void record(ULONG *regs, ULONG usp)
{
    UWORD *frame = (UWORD *)(regs + 15);
    ULONG vec = regs[0], pc = *(ULONG *)(frame + 1);
    int i, n;

    if (vec == 8 && pc == 0xF80BE0)
        return;                         /* exec's Supervisor() */
    if (REC[R_ACOUNT])
        return;                         /* after a dead-end alert: the reset itself */
    REC[R_MAGIC] = 0x54524150;
    REC[R_COUNT]++;
    REC[R_VEC] = vec;
    REC[R_REGS] = save_d0;
    for (i = 1; i < 15; i++) REC[R_REGS + i] = regs[i];
    REC[R_REGS + 15] = (ULONG)(regs + 15);
    REC[R_USP] = usp;
    for (i = 0; i < 24; i++) REC[R_FRAME + i] = ((ULONG *)frame)[i];
    if (codeaddr(pc) && pc > 16)
        copybytes((volatile UBYTE *)&REC[R_CODE], (UBYTE *)(pc - 16), 32);
    if (!(usp & 1) && usp >= 0x400 && usp < 0x10000000 - 480)
    {
        ULONG *st = (ULONG *)usp;
        for (i = 0; i < 120; i++) REC[R_STACK + i] = st[i];
        /* code bytes before likely return addresses (odd words too) */
        n = 0;
        for (i = 0; i < 240 && n < 24; i++)
        {
            ULONG a = *(ULONG *)((UBYTE *)usp + 2 * i);
            if (codeaddr(a) && a > 12 && (a < 0x180000 || a >= 0x181000))
            {
                REC[R_RET + 4 * n] = a;
                copybytes((volatile UBYTE *)&REC[R_RET + 4 * n + 1], (UBYTE *)(a - 12), 12);
                n++;
            }
        }
        for (; n < 24; n++) REC[R_RET + 4 * n] = 0;
    }
}

void record_alert(ULONG num, ULONG task)
{
    REC[R_MAGIC] = 0x54524150;
    REC[R_ALERT] = num;
    REC[R_ATASK] = task;
    REC[R_ACOUNT]++;
}

/* stubs: 12 bytes each, vectors 2..11 */
__asm__(
"   .text\n"
"   .globl _stubs\n"
"_stubs:\n"
"   move.l d0,_save_d0\n moveq #2,d0\n bra.w _trap_common\n"
"   move.l d0,_save_d0\n moveq #3,d0\n bra.w _trap_common\n"
"   move.l d0,_save_d0\n moveq #4,d0\n bra.w _trap_common\n"
"   move.l d0,_save_d0\n moveq #5,d0\n bra.w _trap_common\n"
"   move.l d0,_save_d0\n moveq #6,d0\n bra.w _trap_common\n"
"   move.l d0,_save_d0\n moveq #7,d0\n bra.w _trap_common\n"
"   move.l d0,_save_d0\n moveq #8,d0\n bra.w _trap_common\n"
"   move.l d0,_save_d0\n moveq #9,d0\n bra.w _trap_common\n"
"   move.l d0,_save_d0\n moveq #10,d0\n bra.w _trap_common\n"
"   move.l d0,_save_d0\n moveq #11,d0\n bra.w _trap_common\n"
"_trap_common:\n"
"   movem.l d0-d7/a0-a6,-(sp)\n"
"   move.l sp,a0\n"
"   move.l usp,a1\n"
"   move.l a1,-(sp)\n"
"   move.l a0,-(sp)\n"
"   jsr _record\n"
"   addq.l #8,sp\n"
"   movem.l (sp)+,d0-d7/a0-a6\n"
"   move.l a0,-(sp)\n"
"   lea _orig_vectors,a0\n"
"   lsl.w #2,d0\n"
"   move.l 0(a0,d0.w),d0\n"
"   move.l (sp)+,a0\n"
"   move.l d0,-(sp)\n"
"   move.l _save_d0,d0\n"
"   rts\n"
"   .globl _alert_stub\n"
"_alert_stub:\n"
"   movem.l d0-d1/a0-a1,-(sp)\n"
"   move.l 276(a6),-(sp)\n"
"   move.l d7,-(sp)\n"
"   jsr _record_alert\n"
"   addq.l #8,sp\n"
"   movem.l (sp)+,d0-d1/a0-a1\n"
"   move.l _orig_alert,-(sp)\n"
"   rts\n"
"   .globl _getvbr\n"
"_getvbr:\n"
"   movec vbr,d0\n"
"   rte\n"
"   .globl _setvbr\n"
"_setvbr:\n"
"   move.l _new_vbr_value,d0\n"
"   movec d0,vbr\n"
"   rte\n"
);
extern UBYTE stubs[];
extern void alert_stub(void);
extern ULONG getvbr(void);
extern void setvbr(void);

int main(int argc, char **argv)
{
    ULONG *table, oldvbr, i;
    APTR rec;

    if (argc > 1 && !strcmp(argv[1], "read"))
    {
        if (REC[R_MAGIC] != 0x54524150) { printf("no record\n"); return 0; }
        printf("count %lu vector %lu  alert %08lx task %08lx (alerts %lu)\n", REC[R_COUNT], REC[R_VEC],
            REC[R_ALERT], REC[R_ATASK], REC[R_ACOUNT]);
        printf("d0-d7:"); for (i = 0; i < 8; i++) printf(" %08lx", REC[R_REGS + i]); printf("\n");
        printf("a0-a7:"); for (i = 8; i < 16; i++) printf(" %08lx", REC[R_REGS + i]); printf("\n");
        printf("usp %08lx frame:", REC[R_USP]); for (i = 0; i < 12; i++) printf(" %08lx", REC[R_FRAME + i]); printf("\n");
        printf("code at pc-16:"); for (i = 0; i < 8; i++) printf(" %08lx", REC[R_CODE + i]); printf("\n");
        printf("stack:"); for (i = 0; i < 120; i++) printf("%s%08lx", i % 8 ? " " : "\n ", REC[R_STACK + i]); printf("\n");
        printf("returns:\n");
        for (i = 0; i < 24 && REC[R_RET + 4 * i]; i++)
            printf(" %08lx: %08lx %08lx %08lx\n", REC[R_RET + 4 * i], REC[R_RET + 4 * i + 1],
                REC[R_RET + 4 * i + 2], REC[R_RET + 4 * i + 3]);
        if (argc > 2) REC[R_MAGIC] = 0;
        return 0;
    }
    if (!(SysBase->AttnFlags & AFF_68010)) { printf("needs a 68010+\n"); return 20; }
    rec = AllocAbs(RECSIZE, (APTR)0x180000);
    if (!rec) printf("warning: 0x180000 not allocated\n");
    for (i = 0; i < RECSIZE / 4; i++) REC[i] = 0;
    table = AllocMem(1024 + 4, MEMF_PUBLIC);
    if (!table) return 20;
    oldvbr = Supervisor((ULONG (*)())getvbr);
    for (i = 0; i < 256; i++) table[i] = ((ULONG *)oldvbr)[i];
    for (i = 0; i < 64; i++) orig_vectors[i] = ((ULONG *)oldvbr)[i];
    for (i = 2; i <= 11; i++) table[i] = (ULONG)(stubs + 12 * (i - 2));
    Disable();
    orig_alert = (ULONG)SetFunction((struct Library *)SysBase, -108, (APTR)alert_stub);
    new_vbr_value = (ULONG)table;
    Supervisor((ULONG (*)())setvbr);
    Enable();
    printf("trapcap: vbr %08lx -> %08lx, record at 0x180000\n", oldvbr, (ULONG)table);
    fflush(stdout);
    Wait(SIGBREAKF_CTRL_C);
    Disable();
    new_vbr_value = oldvbr;
    Supervisor((ULONG (*)())setvbr);
    SetFunction((struct Library *)SysBase, -108, (APTR)orig_alert);
    Enable();
    FreeMem(table, 1028);
    if (rec) FreeMem(rec, RECSIZE);
    return 0;
}
