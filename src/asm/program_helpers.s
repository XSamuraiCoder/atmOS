; CEaShell-derived TI-OS program runner support.
; Copyright 2022-2026 RoccoLox Programs and TIny_Hacker.
; SPDX-License-Identifier: GPL-3.0-only

    .assume adl=1
    .include "src/asm/include/equates.inc"

    .global _asm_utils_loadNameOP1
    .global _asm_utils_findVar
    .global _asm_utils_backupPrgmName
    .global _asm_utils_lcdNormal
    .global _asm_utils_clrScrnAndUsedRAM
    .global _asm_utils_dispTextToolbar
    .global _asm_utils_deleteTempRunner
    .global _asm_utils_dispQuitErr
    .global _asm_hooks_installStopHook
    .global _asm_hooks_removeStopHook
    .global _asm_hooks_installAppChangeHook
    .global _asm_hooks_removeAppChangeHook
    .global _asm_hooks_removeGetCSCHook
    .global _asm_hooks_installGetCSCHookCont
    .global _asm_hooks_installHomescreenHook
    .global _asm_hooks_removeBasicKeyHook
    .global _asm_hooks_installBasicKeyHook
    .global _asm_hooks_basicPrgmHook
    .global _rodata_basicPrgmName
    .global _rodata_waitHomescreen
    .global _rodata_waitHomescreenFR
    .global _rodata_errorQuit

    .extern _asm_runProgram_returnOS.restoreHooks

    .section .text

_asm_utils_loadNameOP1:
    ld de, ti.OP1
    ld (de), a
    inc de
    ld bc, 8
    ldir
    ret

_asm_utils_findVar:
    call _asm_utils_loadNameOP1
    call ti.ChkFindSym
    call ti.ChkInRam
    ret z
    push hl
    push bc
    ld hl, 10
    add hl, de
    ld a, c
    ld bc, 0
    ld c, a
    add hl, bc
    ex de, hl
    pop bc
    pop hl
    ret

_asm_utils_backupPrgmName:
    ld hl, ti.OP1
    ld de, backupPrgmName
    jp ti.Mov9b

_asm_utils_lcdNormal:
    ld a, $2D
    ld (ti.mpLcdCtrl), a
    call ti.boot.ClearVRAM
    jp ti.DrawStatusBar

_asm_utils_clrScrnAndUsedRAM:
    call ti.ForceFullScreen
    call ti.ClrScrn
    call ti.HomeUp
    ld hl, ti.pixelShadow
    ld bc, 8400 * 3
    call ti.MemClear
    call ti.ClrTxtShd
    ld hl, ti.textShadow
    ld de, ti.cmdShadow
    ld bc, $104
    ldir
    ret

_asm_utils_dispTextToolbar:
    ld de, $E71C
    ld.sis (ti.drawFGColor & $FFFF), de
    ld.sis de, (ti.statusBarBGColor & $FFFF)
    ld.sis (ti.drawBGColor & $FFFF), de
    ld a, 14
    ld (ti.penRow), a
    ld de, 2
    ld.sis (ti.penCol & $FFFF), de
    call ti.VPutS
    ld de, $FFFF
    ld.sis (ti.drawBGColor & $FFFF), de
    ret

_asm_utils_deleteTempRunner:
    ld hl, _rodata_basicPrgmName
    call ti.Mov9ToOP1
    call ti.ChkFindSym
    call nc, ti.DelVarArc
    ret

_asm_utils_dispQuitErr:
    call ti.CursorOff
    call ti.DispErrorScreen
    ld hl, 1
    ld (ti.curRow), hl
    ld hl, _rodata_errorQuit
    set ti.textInverse, (iy + ti.textFlags)
    call ti.PutS
    res ti.textInverse, (iy + ti.textFlags)
    jp ti.PutS

hooks_parserStopHook:
    db $83
    push af
    cp a, 2
    jr z, parserStopHook.maybeStop

parserStopHook.chain:
    ld a, (parserChainLoc)
    cp a, $7F
    jr nz, parserStopHook.noChain
    pop af
    ld ix, (parserChainLoc + 1)
    jp (ix)

parserStopHook.noChain:
    pop af
    cp a, a
    ret

parserStopHook.maybeStop:
    ld a, hookTokenStop
    cp a, b
    jr nz, parserStopHook.chain
    pop af
    xor a, a
    jp ti.JError

_asm_hooks_installStopHook:
    xor a, a
    ld (parserChainLoc), a
    bit ti.parserHookActive, (iy + ti.hookflags4)
    jr z, installStopHook.noChain
    ld hl, (ti.parserHookPtr)
    ld de, hooks_parserStopHook
    or a, a
    sbc hl, de
    add hl, de
    jr z, installStopHook.checkIfBadExit

installStopHook.chainHooks:
    ld a, (hl)
    cp a, $83
    jr nz, installStopHook.noChain
    ex de, hl
    inc de
    ld hl, parserChainLoc
    ld (hl), $7F
    inc hl
    ld (hl), de
    jr installStopHook.noChain

installStopHook.checkIfBadExit:
    ld hl, parserChainLoc
    ld a, (hl)
    cp a, $7F
    jr nz, installStopHook.noChain
    inc hl
    ld hl, (hl)
    dec hl
    jr installStopHook.chainHooks

installStopHook.noChain:
    ld hl, hooks_parserStopHook
    jp ti.SetParserHook

_asm_hooks_removeStopHook:
    bit ti.parserHookActive, (iy + ti.hookflags4)
    jr z, removeStopHook.clearParser
    ld hl, (ti.parserHookPtr)
    ld de, hooks_parserStopHook
    or a, a
    sbc hl, de
    add hl, de
    ret nz
    ld hl, parserChainLoc
    ld a, (hl)
    cp a, $7F
    jr nz, removeStopHook.clearParser
    inc hl
    ld hl, (hl)
    dec hl
    xor a, a
    ld (parserChainLoc), a
    jp ti.SetParserHook

removeStopHook.clearParser:
    jp ti.ClrParserHook

_asm_hooks_installAppChangeHook:
    xor a, a
    ld (appChangeHookLoc), a
    bit ti.appChangeHookActive, (iy + ti.hookflags4)
    jr z, installAppChangeHook.install
    inc a
    ld (appChangeHookLoc), a
    ld hl, (ti.appChangeHookPtr)
    ld (appChangeHookLoc + 1), hl

installAppChangeHook.install:
    ex de, hl
    jp ti.SetAppChangeHook

_asm_hooks_removeAppChangeHook:
    bit ti.appChangeHookActive, (iy + ti.hookflags4)
    ret z
    ld hl, (ti.appChangeHookPtr)
    or a, a
    sbc hl, de
    ret nz
    ld a, (appChangeHookLoc)
    or a, a
    jp z, ti.ClrAppChangeHook
    ld hl, (appChangeHookLoc + 1)
    jp ti.SetAppChangeHook

_asm_hooks_removeGetCSCHook:
    ret

_asm_hooks_installGetCSCHookCont:
    ret

_asm_hooks_installHomescreenHook:
    ret

_asm_hooks_removeBasicKeyHook:
    bit ti.rawKeyHookActive, (iy + ti.hookflags2)
    ret z
    ld hl, (ti.rawKeyHookPtr)
    ld de, hooks_basicKeyHook
    or a, a
    sbc hl, de
    ret nz
    ld a, (rawKeyHookLoc)
    or a, a
    jp z, ti.ClrRawKeyHook
    ld hl, (rawKeyHookLoc + 1)
    jp ti.SetGetKeyHook

hooks_basicKeyHook:
    db $83
    push af
    ld a, (ti.cxCurApp)
    cp a, ti.kPrgmInput
    jr nz, basicKeyHook.returnAlt
    pop af
    push af
    cp a, ti.kQuit
    jr nz, basicKeyHook.returnAlt
    pop af
    xor a, a
    ret

basicKeyHook.returnAlt:
    pop af
    ld b, a
    inc b
    cp a, b
    ret

_asm_hooks_installBasicKeyHook:
    xor a, a
    ld (rawKeyHookLoc), a
    bit ti.rawKeyHookActive, (iy + ti.hookflags2)
    jr z, installBasicKeyHook.install
    inc a
    ld (rawKeyHookLoc), a
    ld hl, (ti.rawKeyHookPtr)
    ld (rawKeyHookLoc + 1), hl

installBasicKeyHook.install:
    ld hl, hooks_basicKeyHook
    jp ti.SetGetKeyHook

_asm_hooks_basicPrgmHook:
    db $83
    push hl
    ld hl, ti.mpLcdCtrl + 1
    bit 3, (hl)
    pop hl
    jr z, basicPrgmHook.exit
    ld e, a
    ld a, b
    cp a, ti.kError
    ld a, e
    ret nz
    cp a, ti.kQuit
    ret nz

basicPrgmHook.exit:
    push af
    push hl
    push bc
    ld de, _asm_hooks_basicPrgmHook
    call _asm_hooks_removeAppChangeHook
    call _asm_hooks_removeStopHook
    call _asm_hooks_removeBasicKeyHook
    call _asm_runProgram_returnOS.restoreHooks
    call _asm_utils_deleteTempRunner
    pop bc
    pop hl
    pop af
    ret

    .section .rodata

_rodata_basicPrgmName:
    .db ti.ProtProgObj, "SHPRGTMP", 0
_rodata_waitHomescreen:
    .db "Press a key", 0
_rodata_waitHomescreenFR:
    .db "Press a key", 0
_rodata_errorQuit:
    .db "1: Quit", 0
