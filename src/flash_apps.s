.assume	adl=1
	.section .text
	.include "src/asm/include/equates.inc"

; TI OS internal Flash App search and launch path; OS-version-dependent.
; The return-to-shell bridge follows CEaShell's GPL-3.0 app restart behavior.
	.extern	_asm_utils_clrScrnAndUsedRAM
	.global _flash_apps_find_next
	.global _flash_apps_find_app
	.global _os_FindAppStart
	.global _flash_apps_launch
	.global _flash_program_return_to_shell

_flash_apps_find_next:
	call	0x210F4
	ld	a,0
	ret	c
	inc	a
	ret

_os_FindAppStart:
	jp	0x21100

_flash_apps_find_app:
	pop	de
	ex	(sp),hl
	push	de
	push	iy
	call	ti.FindAppStart
	pop	iy
	jr	c,.L_app_not_found
	ret

.L_app_not_found:
	ld	hl,0
	ret

_flash_program_return_to_shell:
	ld	hl,.L_shell_app_name
	call	ti.FindAppStart
	jr	c,.L_shell_not_found
	push	hl
	call	_flash_apps_launch
	jp	0x20108

.L_shell_not_found:
	jp	0x20108

_flash_apps_launch:
	pop	hl			; return address (unused)
	pop	de			; app header
	; Upstream CEaShell switches to the OS stack (_exit_sp) before wiping app RAM.
	; Do the same: the MemClear of $D1787C.. below must never run over our own
	; C stack. The header lives on the OS stack, not in .bss.
	ld	sp,(ti.onSP)
	push	de

	ei			; FIX: a program may have left interrupts off; GetCSC needs them
.L_wait_for_key_release:
	call	ti.GetCSC
	or	a,a
	jr	nz,.L_wait_for_key_release

	ld	a,0x25
	ld	(0xD02687),a
	xor	a,a
	ld	(ti.menuCurrent),a
	ld	(ti.appErr1),a
	ld	(ti.kbdGetKy),a

	ld	iy,ti.flags
	set	1,(iy+0x0D)
	res	3,(iy+0x4A)
	res	5,(iy+0x4C)
	ld	de,ti.userMem
	ld	hl,(ti.asm_prgm_size)
	ex	de,hl
	call	ti.DelMem
	ld	hl,0
	ld	(ti.asm_prgm_size),hl

	res	ti.useTokensInString,(iy+ti.clockFlags)
	res	ti.onInterrupt,(iy+ti.onFlags)
	set	ti.graphDraw,(iy+ti.graphFlags)
	call	ti.ReloadAppEntryVecs
	call	ti.AppSetup
	set	ti.appRunning,(iy+ti.APIFlg)
	set	6,(iy+ti.APIFlg)
	res	0,(iy+0x2C)
	set	ti.appAllowContext,(iy+ti.APIFlg)

	ld	hl,0xD1787C
	ld	bc,0xFFF
	call	ti.MemClear
	call	_asm_utils_clrScrnAndUsedRAM
	call	ti.DrawStatusBar

	pop	hl			; app header
	ld	sp,(ti.onSP)
	push	hl
	call	ti.ResetStacks
	pop	hl
	ld	de,0x100
	add	hl,de
	push	hl			; app code base (kept on the stack, not in .bss)
	ex	de,hl			; de = code base
	ld	hl,0x18
	add	hl,de
	ld	hl,(hl)		; initialized data length
	add	hl,de
	or	a,a
	sbc	hl,de
	jr	z,.L_no_initialized_data
	push	hl
	pop	bc
	ld	hl,0x15
	add	hl,de
	ld	hl,(hl)
	add	hl,de
	ld	de,0xD1787C
	ldir

.L_no_initialized_data:
	pop	de			; code base
	ld	hl,0x1B
	add	hl,de
	ld	hl,(hl)
	add	hl,de
	ld	de,.L_app_exit	; safe return address for when the app returns
	push	de
	jp	(hl)

.L_app_exit:			; FIX: leave to the OS home screen instead of RETing into garbage
	jp	0x20108

	.section .rodata
.L_shell_app_name:
	.db	"atmOS",0,0,0	; must match SHELL_APP_NAME / the Makefile NAME; padded to 8 bytes