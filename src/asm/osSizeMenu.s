;--------------------------------------
;
; atmOS - osSizeMenu.s
; Shows size of the currently selected program in PRGM EXEC,
; and app sizes in the Apps menu.
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
	.extern	_os_size_menu_flags

_asm_osSizeMenu_install:
	ld	iy, ti.flags
	ld	hl, osSizeMenu_hook
	jp	ti.SetGetCSCHook

	.section	.text
osSizeMenu_hook:
	db	$83
	push	af
	cp	a, $1A
	jr	z, osSizeMenu_update
	pop	af
	push	af			; keep the key for the tests below
	; Only keys that close the menu or change tab hide the size. (Other nonzero values reach this
	; hook too - a hide on "any key" erased the size right after it was drawn.)
	cp	a, ti.skEnter
	jr	z, osSizeMenu_keyHide
	cp	a, ti.skClear
	jr	z, osSizeMenu_keyHide
	cp	a, ti.skLeft
	jr	z, osSizeMenu_keyHide
	cp	a, ti.skRight
	jr	z, osSizeMenu_keyHide
	cp	a, ti.skMode
	jr	z, osSizeMenu_keyHide
	cp	a, ti.skPrgm
	jr	z, osSizeMenu_keyHide
	cp	a, 39			; Apps (this toolchain has no ti.skApps)
	jr	z, osSizeMenu_keyHide
	jr	osSizeMenu_noKey
osSizeMenu_keyHide:
	ld	a, (_os_size_menu_buf + 15)
	or	a, a
	jr	z, osSizeMenu_noKey	; no size on screen
	call	osSizeMenu_hideSlot
osSizeMenu_noKey:
	xor	a, a
	ld	(_os_size_menu_flags), a
	pop	af
	cp	a, $1B
	ret	nz
	or	a, a
	ld	a, b
	ret

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
	cp	a, ti.mPrgm_Run
	jr	nz, osSizeMenu_pass
	ld	a, (ti.menuNumItems)
	or	a, a
	jr	z, osSizeMenu_pass
	call	osSizeMenu_drawSelected
osSizeMenu_pass:
	pop	ix
	pop	hl
	pop	de
	pop	bc
	pop	af
	cp	a, $1B
	ret	nz
	or	a, a
	ld	a, b
	ret

; ---- Apps menu (unchanged) ----
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
	inc	hl
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

; ---- Selected program size (EXEC) ----
; ti.progCurrent is the hovered program: its size comes from ChkFindSym on that name (this part
; is what showed the right "9.4KB"). Which ROW it is on is read off the screen: the OS draws the
; selected row's "N:" label as a black box at x = 0, every other row's label has a white
; background (see osSizeMenu_hoverRow). The text is built first and padded with spaces to a fixed
; width, then painted with ONE VPutS (no clear + draw flash). When the hover moves, only the
; previous row's slot is blanked (the OS does not repaint x = 152 on a cursor move). The OS's pen
; position and text colors are saved and restored around every paint.
;
; Scratch in _os_size_menu_buf (16 bytes; formatted text never exceeds 9):
;   buf + 15 = last painted row + 1 (0 = none)
osSizeMenu_drawSelected:
	call	osSizeMenu_hoverRow		; a = hovered row (0-8), carry = none found
	jp	c, osSizeMenu_noSel
	ld	(_os_size_menu_row), a
	ld	a, (_os_size_menu_buf + 15)	; last painted row + 1
	or	a, a
	jr	z, osSizeMenu_selSize
	dec	a
	ld	hl, _os_size_menu_row
	cp	a, (hl)
	jr	z, osSizeMenu_selSize		; same row: the new text covers it
	call	osSizeMenu_paintBlank		; the hover moved: blank the old slot (a = old row)
osSizeMenu_selSize:
	ld	hl, ti.progCurrent
	call	ti.Mov9ToOP1
	call	ti.ChkFindSym
	jr	c, osSizeMenu_selFail
	ld	a, (hl)
	and	a, $1F
	cp	a, ti.ProgObj
	jr	z, osSizeMenu_selOk
	cp	a, ti.ProtProgObj
	jr	nz, osSizeMenu_selFail
osSizeMenu_selOk:
	ld	bc, -6
	add	hl, bc
	ld	c, (hl)				; c = NL
	inc	hl
	ld	a, (hl)				; page byte
	ld	(ti.scrapMem + 2), a
	ld	de, (ti.scrapMem)
	inc	hl
	ld	d, (hl)				; data pointer high
	inc	hl
	ld	e, (hl)				; data pointer low
	push	bc
	call	ti.ChkInRam
	jr	z, osSizeMenu_selData
	ld	hl, 10				; archived: skip the flash record header
	add	hl, de
	ld	a, c
	ld	bc, 0
	ld	c, a
	add	hl, bc
	ex	de, hl				; de += 10 + NL
osSizeMenu_selData:
	pop	bc
	ld	a, (de)
	ld	l, a
	inc	de
	ld	a, (de)
	ld	h, a				; hl = raw data length
	ld	a, c
	add	a, 9				; size = length + NL + 9
	ld	c, a
	ld	b, 0
	add	hl, bc
	ld	b, h
	ld	c, l				; bc = size in bytes
	call	osSizeMenu_format
	call	osSizeMenu_pad
	jr	osSizeMenu_selPaint
osSizeMenu_selFail:
	ld	a, (_os_size_menu_row)		; lookup failed: show nothing rather than a wrong size
	call	osSizeMenu_paintBlank
	xor	a, a
	ld	(_os_size_menu_buf + 15), a
	ret
osSizeMenu_selPaint:
	ld	a, (_os_size_menu_row)
	call	osSizeMenu_paintAt
	ld	a, (_os_size_menu_row)
	inc	a
	ld	(_os_size_menu_buf + 15), a
	ret

; -> a = row (0-8) whose "N:" label is drawn selected (black at x = 2), carry set when none.
; 16 bpp VRAM: pixel (2, 64 + 20 * row) is at $D40000 + (y * 320 + 2) * 2 = $D4A004 + row * $3200.
osSizeMenu_hoverRow:
	ld	hl, $D4A004
	ld	bc, $3200
	ld	d, 0
osSizeMenu_hoverLoop:
	ld	a, (hl)
	inc	hl
	or	a, (hl)				; both bytes zero = black
	dec	hl
	jr	z, osSizeMenu_hoverFound
	add	hl, bc
	inc	d
	ld	a, d
	cp	a, 9
	jr	nz, osSizeMenu_hoverLoop
	scf
	ret
osSizeMenu_hoverFound:
	ld	a, d
	or	a, a				; carry clear
	ret

; No hovered program on screen: blank the last painted slot, if any.
osSizeMenu_noSel:
	ld	a, (_os_size_menu_buf + 15)
	or	a, a
	ret	z
	dec	a
	call	osSizeMenu_paintBlank
	xor	a, a
	ld	(_os_size_menu_buf + 15), a
	ret

; Blank the painted slot from the hook's key path (keeps every register the caller may use).
osSizeMenu_hideSlot:
	push	bc
	push	de
	push	hl
	push	ix
	call	osSizeMenu_noSel
	pop	ix
	pop	hl
	pop	de
	pop	bc
	ret

; Text buffer = 8 spaces (DE -> start of the buffer, then falls into the padding loop).
osSizeMenu_blank:
	ld	de, _os_size_menu_buf
; DE -> end of the text: pad with spaces up to 14 characters, then the 0. (The built-in font's
; space is only ~2 px wide, so 8 characters did not cover the old text; buf + 15 is kept.)
osSizeMenu_pad:
	ld	hl, _os_size_menu_buf + 14
osSizeMenu_padLoop:
	or	a, a
	sbc	hl, de				; hl = end - de
	jr	z, osSizeMenu_padEnd
	jr	c, osSizeMenu_padEnd
	add	hl, de				; back to the end address
	ld	a, ' '
	ld	(de), a
	inc	de
	jr	osSizeMenu_padLoop
osSizeMenu_padEnd:
	xor	a, a
	ld	(de), a
	ret

; A = menu row: blank that row's size slot (a long run of spaces, wider than any size text).
osSizeMenu_paintBlank:
	ld	hl, osSizeMenu_spaces
	jr	osSizeMenu_paintStr
osSizeMenu_spaces:
	db	"                       ", 0	; 23 spaces

; A = menu row (0-8): paint _os_size_menu_buf at x = 152 on that row (y = 62 + 20 * row, the
; same as osSizeMenu_drawText), then restore the OS's pen position and text colors.
osSizeMenu_paintAt:
	ld	hl, _os_size_menu_buf
osSizeMenu_paintStr:
	ld	(_os_size_menu_vat), hl		; string to draw (scratch: the walk is gone)
	ld	l, a
	ld	h, 20
	mlt	hl
	ld	de, 62
	add	hl, de
	ld	a, l
	ld	(_os_size_menu_tenth), a	; y (scratch: free here, the format is finished)
	ld	a, (ti.penRow)
	push	af
	ld.sis	de, (ti.penCol & $FFFF)
	push	de
	ld.sis	de, (ti.drawFGColor & $FFFF)
	push	de
	ld.sis	de, (ti.drawBGColor & $FFFF)
	push	de
	ld	a, (_os_size_menu_tenth)
	ld	(ti.penRow), a
	ld	de, 152
	ld.sis	(ti.penCol & $FFFF), de
	ld	de, $0000
	ld.sis	(ti.drawFGColor & $FFFF), de
	ld	de, $FFFF
	ld.sis	(ti.drawBGColor & $FFFF), de
	ld	hl, (_os_size_menu_vat)
	call	ti.VPutS
	pop	de
	ld.sis	(ti.drawBGColor & $FFFF), de
	pop	de
	ld.sis	(ti.drawFGColor & $FFFF), de
	pop	de
	ld.sis	(ti.penCol & $FFFF), de
	pop	af
	ld	(ti.penRow), a
	ret

osSizeMenu_format:
	xor	a, a
	ld	(_os_size_menu_begun), a
	ld	de, _os_size_menu_buf
	ld	a, b
	cp	a, 4
	jr	nc, osSizeMenu_kbPath
	cp	a, 3
	jr	nz, osSizeMenu_bytesPath
	ld	a, c
	cp	a, 232
	jr	nc, osSizeMenu_kbPath
osSizeMenu_bytesPath:
	push	de
	ld	l, 0
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
	ld	b, 0
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
	ld	a, (_os_size_menu_begun)
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
osSizeMenu_kbPath:
	push	de
	ld	a, b
	and	a, 3
	ld	h, a
	ld	l, c
	ld	e, l
	ld	d, h
	add	hl, hl
	add	hl, hl
	add	hl, de
	ld	a, h
	srl	a
	ld	(_os_size_menu_tenth), a
	pop	de
	ld	a, b
	srl	a
	srl	a
	cp	a, 10
	jr	c, osSizeMenu_kbOne
	ld	c, a
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
	ld	a, (_os_size_menu_tenth)
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
osSizeMenu_emit:
	or	a, a
	jr	nz, osSizeMenu_emitNz
	ld	a, (_os_size_menu_begun)
	or	a, a
	ret	z
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

osSizeMenu_drawText:
	ld	a, (_os_size_menu_row)
	ld	l, a
	ld	h, 20
	mlt	hl
	ld	de, 62
	add	hl, de
	ld	a, l
	ld	(ti.penRow), a
	ld	de, 152
	ld.sis	(ti.penCol & $FFFF), de
	ld	de, $0000
	ld.sis	(ti.drawFGColor & $FFFF), de
	ld	de, $FFFF
	ld.sis	(ti.drawBGColor & $FFFF), de
	ld	hl, _os_size_menu_buf
	jp	ti.VPutS