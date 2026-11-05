.assume	adl=1
	.include	"src/asm/include/equates.inc"
	.section .text

; A Flash App starts on the OS's own stack. The OS was already some calls deep when it launched
; the app, and the CE stops any write to the stack limit ($D19881) with an NMI reset ("RAM
; cleared"), so the app only has what is left, which can be very little. atmOS therefore runs
; on a stack of its own: a block from the heap (NOT a static array - app .bss shares the small
; app RAM window that ends at that same guard).
;
;   void shell_call_on_stack(void (*fn)(void), void *stack_top);
;
; Calls fn with SP = stack_top (the stack grows down from there), then switches back.
	.global	_shell_call_on_stack
	.global	_shell_call_on_os_stack
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

; Calls a no-argument function on the OS stack and returns to the caller's stack.
; void shell_call_on_os_stack(void (*fn)(void));
;
; If the shell is ALREADY on the OS stack (the fallback when no stack of its own could be
; allocated), setting SP = ti.onSP would overwrite the shell's own live frames, which sit just
; below onSP. In that case fn is called directly. The shell's own stack comes from the heap, far
; below the OS stack (more than 8 KB away), so "within 8 KB below onSP" means "on the OS stack".
_shell_call_on_os_stack:
	push	ix
	ld	ix, 0
	add	ix, sp
	ld	hl, (ti.onSP)
	or	a, a
	sbc	hl, sp			; hl = onSP - sp
	ld	de, $002000
	or	a, a
	sbc	hl, de
	jr	c, .L_already_on_os	; (onSP - sp) < 8 KB: already on the OS stack
	ld	hl, (ix + 6)		; fn
	ld	sp, (ti.onSP)
	call	.L_call_hl
	ld	sp, ix
	pop	ix
	ret
.L_already_on_os:
	ld	hl, (ix + 6)		; fn
	call	.L_call_hl
	pop	ix
	ret

.L_call_hl:
	jp	(hl)