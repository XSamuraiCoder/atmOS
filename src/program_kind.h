#ifndef SHELL_PROGRAM_KIND_H
#define SHELL_PROGRAM_KIND_H

#include <stddef.h>
#include <stdint.h>
#include <ti/tokens.h>

/* How the shell runs a program. The number is passed to asm_runProgram_run and stored in pins,
 * so it must match the dispatch at the top of _asm_runProgram_main in runProgram.s. */
#define PROGRAM_ASM     0    /* native eZ80 machine code (EF 7B ...) */
#define PROGRAM_C       1    /* native, made by the C toolchain (EF 7B 00) */
#define PROGRAM_BASIC   2    /* TI-BASIC, and hex-text Asm84CEPrgm (EF 7A) which the runner converts */
#define PROGRAM_ICE     3    /* native, made by ICE (EF 7B 7F) */
#define PROGRAM_ASMCALL 4    /* Z80-mode assembly: run through the OS as  Asm(prgmNAME)  */

/* Decide from the first bytes of a program's data (after the 2-byte size). */
static inline uint8_t program_kind_from_signature(const uint8_t *s, size_t n)
{
    if (n >= 3 && s[0] == OS_TOK_2BYTE_EXT) {
        if (s[1] == OS_TOK_ASM_84CE_PRGM_CMP) {         /* EF 7B: native eZ80 */
            if (s[2] == 0)    return PROGRAM_C;
            if (s[2] == 0x7F) return PROGRAM_ICE;
            return PROGRAM_ASM;
        }
        if (s[1] == OS_TOK_ASM_84C_PRGM || s[1] == OS_TOK_ASM_84C_PRGM_CMP)
            return PROGRAM_ASMCALL;                     /* EF 68 / EF 69: 84+C (Z80 mode) */
    }
    if (n >= 2 && s[0] == OS_TOK_2BYTE &&
        (s[1] == OS_TOK_ASM_PRGM || s[1] == OS_TOK_ASM_PRGM_CMP))
        return PROGRAM_ASMCALL;                         /* BB 6C / BB 6D: AsmPrgm, run with Asm( */
    return PROGRAM_BASIC;
}

#endif
