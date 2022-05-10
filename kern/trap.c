#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};


static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
trap_init(void)
{
	extern struct Segdesc gdt[];

	// LAB 3: Your code here.
	// declerations of funcs defined in trapentry.s and registering to IDT
	void t_divide();			//0:  divide error
	SETGATE(idt[T_DIVIDE], INTERRUPT, GD_KT, &t_divide, DPL_KERN);

	void t_debug();				//1:  debug exception
	SETGATE(idt[T_DEBUG], INTERRUPT, GD_KT, &t_debug, DPL_USER)	

	void t_nmi();				//2:  non-maskable interrupt
	SETGATE(idt[T_NMI], INTERRUPT, GD_KT, &t_nmi, DPL_KERN);	

	void t_brkpt();				//3:  breakpoint
	SETGATE(idt[T_BRKPT], TRAP, GD_KT, &t_brkpt, DPL_USER);	

	void t_oflow();				//4:  overflow
	SETGATE(idt[T_OFLOW], TRAP, GD_KT, &t_oflow, DPL_KERN);	

	void t_bound();				//5:  bounds check
	SETGATE(idt[T_BOUND], INTERRUPT, GD_KT, &t_bound, DPL_KERN);

	void t_illop();				//6:  illegal opcode
	SETGATE(idt[T_ILLOP], INTERRUPT, GD_KT, &t_illop, DPL_KERN);

	void t_device();			//7:  device not available
	SETGATE(idt[T_DEVICE], INTERRUPT, GD_KT, &t_device, DPL_KERN);	

	void t_dblflt();			//8:  double fault
	SETGATE(idt[T_DBLFLT], INTERRUPT, GD_KT, &t_dblflt, DPL_KERN);

								//9:  resevred

	void t_tss();				//10: invalid task switch segment
	SETGATE(idt[T_TSS], INTERRUPT, GD_KT, &t_tss, DPL_KERN);

	void t_segnp();				//11: segment not present	
	SETGATE(idt[T_SEGNP], INTERRUPT, GD_KT, &t_segnp, DPL_KERN);

	void t_stack();				//12: stack exception	
	SETGATE(idt[T_STACK], INTERRUPT, GD_KT, &t_stack, DPL_KERN);

	void t_gpflt();				//13: general protection fault	
	SETGATE(idt[T_GPFLT], INTERRUPT, GD_KT, &t_gpflt, DPL_KERN);

	void t_pgflt();				//14: page fault
	SETGATE(idt[T_PGFLT], INTERRUPT, GD_KT, &t_pgflt, DPL_KERN);

								//15: resevred

	void t_fperr();				//16: device not available	
	SETGATE(idt[T_FPERR], INTERRUPT, GD_KT, &t_fperr, DPL_KERN);

	void t_align();				//17: aligment check
	SETGATE(idt[T_ALIGN], INTERRUPT, GD_KT, &t_align, DPL_KERN);

	void t_mchk();				//18: machine check	
	SETGATE(idt[T_MCHK], INTERRUPT, GD_KT, &t_mchk, DPL_KERN);

	void t_simderr();			//19: SIMD floating point error		
	SETGATE(idt[T_SIMDERR], INTERRUPT, GD_KT, &t_simderr, DPL_KERN);
	


	void t_timer();				//32
	SETGATE(idt[IRQ_OFFSET + IRQ_TIMER], INTERRUPT, GD_KT, &t_timer, DPL_KERN);
	
	void t_kbd();				//33
	SETGATE(idt[IRQ_OFFSET + IRQ_KBD], INTERRUPT, GD_KT, &t_kbd, DPL_KERN);
	
	void t_irq2();				//34
	SETGATE(idt[IRQ_OFFSET + 2], INTERRUPT, GD_KT, &t_irq2, DPL_KERN);
	
	void t_irq3();				//35
	SETGATE(idt[IRQ_OFFSET + 3], INTERRUPT, GD_KT, &t_irq3, DPL_KERN);
	
	void t_serial();			//36
	SETGATE(idt[IRQ_OFFSET + IRQ_SERIAL], INTERRUPT, GD_KT, &t_serial, DPL_KERN);
	
	void t_irq5();				//37
	SETGATE(idt[IRQ_OFFSET + 5], INTERRUPT, GD_KT, &t_irq5, DPL_KERN);
	
	void t_irq6();				//38
	SETGATE(idt[IRQ_OFFSET + 6], INTERRUPT, GD_KT, &t_irq6, DPL_KERN);
	
	void t_spurious();			//39
	SETGATE(idt[IRQ_OFFSET + IRQ_SPURIOUS], INTERRUPT, GD_KT, &t_spurious, DPL_KERN);
	
	void t_irq8();				//40
	SETGATE(idt[IRQ_OFFSET + 8], INTERRUPT, GD_KT, &t_irq8, DPL_KERN);
	
	void t_irq9();				//41
	SETGATE(idt[IRQ_OFFSET + 9], INTERRUPT, GD_KT, &t_irq9, DPL_KERN);
	
	void t_irq10();				//42
	SETGATE(idt[IRQ_OFFSET + 10], INTERRUPT, GD_KT, &t_irq10, DPL_KERN);
	
	void t_irq11();				//43
	SETGATE(idt[IRQ_OFFSET + 11], INTERRUPT, GD_KT, &t_irq11, DPL_KERN);
	
	void t_irq12();				//44
	SETGATE(idt[IRQ_OFFSET + 12], INTERRUPT, GD_KT, &t_irq12, DPL_KERN);
	
	void t_irq13();				//45
	SETGATE(idt[IRQ_OFFSET + 13], INTERRUPT, GD_KT, &t_irq13, DPL_KERN);
	
	void t_ide();				//46
	SETGATE(idt[IRQ_OFFSET + IRQ_IDE], INTERRUPT, GD_KT, &t_ide, DPL_KERN);
	
	void t_irq15();				//47
	SETGATE(idt[IRQ_OFFSET + 15], INTERRUPT, GD_KT, &t_irq15, DPL_KERN);


	void t_syscall();			//48: system call
	SETGATE(idt[T_SYSCALL], INTERRUPT, GD_KT, &t_syscall, DPL_USER);

	// Per-CPU setup 
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//
	// ltr sets a 'busy' flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// wrong, you may not get a fault until you try to return from
	// user space on that CPU.
	//
	// LAB 4: Your code here:

	// Setup a TSS per CPU so that we get the right stack
	// when we trap to the kernel.
	uint32_t curID = (uint32_t)thiscpu->cpu_id;
	thiscpu->cpu_ts.ts_esp0 = (uintptr_t) percpu_kstacks[curID] + KSTKSIZE;
	thiscpu->cpu_ts.ts_ss0 = GD_KD;

	// Initialize the TSS slot of the gdt.
	gdt[(GD_TSS0 >> 3) + curID] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
					sizeof(struct Taskstate) - 1, 0);
	gdt[(GD_TSS0 >> 3) + curID].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0 + (curID << 3));

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

bool debuggerFlag = false;

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.
	uint32_t trapNumber = tf->tf_trapno;

	if (trapNumber == T_PGFLT){
		page_fault_handler(tf);
		return;
	}
	else if (trapNumber == T_BRKPT){
		debuggerFlag = true;
		monitor(tf);
		return;
	}	
	else if (trapNumber == T_DEBUG){
		monitor(tf);
		return;
	}

	else if (trapNumber == T_SYSCALL){
		(tf->tf_regs).reg_eax = syscall(tf->tf_regs.reg_eax, //syscall number
										  tf->tf_regs.reg_edx, //args 1-5
										  tf->tf_regs.reg_ecx,
										  tf->tf_regs.reg_ebx,
										  tf->tf_regs.reg_edi,
										  tf->tf_regs.reg_esi ); //return val in eax
		return;
	}

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (trapNumber == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

	// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling the scheduler!
	// LAB 4: Your code here.
	if (trapNumber == IRQ_OFFSET + IRQ_TIMER){
		lapic_eoi();
		sched_yield();
	}
	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Re-acqurie the big kernel lock if we were halted in
	// sched_yield()
	if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();
	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
		lock_kernel();
		assert(curenv);

		// Garbage collect if current enviroment is a zombie
		if (curenv->env_status == ENV_DYING) {
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	// LAB 3: Your code here.
	if (!(tf->tf_cs & DPL_USER)) // not in user premmissions
		panic("page_fault_handler: pageFault in kernel mode");


	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.

	if (curenv->env_pgfault_upcall){ //Call the environment's page fault upcall, if one exists
		struct UTrapframe* userTf;
		if (tf->tf_esp < UXSTACKTOP && tf->tf_esp >= UXSTACKTOP-PGSIZE){ // nested exception
			uint32_t* stackTop = (uint32_t*)(tf->tf_esp - 4);
			*stackTop = 0x0; //push empty 32b word

			stackTop -= sizeof(struct UTrapframe); //Set up a page fault stack frame on the user exception stack (after current TF in UXSTACK)
			userTf = (struct UTrapframe*)(stackTop);
		} 
		else
			userTf = (struct UTrapframe*)(UXSTACKTOP - sizeof(struct UTrapframe)); //Set up a page fault stack frame on the user exception stack (after UXSTACKTOP)
		
		user_mem_assert(curenv, (void*) userTf, sizeof(struct UTrapframe), PTE_W); //assert write permissions for new tf.

		//set up new user tf:
		userTf->utf_esp = tf->tf_esp;
		userTf->utf_eflags = tf->tf_eflags;
		userTf->utf_eip = tf->tf_eip;
		userTf->utf_regs = tf->tf_regs;
		userTf->utf_err = tf->tf_err;
		userTf->utf_fault_va = fault_va;

		curenv->env_tf.tf_eip = (uintptr_t)curenv->env_pgfault_upcall;
		curenv->env_tf.tf_esp = (uintptr_t)userTf;
		env_run(curenv); // run pgfault user handler
	}


	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}