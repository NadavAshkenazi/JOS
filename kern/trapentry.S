/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

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
	pushl $(num);							\
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
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * code for generating entry points for the different traps.
 */

TRAPHANDLER_NOEC(t_divide, T_DIVIDE) 		# 0: divide error
TRAPHANDLER_NOEC(t_debug, T_DEBUG) 			# 1: debug exception
TRAPHANDLER_NOEC(t_nmi, T_NMI) 				# 2: non-maskable interrupt
TRAPHANDLER_NOEC(t_brkpt, T_BRKPT) 			# 3: breakpoint
TRAPHANDLER_NOEC(t_oflow, T_OFLOW) 			# 4: overflow
TRAPHANDLER_NOEC(t_bound, T_BOUND) 			# 5: bounds check
TRAPHANDLER_NOEC(t_illop, T_ILLOP) 			# 6:illegal opcode
TRAPHANDLER_NOEC(t_device, T_DEVICE) 		# 7: device not available
TRAPHANDLER(t_dblflt, T_DBLFLT) 			# 8: double fault
											# 9: resevred
TRAPHANDLER(t_tss, T_TSS) 					# 10: invalid task switch segment
TRAPHANDLER(t_segnp, T_SEGNP) 				# 11: segment not present
TRAPHANDLER(t_stack, T_STACK) 				# 12: stack exception
TRAPHANDLER(t_gpflt, T_GPFLT) 				# 13: general protection fault
TRAPHANDLER(t_pgflt, T_PGFLT) 				# 14: page fault
											# 15: resevred
TRAPHANDLER_NOEC(t_fperr, T_FPERR) 			# 16: device not available
TRAPHANDLER(t_align, T_ALIGN) 				# 17: page fault
TRAPHANDLER_NOEC(t_mchk, T_MCHK) 			# 18: device not available
TRAPHANDLER_NOEC(t_simderr, T_SIMDERR) 		# 19: device not available


TRAPHANDLER_NOEC(t_timer,IRQ_OFFSET + IRQ_TIMER);			# 32 		
TRAPHANDLER_NOEC(t_kbd,IRQ_OFFSET + IRQ_KBD);				# 33
TRAPHANDLER_NOEC(t_irq2,IRQ_OFFSET + 2);					# 34
TRAPHANDLER_NOEC(t_irq3,IRQ_OFFSET + 3);					# 35
TRAPHANDLER_NOEC(t_serial,IRQ_OFFSET + IRQ_SERIAL);			# 36
TRAPHANDLER_NOEC(t_irq5,IRQ_OFFSET + 5);					# 37
TRAPHANDLER_NOEC(t_irq6,IRQ_OFFSET + 6);					# 38
TRAPHANDLER_NOEC(t_spurious,IRQ_OFFSET + IRQ_SPURIOUS);		# 39
TRAPHANDLER_NOEC(t_irq8,IRQ_OFFSET + 8);					# 40
TRAPHANDLER_NOEC(t_irq9,IRQ_OFFSET + 9);					# 41
TRAPHANDLER_NOEC(t_irq10,IRQ_OFFSET + 10);					# 42
TRAPHANDLER_NOEC(t_e1000, IRQ_OFFSET + IRQ_E1000)
TRAPHANDLER_NOEC(t_irq12,IRQ_OFFSET + 12);					# 44
TRAPHANDLER_NOEC(t_irq13,IRQ_OFFSET + 13);					# 45
TRAPHANDLER_NOEC(t_ide,IRQ_OFFSET + IRQ_IDE);				# 46
TRAPHANDLER_NOEC(t_irq15,IRQ_OFFSET + 15);					# 47

TRAPHANDLER_NOEC(t_syscall, T_SYSCALL) 		# 48: device not available

/*
 * code for _alltraps
 */

 _alltraps:
	# push values to make the stack look like a struct Trapframe
	pushl %ds # push ds + padding
	pushl %es # push ds + padding
	pushal 
	/*
	pushal def:
		Temporary = ESP;
		Push(EAX);
		Push(ECX);
		Push(EDX);
		Push(EBX);
		Push(Temporary);
		Push(EBP);
		Push(ESI);
		Push(EDI);
	*/
	# load GD_KD into %ds and %es (16b values)
	movw $GD_KD, %ax
	movw %ax, %ds
	movw %ax, %es

	# pushl %esp to pass a pointer to the Trapframe as an argument to trap()
	pushl %esp 
	call trap



