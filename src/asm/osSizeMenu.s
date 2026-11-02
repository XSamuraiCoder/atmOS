;--------------------------------------
;
; atmOS - osSizeMenu.s
; Shows program sizes in PRGM EXEC and Flash App sizes in the Apps menu.
;
; Technique adapted from CEaShell's hooks.s and prgmMenuHook.s.
; Copyright 2022-2026 RoccoLox Programs and TIny_Hacker.
; SPDX-License-Identifier: GPL-3.0-only
;
; How it works (all of it CEaShell's empirically established, shipped
; behavior - see their prgmMenuHook.s):
;   - The OS GetCSC hook is called with A = $1A whenever an OS menu has
;     been redrawn (the event CEaShell uses to draw its menu icons).
;   - In PRGM EXEC, ti.userMenuSA holds the first visible program. Its VAT
;     entry and later entries are walked in menu order.
;   - In the Apps menu, ti.userMenuSA holds the first visible app name.
;     FindAppStart and FindAppCustom walk the OS app directory.
;   - Each entry's data pointer is read straight from its VAT entry
;     (CEaShell's icons/getPrgmSize pattern: NL/page/pointer at T-6..
;     T-3, archived entries get de += 10 + NL) and the size shown is
;     the 2-byte data length + NL + 9, formatted with one decimal KB
;     (dropped when zero) and drawn with VPutS at x = 152, rows every
;     20 px starting at y = 58 - CEaShell's icon row addresses.
;   - PRGM EXEC is redrawn on each $1A event. The Apps menu is latched
;     until the next key event to avoid repeated text painting.
;
; State lives in main.c's .bss, like the osLaunch.s hook state: app RAM
; survives after the shell exits (the hook keeps working in the OS) and
; is zeroed at every app start. The hook stays installed after atmOS
; exits, so the EXEC menu keeps showing sizes until a RAM reset.
;
; Flash App build only: add this file to the Makefile's asm sources.
;
;--------------------------------------
	.assume	adl=1
	.include	"src/asm/include/equates.inc"

	.global	_asm_osSizeMenu_install
	.extern	_os_size_menu_row
	.extern	_os_size_menu_vat
	.extern	_os_size_menu_tenth
	.extern	_os_size_menu_begun
	.extern	_os_size_menu_buf

;---------------- install ----------------
; Mirrors _asm_oslaunch_install: force-install (no chaining), same as
; the project's other hooks.
_asm_osSizeMenu_install:
	ld	iy, ti.flags
	; CEaShell keeps ti.catalog1HookPtr nonzero the entire time their
	; GetCSC hook is active, and their getCSCHook does nothing when it
	; is 0 - mirror that ritual exactly: store a nonzero value, then
	; clear the active bit so the value is never called as a real
	; catalog hook (their _asm_hooks_installGetCSCHook does this)
	ld	hl, 1
	ld	(ti.catalog1HookPtr), hl
	call	ti.ClrCatalog1Hook
	ld	hl, osSizeMenu_hook
	jp	ti.SetGetCSCHook

;---------------- the hook ----------------
	.section	.text

osSizeMenu_hook:
	db	$83
	push	af
	cp	a, $1A			; menu redrawn event
	jr	z, osSizeMenu_update
	pop	af			; any other event (a key, the ON
	push	af
	xor	a, a
	ld	(_os_size_menu_flags), a	; CEaShell clears its draw latch on keys
	pop	af
	cp	a, $1B
	ret	nz			; $1A redraws anyway
	or	a, a
	ld	a, b			; return the ON key state (b, usually 0)
	ret				; $1B is sk5 in the GetCSC table:
					; returning it made the OS type 5s!

osSizeMenu_update:
	push	bc
	push	de
	push	hl
	push	ix
	ld	a, (ti.menuCurrent)
	cp	a, ti.mProgramHome
	jr	z, osSizeMenu_execMenu
	cp	a, ti.mApps
	jr	nz, osSizeMenu_pass
	ld	a, (_os_size_menu_flags)
	bit	0, a
	jr	nz, osSizeMenu_pass
	set	0, a
	ld	(_os_size_menu_flags), a
	ld	a, (ti.menuNumItems)
	or	a, a
	jr	z, osSizeMenu_pass
	call	osSizeMenu_drawApps
	jr	osSizeMenu_pass

osSizeMenu_execMenu:
	ld	a, (ti.menuCurrentSub)
	cp	a, ti.mPrgm_Run		; EXEC tab only (see header)
	jr	nz, osSizeMenu_pass
	ld	a, (ti.menuNumItems)
	or	a, a
	jr	z, osSizeMenu_pass		; empty menu
	call	osSizeMenu_drawRows	; redraw every $1A: the text always
					; lands on top of the OS's own paint
osSizeMenu_pass:
	pop	ix
	pop	hl
	pop	de
	pop	bc
	pop	af
	cp	a, $1B
	ret	nz
	or	a, a
	ld	a, b			; same ON-event convention as above
	ret

; Flash Apps live in the app directory, not the VAT. Find the first visible
; app by name, then use the OS app iterator to follow the displayed order.
osSizeMenu_drawApps:
	ld	hl, ti.userMenuSA
	push	hl
	call	ti.Mov9ToOP1
	pop	hl
	inc	hl
	call	ti.FindAppStart
	ret	c
	ex	de, hl
	ld	(_os_size_menu_vat), de
	xor	a, a
	ld	(_os_size_menu_row), a

osSizeMenu_appLoop:
	ld	a, (_os_size_menu_row)
	cp	a, 9
	ret	z
	call	osSizeMenu_appSize
	call	osSizeMenu_formatAppKB
	call	osSizeMenu_drawText
	ld	hl, _os_size_menu_row
	inc	(hl)
	ld	de, (_os_size_menu_vat)
	ld	a, $0D
	call	ti.FindAppCustom
	ret	c
	ld	(_os_size_menu_vat), de
	jr	osSizeMenu_appLoop

; App start address in _os_size_menu_vat; return its size in BC kilobytes.
osSizeMenu_appSize:
	ld	hl, (_os_size_menu_vat)
	push	hl
	call	ti.NextFieldFromType
	call	ti.NextFieldFromType
	pop	de
	or	a, a
	sbc	hl, de
	inc	hl
	inc	hl
	inc	hl				; CEaShell's app-size convention
	ld	bc, 0
	ld	de, 1024
osSizeMenu_appKBLoop:
	or	a, a
	sbc	hl, de
	jr	c, osSizeMenu_appKBDone
	inc	bc
	jr	osSizeMenu_appKBLoop
osSizeMenu_appKBDone:
	add	hl, de
	ret

; bc = integer kilobytes -> "1234KB" at the common output buffer.
osSizeMenu_formatAppKB:
	xor	a, a
	ld	(_os_size_menu_begun), a
	ld	de, _os_size_menu_buf
	push	de
	ld	l, 0
osSizeMenu_appThousands:
	ld	a, c
	sub	a, 232
	ld	e, a
	ld	a, b
	sbc	a, 3
	jr	c, osSizeMenu_appThousandsDone
	ld	c, e
	ld	b, a
	inc	l
	jr	osSizeMenu_appThousands
osSizeMenu_appThousandsDone:
	ld	a, l
	ld	(_os_size_menu_tenth), a
	ld	l, 0
osSizeMenu_appHundreds:
	ld	a, c
	sub	a, 100
	ld	e, a
	ld	a, b
	sbc	a, 0
	jr	c, osSizeMenu_appHundredsDone
	ld	c, e
	ld	b, a
	inc	l
	jr	osSizeMenu_appHundreds
osSizeMenu_appHundredsDone:
	ld	b, 0
osSizeMenu_appTens:
	ld	a, c
	sub	a, 10
	jr	c, osSizeMenu_appTensDone
	ld	c, a
	inc	b
	jr	osSizeMenu_appTens
osSizeMenu_appTensDone:
	pop	de
	ld	a, (_os_size_menu_tenth)
	call	osSizeMenu_emit
	ld	a, l
	call	osSizeMenu_emit
	ld	a, b
	call	osSizeMenu_emit
	ld	a, c
	call	osSizeMenu_emit
	ld	a, (_os_size_menu_begun)
	or	a, a
	jr	nz, osSizeMenu_appUnit
	ld	a, '0'
	ld	(de), a
	inc	de
osSizeMenu_appUnit:
	ld	a, 'K'
	ld	(de), a
	inc	de
	ld	a, 'B'
	ld	(de), a
	inc	de
	xor	a, a
	ld	(de), a
	ret

;---------------- draw the visible rows ----------------
osSizeMenu_drawRows:
	ld	hl, ti.userMenuSA	; OP1 name of the first visible item
	call	ti.Mov9ToOP1
	call	ti.ChkFindSym
	ret	c			; anchor not found: nothing to draw
	ld	(_os_size_menu_vat), hl	; hl = its VAT entry (type byte)
	xor	a, a
	ld	(_os_size_menu_row), a

osSizeMenu_rowLoop:
	ld	hl, (_os_size_menu_vat)
	ld	bc, (ti.pTemp)
	or	a, a
	sbc	hl, bc
	ret	z
	ret	c
	add	hl, bc
	ld	a, (hl)
	and	a, $1F			; type (bits 7-5 hold the name length)
	cp	a, ti.ProgObj
	jr	z, osSizeMenu_include
	cp	a, ti.ProtProgObj
	jp	nz, osSizeMenu_next

osSizeMenu_include:
	; the OS menu hides its temporary programs (prgm#, prgm!): their
	; name's last character is < 'A' (CEaShell's check)
	ld	hl, (_os_size_menu_vat)
	ld	bc, -7
	add	hl, bc
	ld	a, (hl)			; last name character
	cp	a, ti.tA
	jp	c, osSizeMenu_next
	; only 9 rows fit on screen
	ld	a, (_os_size_menu_row)
	cp	a, 9
	ret	z
	; ---- size straight from the VAT entry (CEaShell's icons/
	;      getPrgmSize pattern - no per-row ChkFindSym) ----
	ld	hl, (_os_size_menu_vat)
	ld	bc, -6
	add	hl, bc			; hl -> name length byte (T-6)
	ld	c, (hl)			; c = NL
	inc	hl
	ld	a, (hl)			; a = page byte
	ld	(ti.scrapMem + 2), a
	ld	de, (ti.scrapMem)
	inc	hl
	ld	d, (hl)			; data pointer high
	inc	hl
	ld	e, (hl)			; data pointer low
	call	ti.ChkInRam
	jr	z, osSizeMenu_dataOK
	push	hl
	ld	hl, 10			; archived: skip the flash record header
	add	hl, de
	ld	a, c			; a = NL (c survives ChkInRam)
	ld	bc, 0
	ld	c, a
	add	hl, bc
	ex	de, hl			; de += 10 + NL
	pop	hl

osSizeMenu_dataOK:
	; de -> program data; its first 2 bytes are the data length
	ld	a, (de)
	ld	l, a
	inc	de
	ld	a, (de)
	ld	h, a			; hl = raw data length
	ld	a, c			; c = NL
	add	a, 9			; CEaShell's convention: the OS style
	ld	c, a			; size is length + NL + 9
	ld	b, 0
	add	hl, bc			; hl = size
	ld	b, h
	ld	c, l			; bc = size in bytes
	call	osSizeMenu_format
	call	osSizeMenu_drawText
	ld	hl, _os_size_menu_row
	inc	(hl)

osSizeMenu_next:
	; next entry's type byte = T - 7 - NL
	ld	hl, (_os_size_menu_vat)
	ld	bc, -6
	add	hl, bc
	ld	a, (hl)			; NL
	add	a, 7
	ld	c, a
	ld	b, 0
	ld	hl, (_os_size_menu_vat)
	or	a, a
	sbc	hl, bc
	ld	(_os_size_menu_vat), hl
	; The next iteration checks pTemp before dereferencing this entry.
	jp	osSizeMenu_rowLoop

;---------------- size formatting ----------------
; bc = size (0..65535) -> string at _os_size_menu_buf:
;   < 1000:   "512B" (no leading zeros, lone "0" for size 0)
;   >= 1000:  "9.4KB" (the decimal is dropped when zero)
; Uses balanced DE saves around arithmetic that needs DE as scratch, plus
; the _os_size_menu_begun/_tenth scratch bytes.

osSizeMenu_format:
	xor	a, a
	ld	(_os_size_menu_begun), a	; leading zero suppression flag
	ld	de, _os_size_menu_buf
	ld	a, b
	cp	a, 4
	jr	nc, osSizeMenu_kbPath
	cp	a, 3				; b == 3: size 768..1023
	jr	nz, osSizeMenu_bytesPath
	ld	a, c
	cp	a, 232			; c >= 232: size >= 1000
	jr	nc, osSizeMenu_kbPath		; (hundreds digit would hit 10)

; ---- bytes path: bc = size, 0..999 ----
osSizeMenu_bytesPath:
	push	de			; hLoop uses e as its subtraction scratch
	ld	l, 0					; l = hundreds digit
osSizeMenu_hLoop:
	ld	a, c
	sub	a, 100
	ld	e, a
	ld	a, b
	sbc	a, 0
	jr	c, osSizeMenu_hDone
	ld	c, e
	ld	b, a
	inc	l
	jr	osSizeMenu_hLoop
osSizeMenu_hDone:
	pop	de
	ld	b, 0					; b = tens digit
osSizeMenu_tLoop:
	ld	a, c
	sub	a, 10
	jr	c, osSizeMenu_tDone
	ld	c, a
	inc	b
	jr	osSizeMenu_tLoop
osSizeMenu_tDone:
	ld	a, l
	call	osSizeMenu_emit
	ld	a, b
	call	osSizeMenu_emit
	ld	a, c
	call	osSizeMenu_emit
	ld	a, (_os_size_menu_begun)	; all-zero value: print a lone "0"
	or	a, a
	jr	nz, osSizeMenu_bytesEnd
	ld	a, '0'
	ld	(de), a
	inc	de
osSizeMenu_bytesEnd:
	ld	a, 'B'
	ld	(de), a
	inc	de
	xor	a, a
	ld	(de), a
	ret

; ---- KB path: size >= 1000 ----
osSizeMenu_kbPath:
	; tenth = ((size & 1023) * 10) >> 10, computed as (x * 5) >> 9
	push	de
	ld	a, b
	and	a, 3
	ld	h, a
	ld	l, c					; hl = size & 1023
	ld	e, l
	ld	d, h
	add	hl, hl
	add	hl, hl
	add	hl, de				; hl = (size & 1023) * 5
	ld	a, h
	srl	a				; a = tenth (0..9)
	ld	(_os_size_menu_tenth), a
	pop	de
	; whole KB = b >> 2 (0..63), printed without leading zeros
	ld	a, b
	srl	a
	srl	a				; a = whole KB
	cp	a, 10
	jr	c, osSizeMenu_kbOne			; single digit
	ld	c, a					; two digits: tens by subtraction
	ld	b, '0' - 1
osSizeMenu_kbTens:
	inc	b
	ld	a, c
	sub	a, 10
	jr	c, osSizeMenu_kbTensDone
	ld	c, a
	jr	osSizeMenu_kbTens
osSizeMenu_kbTensDone:
	ld	a, b
	ld	(de), a
	inc	de
	ld	a, c
osSizeMenu_kbOne:
	add	a, '0'
	ld	(de), a
	inc	de
	ld	a, (_os_size_menu_tenth)	; decimal part, dropped when zero
	or	a, a
	jr	z, osSizeMenu_kbUnit
	ld	a, '.'
	ld	(de), a
	inc	de
	ld	a, (_os_size_menu_tenth)
	add	a, '0'
	ld	(de), a
	inc	de
osSizeMenu_kbUnit:
	ld	a, 'K'
	ld	(de), a
	inc	de
	ld	a, 'B'
	ld	(de), a
	inc	de
	xor	a, a
	ld	(de), a
	ret

; emits the digit in a: skipped while it is a leading zero; sets the
; begun flag once the first digit went out. de advances.
osSizeMenu_emit:
	or	a, a
	jr	nz, osSizeMenu_emitNz
	ld	a, (_os_size_menu_begun)
	or	a, a
	ret	z					; leading zero: emit nothing
	ld	a, '0'
	ld	(de), a
	inc	de
	ret
osSizeMenu_emitNz:
	add	a, '0'
	ld	(de), a
	inc	de
	ld	a, 1
	ld	(_os_size_menu_begun), a
	ret

;---------------- draw one row's string ----------------
; Rows and column follow CEaShell's EXEC-menu icon addresses:
; x = 152, y = 58 + 20 * row (text top + 4 to center the ~10 px font).
; Params are set as the LAST step before VPutS so nothing clobbers them.
osSizeMenu_drawText:
	ld	a, (_os_size_menu_row)
	ld	l, a
	ld	h, 20
	mlt	hl			; hl = 20 * row
	ld	de, 62
	add	hl, de
	ld	a, l
	ld	(ti.penRow), a
	ld	de, 152
	ld.sis	(ti.penCol & $FFFF), de
	ld	de, $0000		; black text remains visible on the white row background
	ld.sis	(ti.drawFGColor & $FFFF), de
	ld	de, $FFFF		; white: matches the menu rows
	ld.sis	(ti.drawBGColor & $FFFF), de
	ld	hl, _os_size_menu_buf
	jp	ti.VPutS
