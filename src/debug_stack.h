#ifndef SHELL_DEBUG_STACK_H
#define SHELL_DEBUG_STACK_H

/* The stack probe is compiled into EVERY build (it is tiny and keeps the generated code the
 * same with and without the debug display). Build with -DSHELL_DEBUG_STACK (CFLAGS in the
 * Makefile) to also show the lowest stack address reached in the menu bar ("SP D1xxxx"). */
void shell_note_stack(void);            /* record the current stack depth */
#define NOTE_STACK() shell_note_stack()

#endif