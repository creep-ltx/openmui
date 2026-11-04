| Zune68k: memtally patches for exec AllocMem (-198) and FreeMem (-210).
| The C side (mt_alloc/mt_free) decides about guard padding and calls the
| originals through callAllocMem/callFreeMem.
	.text
	.globl	_newAllocMem
	.globl	_newFreeMem
	.globl	_callAllocMem
	.globl	_callFreeMem
	.globl	_oldAllocMem
	.globl	_oldFreeMem
_newAllocMem:
	movem.l	d1/a0-a1/a6,-(sp)
	lea	16(sp),a0
	move.l	a0,-(sp)
	move.l	d1,-(sp)
	move.l	d0,-(sp)
	jsr	_mt_alloc
	lea	12(sp),sp
	movem.l	(sp)+,d1/a0-a1/a6
	rts
_newFreeMem:
	movem.l	d0-d1/a0-a1/a6,-(sp)
	lea	20(sp),a0
	move.l	a0,-(sp)
	move.l	d0,-(sp)
	move.l	a1,-(sp)
	jsr	_mt_free
	lea	12(sp),sp
	movem.l	(sp)+,d0-d1/a0-a1/a6
	rts
| APTR callAllocMem(ULONG size, ULONG flags)
_callAllocMem:
	move.l	a6,-(sp)
	move.l	8(sp),d0
	move.l	12(sp),d1
	move.l	4.w,a6
	move.l	_oldAllocMem,a1
	jsr	(a1)
	move.l	(sp)+,a6
	rts
| void callFreeMem(APTR block, ULONG size)
_callFreeMem:
	move.l	a6,-(sp)
	move.l	8(sp),a1
	move.l	12(sp),d0
	move.l	4.w,a6
	move.l	_oldFreeMem,a0
	jsr	(a0)
	move.l	(sp)+,a6
	rts
	.data
_oldAllocMem:	.long	0
_oldFreeMem:	.long	0
