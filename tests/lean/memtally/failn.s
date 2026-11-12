| failn: one patch stub per hooked library function. Each stub saves the
| registers, asks fn_hook(index, saved) whether this call is the one to
| fail, and either jumps to the original (registers as they were) or returns
| the function's failure value at once. saved[0..4] = d0 d1 a0 a1 a6,
| saved[5] = the return address of the patched call, saved[6..] the caller's
| stack. The stubs are plain 68000 code.
	.text
	.globl	_fn_orig
	.globl	_fn_fail
	.globl	_fn_stubs

.macro	STUB n
	.globl	_fn_stub\n
_fn_stub\n:
	movem.l	d0-d1/a0-a1/a6,-(sp)
	move.l	sp,-(sp)
	move.l	#\n,-(sp)
	jsr	_fn_hook
	addq.l	#8,sp
	tst.l	d0
	bne	1f
	movem.l	(sp)+,d0-d1/a0-a1/a6
	move.l	_fn_orig+4*\n,-(sp)
	rts
1:	movem.l	(sp)+,d0-d1/a0-a1/a6
	move.l	_fn_fail+4*\n,d0
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
	STUB 19

	.data
_fn_stubs:
	.long	_fn_stub0, _fn_stub1, _fn_stub2, _fn_stub3, _fn_stub4
	.long	_fn_stub5, _fn_stub6, _fn_stub7, _fn_stub8, _fn_stub9
	.long	_fn_stub10, _fn_stub11, _fn_stub12, _fn_stub13, _fn_stub14
	.long	_fn_stub15, _fn_stub16, _fn_stub17, _fn_stub18, _fn_stub19
_fn_orig:
	.space	80
_fn_fail:
	.space	80
