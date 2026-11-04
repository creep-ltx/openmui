	.text
	.globl	_callseg
_callseg:
	move.l	4(sp),a1
	movem.l	d2-d7/a2-a6,-(sp)
	lea	arg(pc),a0
	moveq	#1,d0
	jsr	(a1)
	movem.l	(sp)+,d2-d7/a2-a6
	rts
arg:	.ascii	"\n\0"
	.even
