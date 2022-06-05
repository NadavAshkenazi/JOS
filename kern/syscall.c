/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/time.h>
#include <kern/e1000.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	// LAB 3: Your code here.
	user_mem_assert(curenv, s, len, PTE_U);
	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;

	if (e == curenv) 
		// cprintf("[%08x] exiting gracefully (type: %d)\n", curenv->env_id, curenv->env_type); //XXX
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);

	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	// LAB 4: Your code here
	struct Env* newEnv;
	int res;

	res = env_alloc(&newEnv, curenv->env_id);
	if (res < 0)
		return res;
	
	//status is set to ENV_NOT_RUNNABLE
	newEnv->env_status = ENV_NOT_RUNNABLE;

	//and the register set is copied from the current environment
	newEnv->env_tf = curenv->env_tf;

	newEnv->env_tf.tf_regs.reg_eax = 0; //set newEnv to return with 0;

	return newEnv->env_id; //return from parent with child id
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	// Hint: Use the 'envid2env' function from kern/env.c to translate an
	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.

	// LAB 4: Your code here.

	if (!(status == ENV_RUNNABLE || status == ENV_NOT_RUNNABLE)) // -E_INVAL if status is not a valid status for an environment.
		return -E_INVAL;
		
	struct Env* e;
	int res = envid2env(envid, &e, 1);

	if (res < 0) // -E_BAD_ENV if environment envid doesn't currently exist
		return -E_BAD_ENV;

	e->env_status = status;
	return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	struct Env *requestEnv;
	if (envid2env(envid, &requestEnv, 1) < 0) //check valid enviroment
		return -E_BAD_ENV;

	user_mem_check(requestEnv, tf, sizeof(struct Trapframe), PTE_W); // check if the user provied valid mem with write perm
	requestEnv->env_tf = *tf;
	requestEnv->env_tf.tf_cs |= DPL_USER; 
	requestEnv->env_tf.tf_ss |= DPL_USER; 
	requestEnv->env_tf.tf_eflags |= FL_IF;

	return 0;
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
	struct Env* e;
	int res = envid2env(envid, &e, 1);
	if (res < 0) // -E_BAD_ENV if environment envid doesn't currently exist or the caller doesn't have permission to change envid
		return -E_BAD_ENV;
	
	e->env_pgfault_upcall = func;
	return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.	

	if ((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P)) // check that PTE_U | PTE_P are on
		return -E_INVAL;
	
	if (perm & ~PTE_SYSCALL) // check that only optional bit are on
		return -E_INVAL;

	struct Env* e;
	int res = envid2env(envid, &e, 1);
	if (res < 0) // -E_BAD_ENV if environment envid doesn't currently exist
		return -E_BAD_ENV;

	if (((uintptr_t)va >= UTOP) || (PGOFF(va) != 0)) // check that va does not exceeds UTOP and that there is no offset (page is aligned)
		return -E_INVAL;
	
	struct PageInfo* pp;
	pp = page_alloc(perm);

	if (!pp)
		return -E_NO_MEM;

	res = page_insert(e->env_pgdir, pp, va, perm);
	if (res < 0){
		page_free(pp);
		return -E_NO_MEM;
	}
		
	return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
	if ((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P)) // check that PTE_U | PTE_P are on
		return -E_INVAL;
	
	if (perm & ~PTE_SYSCALL) // check that only optional bit are on
		return -E_INVAL;

	int res;
	struct Env* srcEnv;
	struct Env* dstEnv;

	res = envid2env(srcenvid, &srcEnv, 1);
	if (res < 0 ) //-E_BAD_ENV if srcenvid doesn't currently exist or the caller doesn't have permission to change it
		return -E_BAD_ENV;

	res = envid2env(dstenvid, &dstEnv, 1);
	if (res < 0 ) //-E_BAD_ENV if dstenvid doesn't currently exist or the caller doesn't have permission to change it
		return -E_BAD_ENV;

	struct PageInfo* pp;
	pte_t* pte;
	pp = page_lookup(srcEnv->env_pgdir, srcva, &pte);
	if (!pp) //if srcva is not mapped in srcenvid's address space
		return -E_INVAL;

	// if (srcva or dstva) >= UTOP or (srcva or dstva) is not page-aligned
	if (((uintptr_t)srcva >= UTOP || PGOFF(srcva)) || ((uintptr_t)dstva >= UTOP) || PGOFF(dstva)) // check that va does not exceeds UTOP and that there is no offset (page is aligned)
		return -E_INVAL;
	
	if (perm & PTE_W && !((*pte) & PTE_W)) //must not grant write access to a read-only page
		return -E_INVAL;
	
	res = page_insert(dstEnv->env_pgdir, pp, dstva, perm);
	if (res < 0){ //if there's no memory to allocate any necessary page tables
		return -E_NO_MEM;
	}
	return 0;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().

	// LAB 4: Your code here.

	struct Env* e;
	int res = envid2env(envid, &e, 1);
	if (res < 0) // -E_BAD_ENV if environment envid doesn't currently exist
		return -E_BAD_ENV;

	if ((uintptr_t)va >= UTOP || PGOFF(va) != 0) // check that va does not exceeds UTOP and that there is no offset (page is aligned)
		return -E_INVAL;

	page_remove(e->env_pgdir, va);

	return 0;

}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
	struct Env *targetEnv;
	struct PageInfo *pp;
	pte_t* pte;


	int res = envid2env(envid, &targetEnv, 0);
	// cprintf("envid = %d\n", envid);
	// cprintf("res = %e\n", res);

	if (res < 0)
		return -E_BAD_ENV;

	if (targetEnv->env_ipc_recving == 0)
		return -E_IPC_NOT_RECV;

	if ((uintptr_t) srcva < UTOP){
		if (PGOFF(srcva) != 0)// not page-aligned
			return -E_INVAL;
		
		else if ((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P)) // not present or not with user premissions.
			return -E_INVAL;

		else if ((perm & (~PTE_SYSCALL)) != 0) // holds invalid bits for syscall
			return -E_INVAL;

		pp = page_lookup(curenv->env_pgdir, srcva, &pte);
		if (pp == NULL) // could not find srcva in env pgdir 
			return -E_INVAL;

		if ((perm & PTE_W) && !(*pte & PTE_W)) // asked for writable permissions but srcva is read_only
			return -E_INVAL;
		
		res = page_insert(targetEnv->env_pgdir, pp, targetEnv->env_ipc_dstva, perm);
		if (res < 0)
			return -E_NO_MEM;
		
		targetEnv->env_ipc_perm = perm;
	}
	else
		targetEnv->env_ipc_perm = 0; // no page mapping is sent

	targetEnv->env_ipc_recving = 0;
	targetEnv->env_ipc_from = curenv->env_id;
	targetEnv->env_ipc_value = value;
	targetEnv->env_status = ENV_RUNNABLE;

	return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
	if (((uintptr_t) dstva < UTOP) && (PGOFF(dstva) != 0)) // valid addr but not page-aligned
		return -E_INVAL;

	curenv->env_ipc_recving = 1;
	curenv->env_ipc_dstva = dstva;
	curenv->env_status = ENV_NOT_RUNNABLE; //no need to yield - the clock interupt will cause returning from trap and yielding
	
	return 0;
}


static int sys_set_priority(int priority) {
	curenv->priority = priority;
	return 0;
}

// Return the current time.
static int
sys_time_msec(void)
{
	// LAB 6: Your code here.
	return time_msec(); // get ticks 
}

static int
sys_transmit(void* addr, size_t size){

	user_mem_assert(curenv, addr, PGSIZE, PTE_U|PTE_U); // assert valid address
	struct PageInfo* pp = page_lookup(curenv->env_pgdir, addr, 0);
	return e1000_transmit(pp, size);
}

static int
sys_receive(void * addr){
	struct PageInfo *pp;
	int res =  e1000_receive(&pp);
	while (res < 0){
		curenv->env_net_blocked = true;
		curenv->env_status = ENV_NOT_RUNNABLE;
		sched_yield();

		res = e1000_receive(&pp);
	}
	//insert packet received into host mem.
	res = page_insert(curenv->env_pgdir, pp, addr, PTE_U | PTE_W | PTE_P);
	if (res < 0){
		page_free(pp);
		cprintf("sys_receive: could not insert page to host mem: %p -> %e", addr, res);
		res = -E_NO_MEM;
	}
	user_mem_assert(curenv, addr, PGSIZE, PTE_U | PTE_W | PTE_P); //check prem and destroy if denied

	return res;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	// LAB 3: Your code here.

	switch (syscallno) {

		case SYS_cputs:
			sys_cputs((const char*)a1, (size_t)a2);
			return 0;
		case SYS_cgetc:
			return sys_cgetc();
		case SYS_getenvid:
			return sys_getenvid();
		case SYS_env_destroy:
			return sys_env_destroy((envid_t)a1);
		case SYS_yield:
			sys_yield();
			return 0;
		case SYS_exofork:
			return sys_exofork();
		case SYS_env_set_status:
			return sys_env_set_status((envid_t)a1, (int)a2);
		case SYS_page_alloc:
			return sys_page_alloc((envid_t)a1, (void*)a2, (int)a3);
		case SYS_page_map:
			return sys_page_map((envid_t)a1, (void*)a2, (envid_t)a3, (void*)a4, (int)a5);
		case SYS_page_unmap:
			return sys_page_unmap((envid_t)a1, (void*)a2);
		case SYS_env_set_pgfault_upcall:
			return sys_env_set_pgfault_upcall((envid_t) a1, (void*) a2);
		case SYS_ipc_try_send:
			return sys_ipc_try_send((envid_t) a1, (uint32_t) a2, (void*) a3, (unsigned int) a4);
		case SYS_ipc_recv:
			return sys_ipc_recv((void*) a1);
		case SYS_set_priority:
			return sys_set_priority(a1);
		case SYS_env_set_trapframe:
			return sys_env_set_trapframe((envid_t) a1, (struct Trapframe*) a2);
		case SYS_time_msec:
			return sys_time_msec();
		case SYS_transmit:
			return sys_transmit((void *)a1, (size_t)a2);
		case SYS_receive:
			return sys_receive((void *)a1);
		default: 	
			return -E_INVAL; // todo: was return -E_NO_SYS;
		
	}
}


