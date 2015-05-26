/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushq $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushq $0;							\
	pushq $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
   Find the PUSHA,POPA macros in kern/macro.h. Since amd64 doesn't support
   pusha,popa so you will find these macros handy.
 */
TRAPHANDLER(md_systemerror,T_DBLFLT);
TRAPHANDLER(md_invalidtss,T_TSS);
TRAPHANDLER(md_segnotpres,T_SEGNP);
TRAPHANDLER(md_stackfault,T_STACK);
TRAPHANDLER(md_genprot,T_GPFLT);
TRAPHANDLER(md_pagefault,T_PGFLT);

TRAPHANDLER_NOEC(md_diverror,T_DIVIDE);
TRAPHANDLER_NOEC(md_debug,T_DEBUG);
TRAPHANDLER_NOEC(md_nonmask,T_NMI);
TRAPHANDLER_NOEC(md_breakpoint,T_BRKPT);
TRAPHANDLER_NOEC(md_overflow,T_OFLOW);
TRAPHANDLER_NOEC(md_boundchk,T_BOUND);
TRAPHANDLER_NOEC(md_invalidop,T_ILLOP);
TRAPHANDLER_NOEC(md_deviceerror,T_DEVICE);
/* TRAPHANDLER_NOEC(md_copsegovr,MD_COPSEGOVR)
TRAPHANDLER_NOEC(md_unktrap,MD_UNKTRAP) */
TRAPHANDLER_NOEC(md_xfpuerror,T_FPERR);
TRAPHANDLER_NOEC(md_aligncheck,T_ALIGN);
TRAPHANDLER_NOEC(md_mchchk,T_MCHK);
TRAPHANDLER_NOEC(md_simderr,T_SIMDERR);
TRAPHANDLER_NOEC(md_syscall,T_SYSCALL);
/* lab4 exercise 13 */
TRAPHANDLER_NOEC(md_timer,IRQ_OFFSET + IRQ_TIMER);
TRAPHANDLER_NOEC(md_kbd,IRQ_OFFSET + IRQ_KBD);
TRAPHANDLER_NOEC(md_serial,IRQ_OFFSET + IRQ_SERIAL);
TRAPHANDLER_NOEC(md_spurious,IRQ_OFFSET + IRQ_SPURIOUS);
TRAPHANDLER_NOEC(md_ide,IRQ_OFFSET + IRQ_IDE);
/* TRAPHANDLER_NOEC(md_error,IRQ_OFFSET + IRQ_ERROR); */
/*
 * Lab 3: Your code here for _alltraps
 *
 * Hint: Be sure to review the x64 calling convention from lab1
 *       for how to pass a pointer to the trapframe.
 */

_alltraps:
	subq $8,%rsp
	movw %es, (%rsp)
	subq $8,%rsp
	movw %ds, (%rsp)
	PUSHA
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es
	movq %rsp, %rdi
	call trap
