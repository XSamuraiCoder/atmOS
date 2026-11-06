;--------------------------------------
;
; atmOS - osSizeMenu.s
; Shows size of the currently selected program in PRGM EXEC,
; and app sizes in the Apps menu.
;
; CHANGES vs the broken version:
;   1. osSizeMenu_walkTo now walks apps with ti.FindAppUp (name in OP1, carry = no more),
;      instead of ti.FindAppCustom with A = $0D (unknown convention).
;   2. The name-to-header lookup now uses ti.FindAppStart (HL = name pointer in,
;      HL = app header start in flash out), instead of ti.FindApp with OP1 (guessed).
;   3. osSizeMenu_hdrLen adds 269 to the 4-byte big-endian master field value, not 6:
;      total app size = master value + 269 (6 master + 4 signature field + 256 signature
;      + 3 size bytes), per WikiTI 84PCE:OS:Applications.
;   4. osSizeMenu_appSize bails out (carry, bc = 0) when the header check fails instead
;      of silently formatting hl = 0 as "0KB".
; STILL TO VERIFY: the Apps-menu row geometry assumed by osSizeMenu_hoverRow (first row y,
;   pitch, row count) and that menu row r = r-th app of the FindAppUp walk.
;
;--------------------------------------
	.assume	adl=1
	.include	"src/asm/include/equates.inc"
	.ifndef OSSIZEMENU_DEBUG
	.set	OSSIZEMENU_DEBUG, 0		; make DEBUG_SIZEMENU=1 defines it as 1; or set 1 here by hand
	.endif
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
	.if OSSIZEMENU_DEBUG
	call	osSizeMenu_debugClear		; the OS does not repaint x = 200 on the tab bar
	.endif
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
	.if OSSIZEMENU_DEBUG
	ld	a, 'U'				; trace: update event fired
	call	osSizeMenu_dbgTrace
	.endif
	ld	a, (ti.menuCurrent)
	cp	a, ti.mProgramHome
	jr	z, osSizeMenu_execMenu
	cp	a, ti.mApps
	jr	z, osSizeMenu_appsMenu
	.if OSSIZEMENU_DEBUG
	ld	a, 'N'				; trace: menuCurrent is neither PRGM nor Apps
	call	osSizeMenu_dbgTrace
	.endif
	jr	osSizeMenu_pass
osSizeMenu_appsMenu:
	ld	a, (ti.menuNumItems)
	or	a, a
	jr	nz, osSizeMenu_appsDraw
	.if OSSIZEMENU_DEBUG
	ld	a, '0'				; trace: Apps menu but menuNumItems = 0
	call	osSizeMenu_dbgTrace
	.endif
	jr	osSizeMenu_pass
osSizeMenu_appsDraw:
	call	osSizeMenu_drawSelectedApp
	jr	osSizeMenu_pass

osSizeMenu_execMenu:
	ld	a, (ti.menuCurrentSub)
	cp	a, ti.mPrgm_Run
	jr	nz, osSizeMenu_notExec
	ld	a, (ti.menuNumItems)
	or	a, a
	jr	z, osSizeMenu_notExec
	; The hook event can arrive after the screen already shows EDIT / NEW while menuCurrentSub
	; still says Run, so also require the EXEC tab to be drawn selected (black box at x = 1).
	call	osSizeMenu_execTabSel
	jr	c, osSizeMenu_notExec
	call	osSizeMenu_drawSelected
	jr	osSizeMenu_pass
osSizeMenu_notExec:
	.if OSSIZEMENU_DEBUG
	call	osSizeMenu_debugClear
	.endif
	call	osSizeMenu_noSel		; erase any size painted earlier (no-op if none)
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

; ---- Apps menu: size of the HOVERED app only (same scheme as the PRGM EXEC menu) ----
; The hovered row is read off the screen (osSizeMenu_hoverRow); the app on that row is the
; row-th entry of the FindAppUp walk (the first app is assumed to be the first visible one,
; i.e. the list is not scrolled). Same slot (x = 152), same padding / blanking / pen+color
; restore as EXEC.
; Scratch: buf + 13 = hovered row, buf + 15 = last painted row + 1.
osSizeMenu_drawSelectedApp:
	.if OSSIZEMENU_DEBUG
	ld	a, 'M'				; trace: Apps draw entered
	call	osSizeMenu_dbgTrace
	.endif
	call	osSizeMenu_hoverRow		; a = hovered row (0-8), carry = none found
	jr	nc, osSizeMenu_appHoverOk
	.if OSSIZEMENU_DEBUG
	ld	a, 'X'				; trace: no hovered row found on screen
	call	osSizeMenu_dbgTrace
	.endif
	jp	osSizeMenu_noSel
osSizeMenu_appHoverOk:
	ld	(_os_size_menu_buf + 13), a
	call	osSizeMenu_blankRows		; a = hovered row (clobbers _os_size_menu_vat: do it first)
	; Row 0 ("Finance...") is deliberately left blank. The first app in flash is what the
	; FindAppUp walk returns first, so walk index = menu row.
	ld	a, (_os_size_menu_buf + 13)
	or	a, a
	jr	nz, osSizeMenu_appNotFirst
	ld	(_os_size_menu_buf + 15), a	; a = 0: nothing painted
	ret
osSizeMenu_appNotFirst:
	; Row r (r >= 1) is the r-th app of the FindAppUp walk. FindAppUp leaves each name in
	; OP1; FindAppStart then turns the name pointer into the app header pointer.
	ld	a, (_os_size_menu_buf + 13)
	call	osSizeMenu_walkTo		; OP1 = the hovered app's name, carry = ran out
	jr	nc, osSizeMenu_appNameOk
	.if OSSIZEMENU_DEBUG
	ld	a, 'Z'				; trace: fewer apps than the hovered row
	call	osSizeMenu_dbgTrace
	.endif
	jp	osSizeMenu_noSel
osSizeMenu_appNameOk:
	ld	hl, ti.OP1+1			; HL -> name (as ti.FindAppStart expects)
	call	ti.FindAppStart		; HL = app header start in flash, carry = not found
	jr	nc, osSizeMenu_appFound
	.if OSSIZEMENU_DEBUG
	ld	a, 'F'				; trace: FindAppStart failed on a walked name
	call	osSizeMenu_dbgTrace
	.endif
	jp	osSizeMenu_noSel
osSizeMenu_appFound:
	ld	(_os_size_menu_vat), hl
	ld	a, (_os_size_menu_buf + 13)
	ld	(_os_size_menu_row), a
osSizeMenu_appSelSize:
	.if OSSIZEMENU_DEBUG
	ld	a, 'W'				; trace: app found, about to paint
	call	osSizeMenu_dbgTrace
	ld	hl, (_os_size_menu_vat)
	call	osSizeMenu_dbgHexDump		; first bytes of the app header
	.endif
	call	osSizeMenu_appSize		; bc = size in KB (hl = remainder); carry = bad header
	jr	nc, osSizeMenu_appSizeOk
	ld	de, _os_size_menu_buf		; unreadable header: show "?"
	ld	a, '?'
	ld	(de), a
	inc	de
	call	osSizeMenu_pad
	jr	osSizeMenu_appPaint
osSizeMenu_appSizeOk:
	call	osSizeMenu_formatAppKB		; "NKB" in the buffer, de -> its 0
	call	osSizeMenu_pad
osSizeMenu_appPaint:
	ld	a, (_os_size_menu_row)
	call	osSizeMenu_paintAt
	ld	a, (_os_size_menu_row)
	inc	a
	ld	(_os_size_menu_buf + 15), a
	ret

; a = n (>= 1): leaves the n-th app of the OS app walk in OP1 (type $14 + name); carry set when
; there are fewer than n apps. FindAppUp takes the previous name in OP1 and returns the next
; app's name in OP1 (carry = no more apps); a zeroed name yields the first app.
osSizeMenu_walkTo:
	ld	(_os_size_menu_begun), a
	ld	hl, ti.OP1
	ld	b, 11
	xor	a, a
osSizeMenu_walkClr:
	ld	(hl), a
	inc	hl
	djnz	osSizeMenu_walkClr
	ld	a, $14
	ld	(ti.OP1), a
osSizeMenu_walkLoop:
	call	ti.FindAppUp
	ret	c
	ld	hl, _os_size_menu_begun
	dec	(hl)
	jr	nz, osSizeMenu_walkLoop
	or	a, a				; carry clear
	ret

; App header: first field is $80/$81 $0F, then a 4-byte big-endian length. Total app size =
; length + 269 (6 master-field bytes + 4 signature-field bytes + 256 signature bytes + 3 size
; bytes), per WikiTI 84PCE:OS:Applications. (Observed: the first app starts 81 0F 00 01 1B 99,
; so 0x11B99 + 269 = 72870 bytes.)
; de -> header; returns hl = size in bytes, carry clear; carry set (hl = 0) when it is not a header.
osSizeMenu_hdrLen:
	ld	hl, 0
	ld	a, (de)
	inc	de
	and	a, $FE
	cp	a, $80
	jr	nz, osSizeMenu_hdrBad
	ld	a, (de)
	inc	de
	cp	a, $0F
	jr	nz, osSizeMenu_hdrBad
	inc	de				; skip the top length byte
	ld	a, (de)
	inc	de
	ld	l, a				; hl = b1
	ld	a, (de)
	inc	de
	ld	h, l
	ld	l, a				; hl = b1:b2
	ld	a, (de)
	add	hl, hl				; hl <<= 8 (24-bit), low byte now 0
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	l, a				; hl = b1:b2:b3
	ld	bc, 269			; was 6: the master field value needs + 269 for the app size
	add	hl, bc
	or	a, a				; carry clear
	ret
osSizeMenu_hdrBad:
	ld	hl, 0
	scf
	ret

osSizeMenu_appSize:
	ld	de, (_os_size_menu_vat)
	call	osSizeMenu_hdrLen		; hl = size in bytes, carry = not a header
	jr	nc, osSizeMenu_appSizeDone
	ld	bc, 0				; bad header: caller shows "?"
	scf
	ret
osSizeMenu_appSizeDone:
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
	or	a, a				; carry clear: size is valid
	ret

; bc = size in KB, hl = remainder (0-1023): builds "N[.T]KB" in the buffer with the same
; tenth scheme as osSizeMenu_format's KB path (tenth = remainder * 10 / 1024 = (r*5)>>9),
; so the Apps menu reads "70.9KB" like the EXEC menu's "9.4KB". A zero tenth prints "70KB".
osSizeMenu_formatAppKB:
	xor	a, a
	ld	(_os_size_menu_begun), a
	; tenth = (remainder * 5) >> 9
	ld	d, h
	ld	e, l				; de = r
	add	hl, hl
	add	hl, hl				; hl = 4r
	add	hl, de				; hl = 5r
	ld	a, h
	srl	a				; a = 5r >> 9 = r / 102.4 = tenth (0-9)
	push	af				; keep it across the digit extraction
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
	jr	nz, osSizeMenu_appTenth
	ld	a, '0'
	ld	(de), a
	inc	de
osSizeMenu_appTenth:
	pop	af				; a = tenth digit (0-9)
	or	a, a
	jr	z, osSizeMenu_appUnit		; remainder < ~51 bytes: no ".0"
	push	af
	ld	a, '.'
	ld	(de), a
	inc	de
	pop	af
	add	a, '0'
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
	.if OSSIZEMENU_DEBUG
	call	osSizeMenu_debug
	.endif
	call	osSizeMenu_hoverRow		; a = hovered row (0-8), carry = none found
	jp	c, osSizeMenu_noSel
	ld	(_os_size_menu_row), a
	call	osSizeMenu_blankRows		; a = hovered row: blank every OTHER slot (see below)
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
	ld	a, (hl)				; name length
	ld	(_os_size_menu_buf + 12), a
	; ChkFindSym left de = the data pointer (RAM) or the flash entry (archived). The pointer
	; rebuilt from the VAT entry (page byte + 2 address bytes) is only right for archived
	; programs: RAM programs have no page byte, which broke the size for them.
	call	ti.ChkInRam
	jr	z, osSizeMenu_selData
	ld	hl, 10				; archived: skip the flash record header
	add	hl, de
	ld	a, (_os_size_menu_buf + 12)
	ld	bc, 0
	ld	c, a
	add	hl, bc
	ex	de, hl				; de += 10 + NL
osSizeMenu_selData:
	ld	a, (de)
	ld	l, a
	inc	de
	ld	a, (de)
	ld	h, a				; hl = raw data length
	ld	a, (_os_size_menu_buf + 12)
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
	ld	de, _os_size_menu_buf		; lookup failed: show "?" (visible, not a wrong size)
	ld	a, '?'
	ld	(de), a
	inc	de
	call	osSizeMenu_pad
osSizeMenu_selPaint:
	ld	a, (_os_size_menu_row)
	call	osSizeMenu_paintAt
	ld	a, (_os_size_menu_row)
	inc	a
	ld	(_os_size_menu_buf + 15), a
	ret

; -> carry clear when the EXEC tab is drawn selected, carry set when not.
; The selected tab is a black box starting at x = 0; the unselected one is white there.
; 16 bpp VRAM: pixel (1, 45) is at $D40000 + (45 * 320 + 1) * 2 = $D47082.
osSizeMenu_execTabSel:
	ld	hl, $D47082
	ld	a, (hl)
	inc	hl
	or	a, (hl)				; both bytes zero = black (this also clears carry)
	ret	z
	scf
	ret

; -> a = row (0-8) whose "N:" label is drawn selected, carry set when none.
; Measured from a CEmu capture: the selected label is a black box from x = 0, y = 57 + 20 * row to
; 74 + 20 * row, with the (white) glyph starting at x = 2. An unselected label is white at x = 1.
; So ONE pixel at x = 1 decides it: black = selected. (The earlier tests used x = 2, which lands on
; a glyph stroke: the "5" has one at y = 139-144, which is why row 5 was never found.)
; 16 bpp VRAM: pixel (1, y) is at $D40000 + (y * 320 + 1) * 2; y = 66 -> $D4A502, row pitch $3200.
; NOTE (Apps menu): this geometry was measured on the PRGM EXEC page. Verify the Apps menu has
; the same first-row y, pitch and row count (re-capture in CEmu) before trusting row math there.
osSizeMenu_hoverRow:
	ld	hl, $D4A502
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

	.if OSSIZEMENU_DEBUG
; DEBUG: for rows 0-8 print one character each at (200, 40) (t = pixel (1, 66 + 20 * row) black,
; b = pixel (1, 73 + 20 * row) black; a selected row should be B):
;   B = both test pixels black (looks selected)   t = only the top one   b = only the bottom one
;   . = neither (not selected)
osSizeMenu_debug:
	ld	hl, $D4A502
	ld	de, _os_size_menu_buf
	xor	a, a
	ld	(_os_size_menu_tenth), a		; row counter
osSizeMenu_dbgLoop:
	xor	a, a
	ld	(_os_size_menu_begun), a		; bit 0 = top black, bit 1 = bottom black
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	dec	hl
	jr	nz, osSizeMenu_dbgBottom
	ld	a, 1
	ld	(_os_size_menu_begun), a
osSizeMenu_dbgBottom:
	push	hl
	ld	bc, $1180
	add	hl, bc
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	pop	hl
	jr	nz, osSizeMenu_dbgChar
	ld	a, (_os_size_menu_begun)
	or	a, 2
	ld	(_os_size_menu_begun), a
osSizeMenu_dbgChar:
	push	hl
	ld	hl, osSizeMenu_dbgChars
	ld	a, (_os_size_menu_begun)
	ld	c, a
	ld	b, 0
	add	hl, bc
	ld	a, (hl)
	pop	hl
	ld	(de), a
	inc	de
	ld	bc, $3200
	add	hl, bc
	ld	a, (_os_size_menu_tenth)
	inc	a
	ld	(_os_size_menu_tenth), a
	cp	a, 9
	jr	nz, osSizeMenu_dbgLoop
	xor	a, a
	ld	(de), a
	ld	a, 40
	ld	(ti.penRow), a
	ld	de, 200
	ld.sis	(ti.penCol & $FFFF), de
	ld	de, $0000
	ld.sis	(ti.drawFGColor & $FFFF), de
	ld	de, $FFFF
	ld.sis	(ti.drawBGColor & $FFFF), de
	ld	hl, _os_size_menu_buf
	call	ti.VPutS
	ret
osSizeMenu_dbgChars:
	db	'.', 't', 'b', 'B'
	.endif

; Nothing hovered (or the highlight is mid-fade): blank every slot, if something was painted.
osSizeMenu_noSel:
	ld	a, (_os_size_menu_buf + 15)
	or	a, a
	ret	z
	ld	a, 9				; 9 = keep no row
	call	osSizeMenu_blankRows
	xor	a, a
	ld	(_os_size_menu_buf + 15), a
	ret

; A = row to keep (9 = none): blank the size slot of every other row (0-8). The OS scrolls the
; list by moving pixels, TAKING OUR TEXT WITH IT (it showed up on the wrong rows and stayed), so
; blanking only the previously painted row is not enough. Painting white over white is invisible,
; and the kept row is not touched, so there is no flash. Scratch: buf + 12 = row to keep,
; buf + 14 = loop counter (both rewritten later; the text is built after this call).
osSizeMenu_blankRows:
	ld	(_os_size_menu_buf + 12), a
	xor	a, a
osSizeMenu_blankLoop:
	ld	(_os_size_menu_buf + 14), a
	ld	hl, _os_size_menu_buf + 12
	cp	a, (hl)
	call	nz, osSizeMenu_paintBlank	; a = row
	ld	a, (_os_size_menu_buf + 14)
	inc	a
	cp	a, 9
	jr	nz, osSizeMenu_blankLoop
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

	.if OSSIZEMENU_DEBUG
; DEBUG: erase the readout drawn by osSizeMenu_debug at (200, 40). White-on-white "B"s are used
; instead of spaces: the font's space is only ~2 px wide, so spaces would not cover 9 characters,
; and "B" is the widest glyph the readout uses. Keeps every register and the OS pen/colors.
osSizeMenu_debugClear:
	push	bc
	push	de
	push	hl
	push	ix
	ld	a, (ti.penRow)
	push	af
	ld.sis	de, (ti.penCol & $FFFF)
	push	de
	ld.sis	de, (ti.drawFGColor & $FFFF)
	push	de
	ld.sis	de, (ti.drawBGColor & $FFFF)
	push	de
	ld	a, 40
	ld	(ti.penRow), a
	ld	de, 200
	ld.sis	(ti.penCol & $FFFF), de
	ld	de, $FFFF
	ld.sis	(ti.drawFGColor & $FFFF), de
	ld.sis	(ti.drawBGColor & $FFFF), de
	ld	hl, osSizeMenu_dbgBlank
	call	ti.VPutS
	pop	de
	ld.sis	(ti.drawBGColor & $FFFF), de
	pop	de
	ld.sis	(ti.drawFGColor & $FFFF), de
	pop	de
	ld.sis	(ti.penCol & $FFFF), de
	pop	af
	ld	(ti.penRow), a
	pop	ix
	pop	hl
	pop	de
	pop	bc
	ret
osSizeMenu_dbgBlank:
	db	"BBBBBBBBB", 0

; DEBUG: hl -> bytes; draws the first 6 as 12 hex digits at (150, 40). Keeps every register and
; the OS pen/colors. Uses _os_size_menu_buf + 0..12 (free at the call site).
osSizeMenu_dbgHexDump:
	push	bc
	ld	bc, 6 * 256 + 40
	call	osSizeMenu_dbgHexDumpY
	pop	bc
	ret
; hl -> bytes, b = byte count (max 6), c = pen row (y); draws the hex at x = 150.
osSizeMenu_dbgHexDumpY:
	push	af
	push	bc
	push	de
	push	hl
	push	ix
	ld	de, _os_size_menu_buf
osSizeMenu_hexLoop:
	ld	a, (hl)
	inc	hl
	push	af
	rrca
	rrca
	rrca
	rrca
	call	osSizeMenu_hexNib
	pop	af
	call	osSizeMenu_hexNib
	djnz	osSizeMenu_hexLoop
	xor	a, a
	ld	(de), a
	ld	a, (ti.penRow)
	push	af
	ld.sis	de, (ti.penCol & $FFFF)
	push	de
	ld.sis	de, (ti.drawFGColor & $FFFF)
	push	de
	ld.sis	de, (ti.drawBGColor & $FFFF)
	push	de
	ld	a, c
	ld	(ti.penRow), a
	ld	de, 150
	ld.sis	(ti.penCol & $FFFF), de
	ld	de, $0000
	ld.sis	(ti.drawFGColor & $FFFF), de
	ld	de, $FFFF
	ld.sis	(ti.drawBGColor & $FFFF), de
	ld	hl, _os_size_menu_buf
	call	ti.VPutS
	pop	de
	ld.sis	(ti.drawBGColor & $FFFF), de
	pop	de
	ld.sis	(ti.drawFGColor & $FFFF), de
	pop	de
	ld.sis	(ti.penCol & $FFFF), de
	pop	af
	ld	(ti.penRow), a
	pop	ix
	pop	hl
	pop	de
	pop	bc
	pop	af
	ret
osSizeMenu_hexNib:
	and	a, $0F
	add	a, '0'
	cp	a, '9' + 1
	jr	c, osSizeMenu_hexPut
	add	a, 7
osSizeMenu_hexPut:
	ld	(de), a
	inc	de
	ret

; DEBUG: after an OS call: draws HL and DE (3 bytes each, lowest first) at (150, c) and the carry
; flag (0/1) at (270, c). Keeps HL and DE.
osSizeMenu_dbgShowRegs:
	ld	a, '0'
	jr	nc, osSizeMenu_showNC
	inc	a
osSizeMenu_showNC:
	ld	(_os_size_menu_tenth), a
	push	de
	push	hl
	ld	hl, 0
	add	hl, sp				; hl -> [HL: 3 bytes][DE: 3 bytes]
	ld	b, 6
	call	osSizeMenu_dbgHexDumpY
	ld	a, (_os_size_menu_tenth)
	call	osSizeMenu_dbgTraceY
	pop	hl
	pop	de
	ret

; DEBUG: a = one character, drawn at (270, 40). Keeps every register and the OS pen/colors.
; Uses _os_size_menu_buf + 0/1 only (free at every call site).
osSizeMenu_dbgTrace:
	push	bc
	ld	c, 40
	call	osSizeMenu_dbgTraceY
	pop	bc
	ret
; a = char, c = pen row (y); draws at x = 270.
osSizeMenu_dbgTraceY:
	push	af
	push	bc
	push	de
	push	hl
	push	ix
	ld	(_os_size_menu_buf), a
	xor	a, a
	ld	(_os_size_menu_buf + 1), a
	ld	a, (ti.penRow)
	push	af
	ld.sis	de, (ti.penCol & $FFFF)
	push	de
	ld.sis	de, (ti.drawFGColor & $FFFF)
	push	de
	ld.sis	de, (ti.drawBGColor & $FFFF)
	push	de
	ld	a, c
	ld	(ti.penRow), a
	ld	de, 270
	ld.sis	(ti.penCol & $FFFF), de
	ld	de, $0000
	ld.sis	(ti.drawFGColor & $FFFF), de
	ld	de, $FFFF
	ld.sis	(ti.drawBGColor & $FFFF), de
	ld	hl, _os_size_menu_buf
	call	ti.VPutS
	pop	de
	ld.sis	(ti.drawBGColor & $FFFF), de
	pop	de
	ld.sis	(ti.drawFGColor & $FFFF), de
	pop	de
	ld.sis	(ti.penCol & $FFFF), de
	pop	af
	ld	(ti.penRow), a
	pop	ix
	pop	hl
	pop	de
	pop	bc
	pop	af
	ret
	.endif

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