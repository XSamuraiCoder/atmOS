.assume	adl=1
	.section .text

; A Flash App starts on the OS's own stack. The OS was already some calls deep when it launched
; the app, and the CE stops any write to the stack limit ($D19881) with an NMI reset ("RAM
; cleared"), so the app only has what is left, which can be very little. atmOS therefore runs
; on a stack of its own, a static array in app RAM that the OS never touches.
;
;   void shell_call_on_stack(void (*fn)(void), void *stack_top);
;
; Calls fn with SP = stack_top (the stack grows down from there), then switches back.
	.global	_shell_call_on_stack
_shell_call_on_stack:
	push	ix
	ld	ix, 0
	add	ix, sp			; ix = the old stack; a C function keeps ix intact
	ld	de, (ix + 6)		; fn         (ix+0 saved ix, +3 return address, +6 fn, +9 stack_top)
	ld	hl, (ix + 9)		; stack_top
	ld	sp, hl
	ex	de, hl			; hl = fn
	call	.L_call_hl
	ld	sp, ix			; back on the OS stack
	pop	ix
	ret

.L_call_hl:
	jp	(hl)
