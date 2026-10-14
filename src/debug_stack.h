#ifndef SHELL_DEBUG_STACK_H
#define SHELL_DEBUG_STACK_H

/* Build with -DSHELL_DEBUG_STACK (CFLAGS in the Makefile) to show the lowest stack address the
 * shell has reached in the menu bar ("SP D1xxxx"). If that number gets close to the top of
 * the shell's static data, the stack is about to overwrite it, which shows up as random
 * crashes. Without the flag every call below compiles to nothing. */
#ifdef SHELL_DEBUG_STACK
void shell_note_stack(void);            /* record the current stack depth */
#define NOTE_STACK() shell_note_stack()
#else
#define NOTE_STACK() ((void)0)
#endif

#endif