| gfxwatch patches: each stub saves the registers, calls gw_check(fn, a0, a1, sp) and jumps
| to the original. The C side decides what a1 (a RastPort, a BitMap or memory) is from fn.
	.text
	.globl	_gw_stub0
	.globl	_gw_stub1
	.globl	_gw_stub2
	.globl	_gw_stub3
	.globl	_gw_stub4
	.globl	_gw_stub5
	.globl	_gw_stub6
	.globl	_gw_stub7
	.globl	_gw_stub8
	.globl	_gw_stub9
	.globl	_gw_stub10
	.globl	_gw_stub11
	.globl	_gw_stub12
	.globl	_gw_stub13
	.globl	_gw_stub14
	.globl	_gw_stub15
	.globl	_gw_stub16
	.globl	_gw_stub17
	.globl	_gw_stub18
	.globl	_gw_old
	.globl	_gw_check
.macro STUB n
_gw_stub\n:
	movem.l	d0-d7/a0-a6,-(sp)
	lea	60(sp),a2
	move.l	a2,-(sp)
	move.l	a1,-(sp)
	move.l	a0,-(sp)
	move.l	#\n,-(sp)
	jsr	_gw_check
	lea	16(sp),sp
	movem.l	(sp)+,d0-d7/a0-a6
	move.l	_gw_old+4*\n,-(sp)
	rts
.endm
	STUB 0
	STUB 1
	STUB 2
	STUB 3
	STUB 4
	STUB 5
	STUB 6
	STUB 7
	STUB 8
	STUB 9
	STUB 10
	STUB 11
	STUB 12
	STUB 13
	STUB 14
	STUB 15
	STUB 16
	STUB 17
	STUB 18
	.data
_gw_old:	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
