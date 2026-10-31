;--------------------------------------
;
; atmOS - runProgram.s
; Adapted from CEaShell (RoccoLox Programs / TIny_Hacker, GPL-3.0)
; for atmOS Flash App program launching.
;
; NEW: OS-side launches (src/asm/osLaunch.s) enter at _asm_runProgram_main
; with the return-to-OS flag set; every end-of-run exit now goes through
; _asm_oslaunch_dispatch, which returns to the OS homescreen for those
; runs and to the shell for normal (Files / dock) launches.
;
; void asm_runProgram_run(const char *name, uint8_t var_type,
;                         uint8_t program_kind, const void *preferences)
;   __attribute__((noreturn));
;
; program_kind (program_kind.h):
;   0 PROGRAM_ASM      native eZ80 -> load to userMem, jp
;   1 PROGRAM_C        native C    -> load to userMem, jp
;   2 PROGRAM_BASIC    TI-BASIC    -> ParseInp prgmNAME
;   3 PROGRAM_ICE      native ICE  -> load to userMem, jp
;   4 PROGRAM_ASMCALL  Z80 AsmPrgm -> temp prog Asm(prgmNAME), ParseInp
;
;--------------------------------------
	.assume	adl=1
	.include	"src/asm/include/equates.inc"

	.global	_asm_runProgram_run
	.type	_asm_runProgram_run, @function
	.global	_asm_runProgram_main
	.type	_asm_runProgram_main, @function

	.extern	_asm_utils_clrScrnAndUsedRAM
	.extern	_asm_oslaunch_dispatch
	.extern	_os_launch_return_os_flag

;--------------------------------------
	.section	.text

; C entry: (sp+0) ret, (sp+3) name*, (sp+6) var_type, (sp+9) kind, (sp+12) prefs*
_asm_runProgram_run:
	ld	iy, 0
	add	iy, sp
	ld	a, (iy + 6)		; var_type
	ld	hl, ti.OP1
	ld	(hl), a
	inc	hl
	ld	de, (iy + 3)		; name*
	ex	de, hl
	ld	bc, 8
	ldir
	xor	a, a
	ld	(de), a
	ld	a, (iy + 9)		; program_kind
	; NEW: shell-side launch: clear the OS-launch return flag (osLaunch.s
	; sets it only when the OS homescreen hook intercepted the command).
	ld	hl, _os_launch_return_os_flag
	ld	(hl), 0
	; release Flash App C stack before InsertMem / OS work
	ld	sp, (ti.onSP)
	push	af			; kind stays on OS stack
	call	ti.ResetStacks
	pop	af
	ld	iy, ti.flags
	jp	_asm_runProgram_main

; OP1 = type + name; A = program_kind
_asm_runProgram_main:
	push	af
main.debounce:
	call	ti.GetCSC
	or	a, a
	jr	nz, main.debounce

	; clear any previous asm program from userMem
	ld	de, (ti.asm_prgm_size)
	or	a, a
	sbc	hl, hl
	ld	(ti.asm_prgm_size), hl
	ld	hl, ti.userMem
	call	ti.DelMem

	; find the variable: OP1 already set
	call	ti.ChkFindSym
	jp	c, main.notFound
	; HL = VAT, DE = data ptr (RAM) / VAT entry (archived - fixed below)
	; NEW archived fix (the CEaShell _asm_utils_findVar idiom): for an
	; archived variable ChkFindSym's DE points at the VAT entry, not the
	; data, so the size read below used to read VAT bytes. RAM vars already
	; give DE = data ptr (2-byte size field); only archived pointers are
	; adjusted. Same result as call _asm_utils_findVar + 4 upstream, but
	; self-contained so the notFound check above is kept.
	call	ti.ChkInRam
	jr	z, main.ramPtr
	ld	hl, 10
	add	hl, de
	ld	a, c
	ld	bc, 0
	ld	c, a
	add	hl, bc
	ex	de, hl
main.ramPtr:
	ld	(ti.asm_data_ptr1), de
	ld	bc, 0
	ld	a, (de)
	ld	c, a
	inc	de
	ld	a, (de)
	ld	b, a			; BC = size
	pop	af			; program_kind
	cp	a, 4
	jp	z, main.runAsmCall
	or	a, a
	jr	z, main.runASM
	dec	a
	jr	z, main.runASM		; C
	dec	a
	jp	z, main.runBasic	; BASIC
	dec	a
	jp	nz, main.runBasic	; fallback
	; ICE falls through as native
main.runASM:
	inc	de			; skip size bytes already consumed... 
	; data ptr was at size; we read 2 size bytes so DE points at payload
	; For EF 7B programs the first 2 payload bytes are the header; skip them
	; when loading? CEaShell skips size+$EF7B (4 bytes from original data).
	; Our DE is already past the 2-byte size. Skip EF 7B (2 more).
	push	bc
	ld	hl, 128
	add	hl, bc
	call	ti.EnoughMem
	pop	hl			; HL = size
	ld	a, ti.E_Memory
	jp	c, main.error
	ld	(ti.asm_prgm_size), hl
	push	hl
	push	hl
	ld	de, ti.userMem
	call	ti.InsertMem
	pop	bc
	xor	a, a
	ld	hl, ti.userMem
	push	hl
	call	ti.MemSet
	ld	hl, (ti.asm_data_ptr1)
	inc	hl			; skip size lo
	inc	hl			; skip size hi
	inc	hl			; skip EF
	inc	hl			; skip 7B
	pop	de			; userMem
	pop	bc			; size
	; loaded size includes the 2-byte header we skipped; adjust
	dec	bc
	dec	bc
	ld	a, b
	or	a, c
	jr	z, main.ASMLoaded
	ldir
main.ASMLoaded:
	call	ti.DisableAPD
	set	ti.appAutoScroll, (iy + ti.appFlags)
	call	main.vectorsSetup
	ld	hl, main.return
	push	hl
	jp	ti.userMem

; Z80-mode: build Asm(prgmNAME) as a temporary protected program, ParseInp it.
main.runAsmCall:
	ld	hl, ti.pixelShadow
	ld	(hl), $BB		; 2-byte token prefix
	inc	hl
	ld	(hl), $6A		; Asm(  (CEaShell uses $6A)
	inc	hl
	ld	(hl), $5F		; prgm
	inc	hl
	ld	de, ti.OP1 + 1
	ld	b, 8
main.asmCall.copyName:
	ld	a, (de)
	or	a, a
	jr	z, main.asmCall.nameDone
	ld	(hl), a
	inc	hl
	inc	de
	djnz	main.asmCall.copyName
main.asmCall.nameDone:
	ld	(hl), $11		; )
	inc	hl
	ld	de, ti.pixelShadow
	or	a, a
	sbc	hl, de			; HL = line length
	push	hl
	ld	de, 128
	add	hl, de
	call	ti.EnoughMem
	pop	hl
	ld	a, ti.E_Memory
	jp	c, main.error
	push	hl
	ld	hl, main.tempName
	call	ti.Mov9ToOP1
	pop	hl
	push	hl
	call	ti.CreateProtProg
	inc	de
	inc	de
	pop	bc
	ld	hl, ti.pixelShadow
	ldir
	jp	main.loadComplete

main.runBasic:
	; DE points at payload (past size). Check for Asm84CEPrgm hex-text squish.
	inc	de			; was at size; we already read size into BC
	; actually after reading size, DE is at first payload byte
	; re-sync: asm_data_ptr1 is start of data (size field)
	ld	hl, (ti.asm_data_ptr1)
	inc	hl
	inc	hl			; HL = payload
	ex	de, hl
	ld	a, (de)
	cp	a, ti.tExtTok
	jr	nz, main.noSquish
	inc	de
	ld	a, (de)
	cp	a, ti.tAsm84CePrgm
	jr	z, main.squish
	dec	de
main.noSquish:
	call	ti.ChkInRam
	jr	z, main.loadComplete.ready
	; archived: copy to a temp prot program
	push	de
	push	bc
	ld	hl, 128
	add	hl, bc
	call	ti.EnoughMem
	jr	nc, main.arcOk
	pop	bc
	pop	de
	ld	a, ti.E_Memory
	jp	main.error
main.arcOk:
	ld	hl, main.tempName
	call	ti.Mov9ToOP1
	pop	hl			; size
	push	hl
	call	ti.CreateProtProg
	inc	de
	inc	de
	pop	bc
	pop	hl			; payload src
	ld	a, b
	or	a, c
	jr	z, main.loadComplete
	ldir
main.loadComplete:
	call	ti.OP4ToOP1
main.loadComplete.ready:
	call	ti.RunIndicOn
	call	ti.EnableAPD
	set	ti.progExecuting, (iy + ti.newDispF)
	set	ti.cmdExec, (iy + ti.cmdFlags)
	call	main.vectorsSetup
	ei
	ld	hl, main.return
	push	hl
	jp	ti.ParseInp

; Hex-text Asm84CEPrgm: convert ASCII hex to bytes, run as native
main.squish:
	ex	de, hl
	inc	hl
	dec	bc
	dec	bc
	ld	a, b
	or	a, c
	jr	z, main.syntaxErr
	push	bc
	push	bc
main.loopNewlines:
	ld	a, (hl)
	cp	a, ti.tEnter
	jr	nz, main.notNewline
	pop	de
	dec	de
	push	de
main.notNewline:
	inc	hl
	dec	bc
	ld	a, b
	or	a, c
	jr	nz, main.loopNewlines
	pop	hl
	bit	0, l
	jr	nz, main.syntaxErrPop
	ld	a, h
	or	a, l
	jr	nz, main.notEmpty
	pop	hl
main.syntaxErr:
	ld	a, ti.E_Syntax
	jp	main.error
main.syntaxErrPop:
	pop	hl
	jr	main.syntaxErr
main.notEmpty:
	srl	h
	rr	l			; byte count = nibble count / 2
	push	hl
	ld	de, 128
	add	hl, de
	call	ti.EnoughMem
	pop	hl
	ld	a, ti.E_Memory
	jp	c, main.error
	ld	(ti.asm_prgm_size), hl
	push	hl
	ld	de, ti.userMem
	call	ti.InsertMem
	pop	bc
	xor	a, a
	ld	hl, ti.userMem
	push	hl
	call	ti.MemSet
	ld	hl, (ti.asm_data_ptr1)
	inc	hl
	inc	hl
	inc	hl
	inc	hl			; past size + EF 7A
	pop	de
	pop	bc			; original nibble stream size
main.loopLoad:
	ld	a, b
	or	a, c
	jp	z, main.ASMLoaded
	ld	a, (hl)
	inc	hl
	dec	bc
	cp	a, ti.tEnter
	jr	z, main.loopLoad
	push	de
	call	main.tokHex
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ld	e, a
	ld	a, (hl)
	inc	hl
	dec	bc
	call	main.tokHex
	add	a, e
	pop	de
	ld	(de), a
	inc	de
	jr	main.loopLoad

main.tokHex:
	cp	a, ti.t0
	jr	c, main.tokHexErr
	cp	a, ti.t9 + 1
	jr	c, main.tokHexDigit
	cp	a, ti.tA
	jr	c, main.tokHexErr
	cp	a, ti.tF + 1
	jr	nc, main.tokHexErr
	sub	a, ti.t0
	sub	a, 7
	ret
main.tokHexDigit:
	sub	a, ti.t0
	ret
main.tokHexErr:
	pop	hl
	pop	hl
	ld	a, ti.E_Syntax
	jp	main.error

main.notFound:
	pop	af
	ld	a, ti.E_Undefined
main.error:
	ld	(ti.errNo), a
	call	main.lcdNormal
main.errWait:
	call	ti.GetCSC
	cp	a, ti.skEnter
	jp	z, _asm_oslaunch_dispatch	; NEW: OS launch -> homescreen,
	cp	a, ti.sk1			; shell launch -> restart the shell
	jp	z, _asm_oslaunch_dispatch
	jr	main.errWait

;---------- return from program ----------
main.return:
	ld	iy, ti.flags
	ei
	call	ti.PopErrorHandler
	xor	a, a
	ld	hl, ti.mpTmrCtrl + 1
	ld	a, (hl)
	and	a, l
	ld	(hl), a
	res	ti.progExecuting, (iy + ti.newDispF)
	res	ti.cmdExec, (iy + ti.cmdFlags)
	res	ti.textInverse, (iy + ti.textFlags)
	res	ti.onInterrupt, (iy + ti.onFlags)
	call	ti.ReloadAppEntryVecs
	call	ti.DeleteTempPrograms
	call	ti.CleanAll
	call	ti.ReloadAppEntryVecs
	call	ti.ForceFullScreen
	ld	de, (ti.asm_prgm_size)
	or	a, a
	sbc	hl, hl
	ld	(ti.asm_prgm_size), hl
	ld	hl, ti.userMem
	call	ti.DelMem
main.retDebounce:
	call	ti.GetCSC
	or	a, a
	jr	nz, main.retDebounce
	xor	a, a
	ld	(ti.kbdGetKy), a
	res	ti.monAbandon, (iy + ti.monFlags)
	jp	_asm_oslaunch_dispatch	; NEW: OS launch -> homescreen, else shell

;---------- app vectors + error handler ----------
main.vectorsSetup:
	call	main.lcdNormal
	ld	a, $25
	ld	($D02687), a
	xor	a, a
	ld	(ti.menuCurrent), a
	ld	(ti.appErr1), a
	ld	(ti.kbdGetKy), a
	ld	hl, main.vectors
	call	ti.AppInit
	call	_asm_utils_clrScrnAndUsedRAM
	ld	hl, main.return
	jp	ti.PushErrorHandler

main.vectors:
	.d24	main.vecRet
	.d24	ti.SaveCmdShadow
	.d24	main.vecPutAway
	.d24	main.vecRestore
	.d24	main.vecRet
	.d24	main.vecRet
main.vecRet:
	ret
main.vecPutAway:
	xor	a, a
	ld	(ti.currLastEntry), a
	bit	appInpPrmptInit, (iy + ti.apiFlg2)
	call	z, ti.ForceFullScreen
	call	ti.ReloadAppEntryVecs
	call	ti.PutAway
	ld	b, 0
	ret
main.vecRestore:
	call	ti.HomeUp
	call	ti.ClrScrn
	jp	ti.RStrShadow

main.lcdNormal:
	; minimal: ensure 8bpp is left for the shell to re-init
	ret

; Temporary protected program name used for Asm( and archived BASIC copies
	.section	.rodata
main.tempName:
	.db	ti.ProgObj, "ZATRUN", 0, 0