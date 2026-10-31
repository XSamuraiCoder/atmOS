;--------------------------------------
;
; atmOS - osLaunch.s
; Run programs from the TI-OS through the shell's program runner:
; the OS program menu (EXEC tab),  prgmPRGM  and  Asm(prgmPRGM)  typed
; on the homescreen now work for asm / C / ICE / hex / archived programs.
;
; Homescreen-hook technique adapted from CEaShell's hooks.s and
; runProgram.s.  Copyright 2022-2026 RoccoLox Programs and TIny_Hacker.
; SPDX-License-Identifier: GPL-3.0-only
;
; How it works: when Enter is pressed, the OS first stores the command
; line in its temporary program "prgm#" and then calls the homescreen
; hook with reason 2.  If that line is  prgmNAME  or  Asm(prgmNAME),
; this hook finds the target program, classifies it exactly like
; program_kind.h does, and jumps into _asm_runProgram_main with
; os_launch_return_os_flag set: the program runs through atmOS's
; runner (archived copy, ICE, hex squish, ...) and afterwards returns
; to the OS homescreen instead of restarting the shell.
;
; Commands the stock OS already runs correctly stay with the OS:
; RAM BASIC via prgmNAME, and Asm( of RAM native / hex programs.
;
; State lives in main.c's .bss (uint8_t os_launch_*): app RAM survives
; program runs and is zeroed at every app start, which resets the
; "return to OS" flag to the safe default (return to the shell).
;
; Flash App build only (it externs _flash_program_return_to_shell):
; add this file to the Makefile's asm sources.
;
;--------------------------------------
	.assume	adl=1
	.include	"src/asm/include/equates.inc"

	.global	_asm_oslaunch_install
	.global	_asm_oslaunch_remove
	.global	_asm_oslaunch_dispatch
	.global	_asm_runProgram_returnOS.restoreHooks

	.extern	_asm_runProgram_main
	.extern	_flash_program_return_to_shell
	.extern	_os_launch_flags
	.extern	_os_launch_kind
	.extern	_os_launch_return_os_flag

; token bytes kept local (values, not ti.* names, so this also builds
; against trimmed equates; they match program_kind.h and runProgram.s)
.equ	osLaunch_tok2Byte,		$BB	; 2-byte token prefix
.equ	osLaunch_tokAsmCmd,		$6A	; Asm(  = BB 6A on the 84+CE
.equ	osLaunch_tokProg,		$5F	; prgm
.equ	osLaunch_tokExt,		$EF	; tExtTok
.equ	osLaunch_tokHexSrc,		$7A	; Asm84CEPrgm (hex-text source)
.equ	osLaunch_tokNative,		$7B	; AsmComp( = native binary prefix
.equ	osLaunch_iceMark,		$7F	; EF 7B 7F : ICE
; os_launch_flags bits
.equ	osLaunch_fAsm,		0	; the command used Asm(
.equ	osLaunch_fArchived,		1	; the target program is archived
.equ	osLaunch_fNative,		2	; the target starts with EF 7x

	.section	.text

;---------------- the hook ----------------
hooks_homescreenHook:
	db	$83
	cp	a, 2			; reason 2: Enter on the homescreen
	jp	z, osLaunch.run
	cp	a, a			; other reasons: Z = the OS continues
	ret

osLaunch.run:
	push	af
	push	bc
	push	de
	push	hl
	; the OS wrote the command line into prgm#: read it back
	ld	hl, osLaunch_hashProg
	call	ti.Mov9ToOP1
	call	ti.ChkFindSym
	jp	c, osLaunch.pass
	ex	de, hl			; hl -> size field ("#" is always in RAM)
	ld	bc, 0
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	hl			; hl -> first token of the command
	ld	a, b
	or	a, c
	jp	z, osLaunch.pass
	ld	a, b
	or	a, a
	jp	nz, osLaunch.cmd		; long command line: fine
	ld	a, c
	cp	a, 2
	jp	c, osLaunch.pass		; 0/1 bytes: empty command line

osLaunch.cmd:
	xor	a, a
	ld	(_os_launch_flags), a
	ld	a, (hl)
	cp	a, osLaunch_tokProg		; prgmNAME
	jp	z, osLaunch.prgm
	cp	a, osLaunch_tok2Byte		; Asm(prgmNAME)?
	jp	nz, osLaunch.pass
	dec	bc
	jp	z, osLaunch.pass
	inc	hl
	ld	a, (hl)
	cp	a, osLaunch_tokAsmCmd
	jp	nz, osLaunch.pass
	dec	bc
	jp	z, osLaunch.pass
	inc	hl
	ld	a, (hl)
	cp	a, osLaunch_tokProg
	jp	nz, osLaunch.pass		; Asm(<not a program>): OS handles
	ld	a, (_os_launch_flags)
	set	osLaunch_fAsm, a
	ld	(_os_launch_flags), a		; fall through with hl -> tProg

osLaunch.prgm:
	dec	bc			; the prgm token
	inc	hl			; hl -> program name
	ld	de, ti.OP1
	ld	a, ti.ProgObj
	ld	(de), a
	inc	de
	ld	a, b
	or	a, a
	jp	nz, osLaunch.name8
	ld	a, c
	cp	a, 9
	jp	c, osLaunch.name		; bc < 9: take the whole rest
osLaunch.name8:
	ld	bc, 8			; names are 8 bytes max
osLaunch.name:
	ldir
	xor	a, a
	ld	(de), a			; zero terminator / padding

	; never touch the OS's own temporary system programs
	ld	hl, ti.OP1 + 1
	ld	a, (hl)
	cp	a, '#'
	jp	z, osLaunch.pass
	cp	a, '!'
	jp	z, osLaunch.pass

	; find the target (protected programs hide behind ProtProgObj)
	call	ti.ChkFindSym
	jp	nc, osLaunch.found
	ld	hl, ti.OP1
	ld	(hl), ti.ProtProgObj
	call	ti.ChkFindSym
	jp	c, osLaunch.pass		; no such program: OS shows the error

osLaunch.found:
	call	ti.ChkInRam
	jp	z, osLaunch.data
	; archived: data pointer = de + 10 + size (same idiom as _asm_utils_findVar)
	ld	hl, 10
	add	hl, de
	ld	a, c
	ld	bc, 0
	ld	c, a
	add	hl, bc
	ex	de, hl
	ld	a, (_os_launch_flags)
	set	osLaunch_fArchived, a
	ld	(_os_launch_flags), a

osLaunch.data:
	inc	de
	inc	de			; past the 2-byte size: de -> program data
	; classify like program_kind.h
	ld	a, (de)
	cp	a, osLaunch_tokExt		; EF ...
	jp	nz, osLaunch.basic
	inc	de
	ld	a, (de)
	dec	de
	cp	a, osLaunch_tokNative		; EF 7B: native binary
	jp	z, osLaunch.native
	cp	a, osLaunch_tokHexSrc		; EF 7A: hex-text source
	jp	nz, osLaunch.basic
	ld	a, 2			; PROGRAM_BASIC: the runner squishes it
	ld	(_os_launch_kind), a
	ld	a, (_os_launch_flags)
	set	osLaunch_fNative, a
	ld	(_os_launch_flags), a		; but the stock OS cannot run it
	jp	osLaunch.decide			; from prgmNAME

osLaunch.basic:
	ld	a, 2			; PROGRAM_BASIC
	ld	(_os_launch_kind), a
	jp	osLaunch.decide

osLaunch.native:
	inc	de
	inc	de
	ld	a, (de)
	or	a, a
	jp	z, osLaunch.kindC		; EF 7B 00: C toolchain
	cp	a, osLaunch_iceMark
	jp	z, osLaunch.kindICE		; EF 7B 7F: ICE
	xor	a, a			; PROGRAM_ASM
	jp	osLaunch.kindGot
osLaunch.kindC:
	ld	a, 1			; PROGRAM_C
	jp	osLaunch.kindGot
osLaunch.kindICE:
	ld	a, 3			; PROGRAM_ICE
osLaunch.kindGot:
	ld	(_os_launch_kind), a
	ld	a, (_os_launch_flags)
	set	osLaunch_fNative, a
	ld	(_os_launch_flags), a

osLaunch.decide:
	; take over only when the stock OS cannot run the command itself:
	;  - archived programs of any kind ("ERR:ARCHIVED" otherwise)
	;  - prgmNAME of a native or hex program (the OS would read it as BASIC)
	; Asm( of a RAM program (native or hex) runs fine in the stock OS.
	ld	a, (_os_launch_flags)
	bit	osLaunch_fArchived, a
	jp	nz, osLaunch.go
	bit	osLaunch_fAsm, a
	jp	nz, osLaunch.pass
	bit	osLaunch_fNative, a
	jp	z, osLaunch.pass		; prgmNAME + RAM + plain BASIC: OS run

osLaunch.go:
	ld	a, 1
	ld	(_os_launch_return_os_flag), a	; the runner returns to the OS
	pop	hl
	pop	de
	pop	bc
	pop	af
	ld	iy, ti.flags
	ld	a, (_os_launch_kind)
	jp	_asm_runProgram_main		; OP1 = type + name, A = kind

osLaunch.pass:
	pop	hl
	pop	de
	pop	bc
	pop	af
	cp	a, a			; Z: the OS executes the command
	ret

;---------------- return path ----------------
; runProgram.s jumps here instead of straight to _flash_program_return_to_shell.
; Shell launches (Files, dock pins, NOTES program) restart the shell as before;
; a program this hook launched from the OS returns to the OS homescreen.
_asm_oslaunch_dispatch:
	ld	a, (_os_launch_return_os_flag)
	or	a, a
	jp	z, _flash_program_return_to_shell
	; fall through: OS launch
_asm_oslaunch_returnOS:
	xor	a, a
	ld	(_os_launch_return_os_flag), a	; back to the safe default
	call	ti.SaveCmdShadow
	ld	a, ti.cxCmd
	call	ti.NewContext0			; fresh homescreen context: drops the
						; still-pending prgmNAME / Asm( line
	ld	a, (ti.errNo)
	or	a, a
	jp	z, osLaunch.retOS.noClr
	call	ti.ClrScrn			; an error ended the run: clean the
	call	ti.HomeUp			; screen so the homescreen is fresh
osLaunch.retOS.noClr:
	call	ti.PPutAway
	; OS routines during the run (ParseInp, editor, other hooks) can clear
	; our homescreen hook: put it back (upstream CEaShell does the same at
	; the end of its returnOS)
	ld	iy, ti.flags
	ld	hl, hooks_homescreenHook
	call	ti.SetHomescreenHook
	xor	a, a
	or	a, 2				; homescreen hook protocol: "handled"
	ret					; back into the OS, right after the
						; homescreen hook call

;---------------- install / remove ----------------
_asm_oslaunch_install:
	ld	iy, ti.flags
	ld	hl, hooks_homescreenHook
	jp	ti.SetHomescreenHook

_asm_oslaunch_remove:
	ld	iy, ti.flags
	bit	ti.homescreenHookActive, (iy + ti.hookflags2)
	ret	z
	ld	hl, (ti.homescreenHookPtr)
	ld	de, hooks_homescreenHook
	or	a, a
	sbc	hl, de
	ret	nz			; someone else's hook: leave it alone
	jp	ti.ClrHomescreenHook

;---------------- resolves the extern in hooks.s ----------------
; _asm_hooks_basicPrgmHook calls this after an error / quit exit of a
; BASIC run (CEaShell's runProgram.s provides it upstream): put our
; homescreen hook back and return with the hook protocol's "handled".
; (If your tree already defines this label elsewhere, delete this block.)
_asm_runProgram_returnOS.restoreHooks:
	ld	iy, ti.flags
	ld	hl, hooks_homescreenHook
	call	ti.SetHomescreenHook
	xor	a, a
	or	a, 2
	ret

	.section	.rodata
osLaunch_hashProg:
	.db	ti.ProgObj, '#', 0, 0, 0, 0, 0, 0, 0	; the OS's command-log program