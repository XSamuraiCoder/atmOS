;--------------------------------------
;
; CEaShell Source Code - runProgram.s
; By RoccoLox Programs and TIny_Hacker
; Copyright 2022 - 2026
; License: GPL-3.0
;
;--------------------------------------

    .assume adl=1

    .include "src/asm/include/equates.inc"

    .global _asm_runProgram_run
    .type   _asm_runProgram_run, @function
    .global _asm_runProgram_main
    .type   _asm_runProgram_main, @function
    .global _asm_runProgram_returnOS.restoreHooks
    .type   _asm_runProgram_returnOS.restoreHooks, @function
    .global _asm_runProgram_error
    .type   _asm_runProgram_error, @function

    .extern _flash_program_return_to_shell
    .extern _asm_hooks_installStopHook
    .extern _asm_hooks_removeStopHook
    .extern _asm_hooks_installGetCSCHookCont
    .extern _asm_hooks_removeGetCSCHook
    .extern _asm_hooks_installAppChangeHook
    .extern _asm_hooks_removeAppChangeHook
    .extern _asm_hooks_installHomescreenHook
    .extern _asm_hooks_removeBasicKeyHook
    .extern _asm_hooks_installBasicKeyHook
    .extern _asm_hooks_basicPrgmHook
    .extern _asm_utils_findVar
    .extern _asm_utils_backupPrgmName
    .extern _asm_utils_lcdNormal
    .extern _asm_utils_clrScrnAndUsedRAM
    .extern _asm_utils_dispTextToolbar
    .extern _asm_utils_deleteTempRunner
    .extern _asm_utils_dispQuitErr
    .extern _rodata_waitHomescreen
    .extern _rodata_waitHomescreenFR
    .extern _rodata_basicPrgmName

;--------------------------------------

    .section .text

_asm_runProgram_run:
    ld iy, 0
    add iy, sp
    ld hl, (iy + 12)
    ld de, 15
    add hl, de
    ld de, (hl)
    ld (editArcProgs), de
    xor a, a ; returnCEaShell
    ld (returnLoc), a
    inc hl
    inc hl
    inc hl
    inc hl
    push iy
    ld iy, ti.flags
    set disableBusyIndicator, (iy + shellFlags)
    bit 0, (hl)
    jr nz, $ + 6
    res disableBusyIndicator, (iy + shellFlags)
    pop iy
    ld a, (iy + 6) ; program type
    ld hl, ti.OP1
    ld (hl), a
    inc hl
    ld de, (iy + 3)
    ld bc, 8
    ex de, hl
    ldir
    ld a, (iy + 9) ; shell type
    ; The Flash App C stack must be released before InsertMem can reclaim RAM.
    ; Keep the shell type on the stack (not in RAM) across ResetStacks.
    ld sp, (ti.onSP)
    push af
    call ti.ResetStacks
    pop af
    ld iy, ti.flags
    jp _asm_runProgram_main

; OP1 = Program name
; a = Shell type

_asm_runProgram_main:
    push af
    ld iy, ti.flags

main.debounce:
    call ti.GetCSC
    or a, a
    jr nz, main.debounce
    call _asm_utils_backupPrgmName
    ld de, (ti.asm_prgm_size)
    or a, a
    sbc hl, hl
    ld (ti.asm_prgm_size), hl
    ld hl, ti.userMem
    call ti.DelMem
    call _asm_utils_findVar + 4
    ld (ti.asm_data_ptr1), de
    ld bc, 0
    ld a, (de)
    ld c, a
    inc de
    ld a, (de)
    ld b, a
    pop af
    cp a, 4
    jp z, main.runAsmCall ; Z80-mode assembly: run through the OS as Asm(prgmNAME)
    or a, a
    jr z, main.runASM ; assembly program
    dec a
    jr z, main.runASM ; C program
    dec a
    jp z, main.runBasic ; BASIC program
    dec a
    jp nz, main.runBasic ; ICE program

main.runASM:
    ld hl, isASM
    ld (hl), true
    inc de
    push bc
    ld hl, 128
    add hl, bc
    call ti.EnoughMem
    pop hl
    ld a, ti.E_Memory
    jp c, _asm_runProgram_error
    ld (ti.asm_prgm_size), hl
    push hl
    push hl
    ld de, ti.userMem
    call ti.InsertMem
    pop bc
    xor a, a
    ld hl, ti.userMem
    push hl
    call ti.MemSet
    ld hl, (ti.asm_data_ptr1)
    inc hl ; skip size + $EF7B
    inc hl
    inc hl
    inc hl
    pop de
    pop bc
    ld a, b
    or a, c
    jr z, _asm_runProgram_main.ASMLoaded
    ldir

_asm_runProgram_main.ASMLoaded:
    call ti.DisableAPD
    set ti.appAutoScroll, (iy + ti.appFlags)
    call runProgram_vectorsSetup
    ld hl, runProgram_return
    push hl
    jp ti.userMem

; Z80-mode assembly (Asm84CPrgm / Asm84CCmp / AsmPrgm / AsmCmp) cannot run in eZ80 mode. Do what the
; home screen does: run the BASIC line  Asm(prgmNAME)  and let the OS start the program.
; The line is built as tokens in scratch RAM, copied into the temporary program, and run like a
; BASIC program (so the return path cleans the temporary program up).
; OP1 = type, program name (8 bytes, zero padded)
main.runAsmCall:
    ld hl, isASM
    ld (hl), false
    ld hl, ti.pixelShadow
    ld (hl), $BB ; 2-byte token prefix
    inc hl
    ld (hl), $6A ; Asm(
    inc hl
    ld (hl), $5F ; prgm
    inc hl
    ld de, ti.OP1 + 1
    ld b, 8
main.asmCall.copyName:
    ld a, (de)
    or a, a
    jr z, main.asmCall.nameDone
    ld (hl), a
    inc hl
    inc de
    djnz main.asmCall.copyName
main.asmCall.nameDone:
    ld (hl), $11 ; )
    inc hl
    ld de, ti.pixelShadow
    or a, a
    sbc hl, de ; hl = length of the line
    push hl
    ld de, 128
    add hl, de
    call ti.EnoughMem
    pop hl
    ld a, ti.E_Memory
    jp c, _asm_runProgram_error
    push hl
    ld hl, _rodata_basicPrgmName
    call ti.Mov9ToOP1
    pop hl
    push hl
    call ti.CreateProtProg
    inc de
    inc de
    pop bc
    ld hl, ti.pixelShadow
    ldir
    jp main.loadComplete

main.runBasic:
    ld hl, isASM
    ld (hl), false
    inc de
    ld a, (de)
    cp a, ti.tExtTok
    jr nz, main.noSquish
    inc de
    ld a, (de)
    cp a, ti.tAsm84CePrgm
    jr z, main.squish
    dec de

main.noSquish:
    call ti.ChkInRam
    jr z, main.loadComplete.ready
    push de
    push bc
    ld hl, 128
    add hl, bc
    call ti.EnoughMem
    jr nc, $ + 10
    pop bc
    pop de
    ld a, ti.E_Memory
    jp _asm_runProgram_error
    ld hl, _rodata_basicPrgmName
    call ti.Mov9ToOP1
    pop hl
    push hl
    call ti.CreateProtProg
    inc de
    inc de
    pop bc
    pop hl
    ld a, b
    or a, c
    jr z, main.loadComplete
    ldir

main.loadComplete:
    call ti.OP4ToOP1
main.loadComplete.ready:
    call ti.RunIndicOn
    call ti.EnableAPD
    bit disableBusyIndicator, (iy + shellFlags)
    call nz, ti.RunIndicOff
    set ti.progExecuting, (iy + ti.newDispF)
    set ti.cmdExec, (iy + ti.cmdFlags)
    call runProgram_vectorsSetup
    call _asm_hooks_installStopHook
    call _asm_hooks_installBasicKeyHook
    ld de, _asm_hooks_basicPrgmHook
    call _asm_hooks_installAppChangeHook
    ei
    ld hl, runProgram_return
    push hl
    jp ti.ParseInp

main.squish:
    ex de, hl
    inc hl
    dec bc
    dec bc ; remove Asm84CEPrgm from un-squished size
    ld a, b
    or a, c
    jr z, main.error
    push bc
    push bc

main.loopNewlines:
    ld a, (hl)
    cp a, ti.tEnter
    jr nz, main.notNewline
    pop de
    dec de
    push de

main.notNewline:
    inc hl
    dec bc
    ld a, b
    or a, c
    jr nz, main.loopNewlines
    pop hl ; un-squished size minus newlines
    bit 0, l
    jr nz, main.error - 2 ; check if an odd number of tokens
    ld a, h
    or a, l
    jr nz, main.notEmpty
    pop hl
    pop hl

main.error:
    ld a, ti.E_Syntax
    jp _asm_runProgram_error

main.notEmpty:
    push hl
    ld de, 128
    add hl, de
    call ti.EnoughMem
    pop hl
    ld a, ti.E_Memory
    jp c, _asm_runProgram_error
    ld (ti.asm_prgm_size), hl
    push hl
    ld de, ti.userMem
    call ti.InsertMem
    pop bc
    xor a, a
    ld hl, ti.userMem
    push hl
    call ti.MemSet
    ld hl, (ti.asm_data_ptr1)
    inc hl
    inc hl
    inc hl
    inc hl
    pop de ; userMem
    pop bc ; original (un-squished) size

main.loopLoad:
    ld a, b
    or a, c
    jp z, _asm_runProgram_main.ASMLoaded
    ld a, (hl)
    inc hl
    dec bc
    cp a, ti.tEnter
    jr z, main.loopLoad
    push de
    call runProgram_convertTokenToHex
    add a, a
    add a, a
    add a, a
    add a, a
    ld e, a
    ld a, (hl)
    inc hl
    dec bc
    call runProgram_convertTokenToHex
    add a, e
    pop de
    ld (de), a
    inc de
    jr main.loopLoad

runProgram_return:
    ; FIX: asm / ICE programs may leave IY pointing elsewhere and interrupts
    ; disabled. Everything below (flag bits, GetCSC loops) needs both.
    ld iy, ti.flags
    ei
    ; FIX: removed the (returnLoc) check. This runner is only ever started from the
    ; Flash App with returnLoc = 0, and if a program overwrote that byte the old code
    ; took the "return to OS" path, which ends in a RET to a stack that no longer
    ; holds an OS return address.
    ld hl, ti.textShadow
    ld bc, 260

return.chkHomescreen:
    ld a, (hl)
    cp a, ' '
    jr nz, return.chkDonePause
    inc hl
    dec bc
    ld a, b
    or a, c
    jr nz, return.chkHomescreen
    jr return.chkDone

return.chkDonePause:
    ld.sis hl, (ti.localLanguage & $FFFF)
    or a, a
    ld de, $010C ; check for French language
    sbc hl, de
    ld hl, _rodata_waitHomescreen
    jr nz, $ + 6
    ld hl, _rodata_waitHomescreenFR
    call _asm_utils_dispTextToolbar

return.loopPause:
    call ti.GetCSC
    or a, a
    jr z, return.loopPause
    call ti.os.ClearStatusBarLow

return.chkDone:
    call ti.PopErrorHandler
    xor a, a
    res error, (iy + shellFlags)

runProgram_return.error:
    ld iy, ti.flags ; FIX
    ei              ; FIX
    push af
    ld hl, ti.mpTmrCtrl + 1 ; fix timers
    ld a, (hl)
    and a, l
    ld (hl), a
    res ti.progExecuting, (iy + ti.newDispF)
    res ti.cmdExec, (iy + ti.cmdFlags)
    res ti.textInverse, (iy + ti.textFlags)
    res ti.onInterrupt, (iy + ti.onFlags)
    call ti.ReloadAppEntryVecs
    call _asm_hooks_removeStopHook
    call _asm_hooks_removeBasicKeyHook
    ld de, _asm_hooks_basicPrgmHook
    call _asm_hooks_removeAppChangeHook
    pop bc
    ld a, b
    or a, a
    call nz, runProgram_showError

return.quit:
    call ti.DeleteTempPrograms
    call ti.CleanAll
    call ti.ReloadAppEntryVecs
    call ti.ForceFullScreen
    call _asm_utils_deleteTempRunner
    ld de, (ti.asm_prgm_size)
    or a, a
    sbc hl, hl
    ld (ti.asm_prgm_size), hl
    ld hl, ti.userMem
    call ti.DelMem
    res appWantHome, (iy + sysHookFlg)

return.debounce:
    call ti.GetCSC
    or a, a
    jr nz, return.debounce
    xor a, a
    ld (ti.kbdGetKy), a
    ; FIX: always return to the shell. The old code tested (returnLoc) and, when it
    ; was non-zero, ran _asm_runProgram_returnOS, which finishes with RET. Here there
    ; is no OS caller to return to (the stack was reset in _asm_runProgram_run).
    res ti.monAbandon, (iy + ti.monFlags)
    jp _flash_program_return_to_shell

_asm_runProgram_returnOS:
    call ti.SaveCmdShadow
    ld a, ti.cxCmd
    call ti.NewContext0
    bit error, (iy + shellFlags)
    jr z, returnOS.skipClear
    call ti.ClrScrn
    call ti.HomeUp

returnOS.skipClear:
    call ti.PPutAway

_asm_runProgram_returnOS.restoreHooks:
    ld a, (editArcProgs)
    bit 0, a
    jr nz, $ + 8
    ld a, (editLockProgs)
    bit 0, a
    call nz, _asm_hooks_installHomescreenHook
    ld a, (getCSCHooks)
    call _asm_hooks_installGetCSCHookCont
    xor a, a
    or a, 2
    ret

runProgram_showError:
    call _asm_utils_lcdNormal
    ld a, (ti.errNo)
    or a, a
    ret z
    call ti.DispErrorScreen
    call _asm_utils_dispQuitErr
runProgram_showError.wait:
    call ti.GetCSC
    or a, a
    jr z, runProgram_showError.wait
    ret

_asm_runProgram_error:
    ; FIX: no (returnLoc) test / JError exit, see runProgram_return
    ld (ti.errNo), a
    call _asm_utils_lcdNormal
    call _asm_utils_dispQuitErr

error.waitLoop:
    call ti.GetCSC
    cp a, ti.sk1
    jr z, $ + 4
    cp a, ti.skEnter
    jp z, _flash_program_return_to_shell
    jr error.waitLoop

runProgram_vectorsSetup:
    call _asm_utils_lcdNormal
    ld a, $25
    ld ($D02687), a
    xor a, a
    ld (ti.menuCurrent), a
    ld (ti.appErr1), a
    ld (ti.kbdGetKy), a
    ld hl, runProgram_vectors
    call ti.AppInit
    call _asm_utils_clrScrnAndUsedRAM
    call _asm_hooks_removeGetCSCHook
    ld hl, runProgram_return.error
    jp ti.PushErrorHandler

runProgram_vectors:
    .d24 vectors.ret
    .d24 ti.SaveCmdShadow
    .d24 vectors.putaway
    .d24 vectors.restore
    .d24 vectors.ret
    .d24 vectors.ret

vectors.ret:
    ret

vectors.putaway:
    xor a, a
    ld (ti.currLastEntry), a
    bit appInpPrmptInit, (iy + ti.apiFlg2)
    call z, ti.ForceFullScreen
    call ti.ReloadAppEntryVecs
    call ti.PutAway
    ld b, 0
    ret

vectors.restore:
    call ti.HomeUp
    call ti.ClrScrn
    jp ti.RStrShadow

runProgram_convertTokenToHex:
    cp a, ti.t0
    jr c, convertTokenToHex.error
    cp a, ti.t9 + 1
    jr nc, convertTokenToHex.skip
    jr convertTokenToHex.convert

convertTokenToHex.error:
    pop hl
    pop hl
    ld a, ti.E_Syntax
    jp _asm_runProgram_error

convertTokenToHex.skip:
    cp a, ti.tA
    jr c, convertTokenToHex.error
    cp a, ti.tF + 1
    jr nc, convertTokenToHex.error

convertTokenToHex.convert:
    sub a, ti.t0
    cp a, $0A
    ret c
    sub a, 7
    ret