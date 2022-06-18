// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int res;

	// Checks that the faulting access was (1) a write, and (2) to a
	// copy-on-write page. If not, panic.
	
	if (!((err & FEC_WR) && (uvpt[PGNUM(addr)] & PTE_COW)))
		panic("pgfault: not write fault or not COW\n");

	// Allocates a new page, map it at a temporary location (PFTEMP),
	// copies the data from the old page to the new page, then moves the new
	// page to the old page's address.

	envid_t envid = sys_getenvid();
	res = sys_page_alloc(envid, PFTEMP, (PTE_W | PTE_U | PTE_P));
	if (res < 0)
		panic("pgfault: cant allocate new page - %e\n", res);
	
	memcpy(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE); //copy page
	res = sys_page_map(envid, PFTEMP, envid, ROUNDDOWN(addr, PGSIZE), PTE_W | PTE_U | PTE_P); //remap to new location
	if (res < 0)
		panic("pgfault: remapping failed - %e\n", res);

	res =  sys_page_unmap(envid, PFTEMP); // unmap temp location
	if (res < 0)
		panic("pgfault: unmmaping failed - %e\n", res);
}


// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  
//
// Returns: 0 on success, panicing on error.

static int
duppage(envid_t envid, unsigned pn)
{
	int res;
	void* va = (void*)(pn * PGSIZE);
	pte_t perm;
	
	if ((uvpt[pn] & PTE_SHARE)){
		res = sys_page_map(0, va, envid, va, uvpt[pn] & PTE_SYSCALL);
		if (res < 0)
			panic("duppage: Failed to map page from child to parent's shared page- %e", res);
	}
	 
	else if (uvpt[pn] & (PTE_W | PTE_COW)){ //should dupplicate page

		res = sys_page_map(0, va, envid, va, PTE_COW | PTE_U | PTE_P);
		if (res < 0)
			panic("duppage: cant change childEnv mapping - %e\n", res);	

		res = sys_page_map(0, va, 0, va, PTE_COW | PTE_U | PTE_P);
		if (res < 0)
			panic("duppage: cant change parentEnv mapping - %e\n", res);

	}
	else { //should not duplicate - read only or not COW
		res = sys_page_map(0, va, envid, va, PTE_U | PTE_P);
		if (res < 0)
			panic("duppage: cant change childEnv mapping(READ ONLY) - %e\n", res);	
	}
	return 0;
	
}


// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
//   Neither user exception stack are not marked as copy-on-write,
//   so we allocate a new page for the child's user exception stack.


void prepareChild(envid_t envid){
	extern void _pgfault_upcall(void); 
	intptr_t vadrr = 0;
	for (; vadrr < USTACKTOP; vadrr += PGSIZE)
	{
		if ((uvpd[PDX(vadrr)] & PTE_P) && (uvpt[PGNUM(vadrr)] & PTE_P)){
			duppage(envid, PGNUM(vadrr));
		}

	}
	int res = sys_page_alloc(envid, (void*) (UXSTACKTOP - PGSIZE), PTE_W | PTE_U | PTE_P); // allocate uxstack page
	if (res < 0)
		panic("prepareChild: cant allocate uxstack page -%e", res);

	res = sys_env_set_pgfault_upcall(envid, _pgfault_upcall); 
	if (res < 0)
		panic("prepareChild: cant set pgfault_upcall -%e", res);

	res = sys_env_set_status(envid, ENV_RUNNABLE);
	if (res < 0)
		panic("prepareChild: cant change child status to runable -%e", res);
}

envid_t
fork(void)
{

	extern void _pgfault_upcall(void); //Set up our page fault handler
	set_pgfault_handler(pgfault);
	int envid = sys_exofork(); //Create the child
	if (envid <0)
		panic("fork: sys_exfork faild - %e\n", envid);

	if (envid == 0){
		thisenv = &envs[ENVX(sys_getenvid())]; // setup thisenv extern val
	}
	
	else
		prepareChild(envid);


	
	return envid; // child id for parent, 0 for child
}


envid_t
monitoredFork(void)
{
	extern void _pgfault_upcall(void); //Set up our page fault handler
	set_pgfault_handler(pgfault);
	int envid = sys_monitored_exofork(); //Create a child
	if (envid <0)
		panic("fork: sys_monitor_exofork faild - %e\n", envid);

	if (envid == 0){
		thisenv = &envs[ENVX(sys_getenvid())]; // setup thisenv extern val
	}
	
	else
		prepareChild(envid);


	
	return envid; // child id for parent, 0 for child
}

int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}

 envid_t
priorityFork(int priority){
	extern void _pgfault_upcall(void); //Set up our page fault handler
	set_pgfault_handler(pgfault);
	int envid = sys_exofork(); //Create a child
	if (envid <0)
		panic("fork: sys_exfork faild - %e\n", envid);

	if (envid == 0){
		sys_set_priority(priority);
		thisenv = &envs[ENVX(sys_getenvid())]; // setup thisenv external value
	}
	else
		prepareChild(envid);

	return envid; // child id for parent, 0 for child
}
