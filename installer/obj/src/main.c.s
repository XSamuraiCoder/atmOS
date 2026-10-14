	.section	.text,"ax",@progbits
	.assume	ADL = 1
	.file	"main.c"
	.section	.text._delete_vars,"ax",@progbits
	.globl	_delete_vars                    ; -- Begin function delete_vars
	.type	_delete_vars,@function
_delete_vars:                           ; @delete_vars
; %bb.0:
	ld	hl, -16
	call	__frameset
	ld	iy, _.str
	or	a, a
	sbc	hl, hl
	lea	bc, ix - 10
	.local	.LBB0_1
.LBB0_1:                                ; =>This Inner Loop Header: Depth=1
	ld	de, 99
	ld	(ix - 13), hl
	or	a, a
	sbc	hl, de
	jr	z, .LBB0_4
; %bb.2:                                ;   in Loop: Header=BB0_1 Depth=1
	ld	hl, (ix - 13)
	push	hl
	push	iy
	push	bc
	ld	(ix - 16), bc
	call	_sprintf
	pop	hl
	pop	hl
	pop	hl
	or	a, a
	sbc	hl, hl
	push	hl
	push	hl
	ld	hl, (ix - 16)
	push	hl
	ld	hl, 21
	push	hl
	call	_os_ChkFindSym
	pop	de
	pop	de
	pop	de
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	z, .LBB0_4
; %bb.3:                                ;   in Loop: Header=BB0_1 Depth=1
	ld	hl, 21
	push	hl
	ld	hl, (ix - 16)
	push	hl
	call	_delete_var
	pop	hl
	pop	hl
	ld	hl, (ix - 13)
	inc	hl
	ld	de, _.str
	push	de
	pop	iy
	ld	bc, (ix - 16)
	jr	.LBB0_1
	.local	.LBB0_4
.LBB0_4:
	ld	sp, ix
	pop	ix
	ret
	.local	.Lfunc_end0
.Lfunc_end0:
	.size	_delete_vars, .Lfunc_end0-_delete_vars
                                        ; -- End function
	.section	.text.___main_argc_argv,"ax",@progbits
	.globl	___main_argc_argv               ; -- Begin function __main_argc_argv
	.type	___main_argc_argv,@function
___main_argc_argv:                      ; @__main_argc_argv
; %bb.0:
	ld	hl, -65
	call	__frameset
	ld	iy, -3145600
	call	_os_ClrLCD
	call	_os_HomeUp
	call	_os_DrawStatusBar
	or	a, a
	sbc	hl, hl
	ld	(ix - 31), hl
	call	_port_setup
	bit	0, a
	jr	z, .LBB1_2
; %bb.1:
	ld	hl, 3
	jp	.LBB1_44
	.local	.LBB1_2
.LBB1_2:
	ld	hl, _appvars
	xor	a, a
	ld	(ix - 53), a
	lea	de, ix - 15
	ld	(ix - 40), de
	lea	de, ix - 28
	ld	(ix - 37), de
	ld	bc, 99
	ld	de, 0
	ld	(ix - 43), de
	push	hl
	pop	iy
	.local	.LBB1_3
.LBB1_3:                                ; =>This Inner Loop Header: Depth=1
	push	de
	pop	hl
	or	a, a
	sbc	hl, bc
	ld	(ix - 34), iy
	jp	z, .LBB1_13
; %bb.4:                                ;   in Loop: Header=BB1_3 Depth=1
	ld	(ix - 46), de
	push	de
	ld	hl, _.str
	push	hl
	ld	hl, (ix - 37)
	push	hl
	call	_sprintf
	ld	(ix - 49), hl
	pop	hl
	pop	hl
	pop	hl
	pea	ix - 15
	or	a, a
	sbc	hl, hl
	push	hl
	ld	hl, (ix - 37)
	push	hl
	ld	hl, 21
	push	hl
	call	_os_ChkFindSym
	pop	de
	pop	de
	pop	de
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	z, .LBB1_42
; %bb.5:                                ;   in Loop: Header=BB1_3 Depth=1
	ld	hl, (ix - 15)
	push	hl
	pop	iy
	ld	de, (ix - 49)
	add	iy, de
	push	hl
	pop	bc
	ld	de, -3145728
	or	a, a
	sbc	hl, de
	jr	c, .LBB1_7
; %bb.6:                                ;   in Loop: Header=BB1_3 Depth=1
	push	bc
	pop	hl
	jr	.LBB1_8
	.local	.LBB1_7
.LBB1_7:                                ;   in Loop: Header=BB1_3 Depth=1
	lea	hl, iy + 10
	.local	.LBB1_8
.LBB1_8:                                ;   in Loop: Header=BB1_3 Depth=1
	ld	iy, (hl)
	ld	bc, 0
	ld	c, iyl
	ld	b, iyh
	ld	iy, (ix - 34)
	ld	(iy + 3), bc
	lea	de, iy + 0
	ld	(ix - 49), hl
	push	hl
	pop	iy
	lea	hl, iy + 2
	push	de
	pop	iy
	ld	(iy), hl
	ld	de, (ix - 43)
	ld	(iy + 6), de
	ld	hl, (ix - 46)
	add	hl, bc
	or	a, a
	sbc	hl, bc
	push	bc
	pop	iy
	jr	nz, .LBB1_11
; %bb.9:                                ;   in Loop: Header=BB1_3 Depth=1
	ld	de, 261
	ld	iy, (ix - 49)
	add	iy, de
	ld	hl, 10
	push	hl
	push	iy
	ld	hl, _app_name
	push	hl
	call	_strncpy
	pop	hl
	pop	hl
	pop	hl
	ld	hl, _app_name
	push	hl
	call	_os_FindAppStart
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jp	nz, .LBB1_43
; %bb.10:                               ;   in Loop: Header=BB1_3 Depth=1
	ld	iy, (ix - 34)
	ld	iy, (iy + 3)
	ld	de, (ix - 43)
	.local	.LBB1_11
.LBB1_11:                               ;   in Loop: Header=BB1_3 Depth=1
	lea	hl, iy + 0
	add	hl, de
	ld	(ix - 43), hl
	ld	(ix - 31), hl
	lea	hl, iy + 0
	ld	bc, 65200
	or	a, a
	sbc	hl, bc
	jr	nz, .LBB1_14
; %bb.12:                               ;   in Loop: Header=BB1_3 Depth=1
	ld	iy, (ix - 34)
	lea	iy, iy + 9
	ld	de, (ix - 46)
	inc	de
	ld	bc, 99
	jp	.LBB1_3
	.local	.LBB1_13
.LBB1_13:
	ld	iy, (iy + 3)
	.local	.LBB1_14
.LBB1_14:
	ld	de, 3
	add	iy, de
	lea	hl, iy + 0
	ld	iy, (ix - 34)
	ld	(iy + 3), hl
	call	_find_last_app
	push	hl
	pop	iy
	ld	hl, (ix - 43)
	call	__ineg
	ex	de, hl
	ld	(ix - 52), iy
	add	iy, de
	lea	hl, iy - 3
	ld	c, 16
	ld	(ix - 46), hl
	call	__ishru
	ld	(ix - 49), hl
	ld	a, l
	dec	a
	ld	(ix - 56), a
	call	_os_GetSymTablePtr
	push	hl
	pop	bc
	.local	.LBB1_15
.LBB1_15:                               ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB1_19 Depth 2
	pea	ix - 18
	ld	hl, (ix - 40)
	push	hl
	pea	ix - 6
	pea	ix - 3
	push	bc
	call	_os_NextSymEntry
	push	hl
	pop	bc
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	pop	hl
	sbc	hl, hl
	adc	hl, bc
	jp	z, .LBB1_24
; %bb.16:                               ;   in Loop: Header=BB1_15 Depth=1
	ld	de, (ix - 18)
	push	de
	pop	hl
	ld	(ix - 43), bc
	ld	bc, -3145728
	or	a, a
	sbc	hl, bc
	ld	bc, (ix - 43)
	jr	nc, .LBB1_15
; %bb.17:                               ;   in Loop: Header=BB1_15 Depth=1
	ex	de, hl
	ld	c, 16
	call	__ishru
	ld	bc, (ix - 43)
	push	hl
	pop	iy
	ld	a, iyl
	ld	l, (ix - 56)
	cp	a, l
	jr	c, .LBB1_15
; %bb.18:                               ;   in Loop: Header=BB1_15 Depth=1
	ld	hl, (ix - 46)
	ld	(ix - 59), hl
	ld	bc, 0
	.local	.LBB1_19
.LBB1_19:                               ;   Parent Loop BB1_15 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	push	bc
	pop	hl
	ld	de, 99
	or	a, a
	sbc	hl, de
	jp	z, .LBB1_23
; %bb.20:                               ;   in Loop: Header=BB1_19 Depth=2
	ld	(ix - 62), iy
	ld	(ix - 65), bc
	push	bc
	ld	hl, _.str
	push	hl
	ld	hl, (ix - 37)
	push	hl
	call	_sprintf
	pop	hl
	pop	hl
	pop	hl
	ld	hl, (ix - 6)
	push	hl
	ld	hl, (ix - 37)
	push	hl
	ld	hl, (ix - 40)
	push	hl
	call	_strncmp
	pop	de
	pop	de
	pop	de
	add	hl, bc
	or	a, a
	sbc	hl, bc
	jr	z, .LBB1_22
; %bb.21:                               ;   in Loop: Header=BB1_19 Depth=2
	ld	bc, (ix - 65)
	inc	bc
	ld	de, 65200
	ld	hl, (ix - 59)
	add	hl, de
	ld	(ix - 59), hl
	ld	iy, (ix - 62)
	jr	.LBB1_19
	.local	.LBB1_22
.LBB1_22:                               ;   in Loop: Header=BB1_15 Depth=1
	ld	hl, (ix - 59)
	ld	c, 16
	call	__ishru
	ld	de, (ix - 62)
	ld	a, e
	cp	a, l
                                        ; kill: def $a killed $a
	sbc	a, a
	bit	0, a
	ld	bc, (ix - 43)
	jp	nz, .LBB1_15
	.local	.LBB1_23
.LBB1_23:
	ld	hl, 4
	jp	.LBB1_44
	.local	.LBB1_24
.LBB1_24:
	ld	hl, _.str.13
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	hl, _app_name
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	iy, -3145600
	call	_os_NewLine
	ld	hl, _.str.14
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	iy, -3145600
	call	_os_NewLine
	call	_os_NewLine
	ld	hl, _.str.15
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	iy, -3145600
	call	_os_NewLine
	ld	hl, _.str.16
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	c, 16
	ld	de, (ix - 52)
	push	de
	pop	hl
	call	__ishru
	push	hl
	pop	bc
	ld	(ix - 40), de
	.local	.LBB1_25
.LBB1_25:                               ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB1_28 Depth 2
	ld	de, 0
	ld	a, c
	ld	hl, (ix - 49)
	cp	a, l
	ld	iy, (ix - 34)
	jp	c, .LBB1_41
; %bb.26:                               ;   in Loop: Header=BB1_25 Depth=1
	lea	hl, iy + 0
	ld	(ix - 37), bc
	ld	bc, _appvars
	or	a, a
	sbc	hl, bc
	ld	hl, (ix - 37)
	jp	c, .LBB1_41
; %bb.27:                               ;   in Loop: Header=BB1_25 Depth=1
	ld	e, l
	ex	de, hl
	ld	c, 16
	call	__ishl
	push	hl
	pop	bc
	ld	de, 65536
	add	hl, de
	ld	(ix - 52), hl
	ld	de, (ix - 40)
	ld	(ix - 43), bc
	.local	.LBB1_28
.LBB1_28:                               ;   Parent Loop BB1_25 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	push	bc
	pop	hl
	or	a, a
	sbc	hl, de
	ld	(ix - 34), iy
	jp	nc, .LBB1_40
; %bb.29:                               ;   in Loop: Header=BB1_28 Depth=2
	lea	hl, iy + 0
	push	de
	pop	bc
	ld	de, _appvars
	or	a, a
	sbc	hl, de
	push	bc
	pop	de
	jp	c, .LBB1_40
; %bb.30:                               ;   in Loop: Header=BB1_28 Depth=2
	ld	de, (ix - 52)
	ld	bc, (iy + 6)
	ld	hl, (ix - 46)
	add	hl, bc
	ld	(ix - 56), hl
	ld	bc, (iy + 3)
	add	hl, bc
	ld	(ix - 59), hl
	or	a, a
	sbc	hl, de
	jr	c, .LBB1_32
; %bb.31:                               ;   in Loop: Header=BB1_28 Depth=2
	ld	(ix - 59), de
	.local	.LBB1_32
.LBB1_32:                               ;   in Loop: Header=BB1_28 Depth=2
	ld	hl, (ix - 43)
	ld	bc, (ix - 56)
	or	a, a
	sbc	hl, bc
	jr	c, .LBB1_34
; %bb.33:                               ;   in Loop: Header=BB1_28 Depth=2
	ld	bc, (ix - 43)
	.local	.LBB1_34
.LBB1_34:                               ;   in Loop: Header=BB1_28 Depth=2
	lea	hl, iy + 0
	ld	iy, (hl)
	push	bc
	pop	hl
	ld	de, (ix - 56)
	or	a, a
	sbc	hl, de
	ex	de, hl
	add	iy, de
	ld	hl, (ix - 59)
	or	a, a
	sbc	hl, bc
	bit	0, (ix - 53)                    ; 1-byte Folded Reload
	ld	(ix - 40), bc
	jr	z, .LBB1_36
; %bb.35:                               ;   in Loop: Header=BB1_28 Depth=2
	push	hl
	push	iy
	push	bc
	jr	.LBB1_37
	.local	.LBB1_36
.LBB1_36:                               ;   in Loop: Header=BB1_28 Depth=2
	ld	de, -3
	add	hl, de
	push	hl
	push	iy
	push	bc
	call	_flash_write
	pop	hl
	pop	hl
	pop	hl
	ld	hl, 3
	push	hl
	pea	ix - 31
	ld	iy, (ix - 59)
	pea	iy - 3
	.local	.LBB1_37
.LBB1_37:                               ;   in Loop: Header=BB1_28 Depth=2
	call	_flash_write
	pop	hl
	pop	hl
	pop	hl
	ld	bc, (ix - 40)
	push	bc
	pop	hl
	ld	de, (ix - 56)
	or	a, a
	sbc	hl, de
	ld	de, -9
	ld	iy, (ix - 34)
	push	bc
	pop	hl
	jr	z, .LBB1_39
; %bb.38:                               ;   in Loop: Header=BB1_28 Depth=2
	ld	de, 0
	.local	.LBB1_39
.LBB1_39:                               ;   in Loop: Header=BB1_28 Depth=2
	add	iy, de
	ld	a, 1
	ld	(ix - 53), a                    ; 1-byte Folded Spill
	ld	bc, (ix - 43)
	ex	de, hl
	jp	.LBB1_28
	.local	.LBB1_40
.LBB1_40:                               ;   in Loop: Header=BB1_25 Depth=1
	ld	(ix - 40), de
	ld	hl, (ix - 37)
	dec	l
	ld	(ix - 37), hl
	push	hl
	call	_flash_erase
	ld	bc, (ix - 37)
	pop	hl
	jp	.LBB1_25
	.local	.LBB1_41
.LBB1_41:
	ld	hl, (ix - 46)
	push	hl
	call	_fix_relocations
	pop	hl
	or	a, a
	sbc	hl, hl
	jr	.LBB1_44
	.local	.LBB1_42
.LBB1_42:
	ld	hl, 2
	jr	.LBB1_44
	.local	.LBB1_43
.LBB1_43:
	ld	hl, 1
	.local	.LBB1_44
.LBB1_44:
	ld	(ix - 34), hl
	ld	hl, _.str.7
	ld	(ix - 40), hl
	ld	hl, _.str.8
	ld	(ix - 37), hl
	ld	iy, -3145600
	call	_os_ClrLCD
	call	_os_HomeUp
	call	_os_DrawStatusBar
	ld	hl, JTI1_0
	ld	de, (ix - 34)
	add	hl, de
	add	hl, de
	add	hl, de
	ld	hl, (hl)
	jp	(hl)
	.local	.LBB1_45
.LBB1_45:
	ld	hl, _.str.1
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	iy, -3145600
	call	_os_NewLine
	ld	hl, _.str.2
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	hl, _app_name
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	iy, -3145600
	call	_os_NewLine
	call	_os_NewLine
	ld	hl, _.str.3
	push	hl
	call	_os_PutStrFull
	pop	hl
	call	_confirm_delete_vars
	bit	0, a
	jp	z, .LBB1_53
; %bb.46:
	ld	hl, (ix + 9)
	ld	hl, (hl)
	ld	de, 6
	push	de
	push	hl
	call	_delete_var
	pop	hl
	pop	hl
	call	_delete_vars
	jp	.LBB1_53
	.local	.LBB1_47
.LBB1_47:
	ld	hl, _.str.12
	ld	(ix - 37), hl
	ld	hl, _.str.7
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	iy, -3145600
	call	_os_NewLine
	ld	hl, _.str.10
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	iy, -3145600
	call	_os_NewLine
	ld	hl, _.str.11
	ld	(ix - 40), hl
	jr	.LBB1_50
	.local	.LBB1_48
.LBB1_48:
	ld	hl, _.str.4
	ld	de, _.str.5
	ld	(ix - 40), de
	ld	de, _.str.6
	ld	(ix - 37), de
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	iy, -3145600
	call	_os_NewLine
	ld	hl, _.str.2
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	hl, _app_name
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	iy, -3145600
	call	_os_NewLine
	call	_os_NewLine
	jr	.LBB1_50
	.local	.LBB1_49
.LBB1_49:
	ld	hl, _.str.9
	ld	(ix - 37), hl
	.local	.LBB1_50
.LBB1_50:
	ld	hl, (ix - 40)
	push	hl
	call	_os_PutStrFull
	pop	hl
	ld	iy, -3145600
	call	_os_NewLine
	ld	hl, (ix - 37)
	push	hl
	call	_os_PutStrFull
	pop	hl
	.local	.LBB1_51
.LBB1_51:                               ; =>This Inner Loop Header: Depth=1
	call	_os_GetCSC
	or	a, a
	jr	nz, .LBB1_51
	.local	.LBB1_52
.LBB1_52:                               ; =>This Inner Loop Header: Depth=1
	call	_os_GetCSC
	or	a, a
	jr	z, .LBB1_52
	.local	.LBB1_53
.LBB1_53:
	ld	hl, (ix - 34)
	ld	sp, ix
	pop	ix
	ret
	.local	.Lfunc_end1
.Lfunc_end1:
	.size	___main_argc_argv, .Lfunc_end1-___main_argc_argv
	.section	.rodata.___main_argc_argv,"a",@progbits
JTI1_0:
	d24	.LBB1_45
	d24	.LBB1_48
	d24	.LBB1_50
	d24	.LBB1_49
	d24	.LBB1_47
                                        ; -- End function
	.section	.rodata._.str,"a",@progbits
	.balign	1
	.local	_.str
_.str:
	.asciz	"atmOS%u"

	.section	.rodata._.str.1,"a",@progbits
	.balign	1
	.local	_.str.1
_.str.1:
	.asciz	"Successfully installed."

	.section	.rodata._.str.2,"a",@progbits
	.balign	1
	.local	_.str.2
_.str.2:
	.asciz	"App: "

	.section	.bss._app_name,"aw",@nobits
	.balign	1
	.local	_app_name
_app_name:
	.zero	10

	.section	.rodata._.str.3,"a",@progbits
	.balign	1
	.local	_.str.3
_.str.3:
	.asciz	"Delete installer files?"

	.section	.rodata._.str.4,"a",@progbits
	.balign	1
	.local	_.str.4
_.str.4:
	.asciz	"Already installed."

	.section	.rodata._.str.5,"a",@progbits
	.balign	1
	.local	_.str.5
_.str.5:
	.asciz	"Delete app from the"

	.section	.rodata._.str.6,"a",@progbits
	.balign	1
	.local	_.str.6
_.str.6:
	.asciz	"mem menu to reinstall."

	.section	.rodata._.str.7,"a",@progbits
	.balign	1
	.local	_.str.7
_.str.7:
	.asciz	"Install failed."

	.section	.rodata._.str.8,"a",@progbits
	.balign	1
	.local	_.str.8
_.str.8:
	.asciz	"Missing an appvar."

	.section	.rodata._.str.9,"a",@progbits
	.balign	1
	.local	_.str.9
_.str.9:
	.asciz	"Unsupported OS version."

	.section	.rodata._.str.10,"a",@progbits
	.balign	1
	.local	_.str.10
_.str.10:
	.asciz	"Out of archive space."

	.section	.rodata._.str.11,"a",@progbits
	.balign	1
	.local	_.str.11
_.str.11:
	.asciz	"Try running the"

	.section	.rodata._.str.12,"a",@progbits
	.balign	1
	.local	_.str.12
_.str.12:
	.asciz	"GarbageCollect command."

	.section	.bss._appvars,"aw",@nobits
	.balign	1
	.local	_appvars
_appvars:
	.zero	891

	.section	.rodata._.str.13,"a",@progbits
	.balign	1
	.local	_.str.13
_.str.13:
	.asciz	"Installing: "

	.section	.rodata._.str.14,"a",@progbits
	.balign	1
	.local	_.str.14
_.str.14:
	.asciz	"Please wait..."

	.section	.rodata._.str.15,"a",@progbits
	.balign	1
	.local	_.str.15
_.str.15:
	.asciz	"This may take a while."

	.section	.rodata._.str.16,"a",@progbits
	.balign	1
	.local	_.str.16
_.str.16:
	.asciz	"Do not reset!"

	.ident	"clang version 19.1.0 (https://github.com/CE-Programming/llvm-project ef28e9c54cd1333a6091ab2ffbd315b465fc5090)"
	.section	".note.GNU-stack","",@progbits
	.extern	_os_HomeUp
	.extern	_os_ClrLCD
	.extern	_llvm.eh.sjlj.functioncontext
	.extern	_os_GetCSC
	.extern	_strncpy
	.extern	_llvm.stacksave.p0
	.extern	_llvm.eh.sjlj.callsite
	.extern	_llvm.eh.sjlj.setup.dispatch
	.extern	_llvm.lifetime.end.p0
	.extern	_os_ChkFindSym
	.extern	_llvm.lifetime.start.p0
	.extern	_strncmp
	.extern	__ishru
	.extern	_fix_relocations
	.extern	_port_setup
	.extern	_flash_erase
	.extern	_os_DrawStatusBar
	.extern	_llvm.eh.sjlj.lsda
	.extern	_flash_write
	.extern	__Unwind_SjLj_Register
	.extern	_os_FindAppStart
	.extern	__Unwind_SjLj_Unregister
	.extern	_llvm.frameaddress.p0
	.extern	_os_PutStrFull
	.extern	_delete_var
	.extern	_confirm_delete_vars
	.extern	_llvm.stackrestore.p0
	.extern	__frameset
	.extern	_sprintf
	.extern	__ishl
	.extern	_os_GetSymTablePtr
	.extern	_find_last_app
	.extern	_os_NewLine
	.extern	_os_NextSymEntry
	.extern	__ineg
