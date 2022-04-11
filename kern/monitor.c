// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>


#define CMDBUF_SIZE	80	// enough for one VGA text line

int showmappings(int argc, char **argv, struct Trapframe *tf);

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Trace previous functions from the stack ", mon_backtrace},
	{"showmappings", "Display in a useful and easy-to-read format all of the physical page mapping", showmappings}
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{	
	uint32_t* ebp = (uint32_t*) read_ebp();
	struct Eipdebuginfo info;
	cprintf("Stack backtrace:\n");
	while (ebp) {
	    uint32_t* eip = ebp+1;
	    uint32_t* args = ebp+2;
            debuginfo_eip(*eip, &info);
	    cprintf("ebp %08x  eip %08x  args", ebp, *(eip));
	    cprintf(" %08x %08x %08x %08x %08x\n", args[0], args[1], args[2], args[3], args[4]);
	    cprintf("\t%s:%u: %.*s+%u\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, (*eip)-info.eip_fn_addr);
	    ebp = (uint32_t*) *ebp;
	  }
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");
	 cprintf("%v%s\n%v%s\n%v%s\n", 
	    0x0100, "blue", 0x0200, "green", 0x0400, "red");

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}

uint32_t string2address(char* buff){
	uint32_t address = 0;
	buff++;
	while (*(++buff) != 0){
		uint32_t translation = 0;
		if('a' <= *buff && *buff <= 'f'){
			translation = *buff-'a'+10;
		}
		else if('A' <= *buff && *buff <= 'F'){
			translation = *buff-'A'+10;
		}
		else if('0' <= *buff && *buff <= '9'){
			translation = *buff-'0';
		}
		else{
			return (uint32_t)0xFFFFFFFF;
		}
		address = address*16 + translation;
	}
	return address;
}

void printPTE(pte_t *pte){
	cprintf("P_Flag(Present bit): %x, W_Flag(Writing bit): %x, U_Flag(User bit): %x\n",
		 *pte&PTE_P, *pte&PTE_W, *pte&PTE_U);
}

int
showmappings(int argc, char **argv, struct Trapframe *tf){
	if (argc < 3){
		cprintf("to use shoemappings run the following line: \n");
		cprintf("showmappings start(hex) end(hex)\n");
	}

	uint32_t start, end;
	cprintf("start: %x, end: %x\n", string2address(argv[1]), string2address(argv[2]));
	for (start = string2address(argv[1]); start <= string2address(argv[2]); start += PGSIZE) {
		pte_t *pte = pgdir_walk(kern_pgdir, (void *) start, 1);	
		if (!pte) 
			panic("boot_map_region: can't allocate pte\n");

		if (*pte & PTE_P) {
			cprintf("page at %x with ", start);
			printPTE(pte);
		} 
		
		else
			cprintf("page at %x does not exist! \n", start);
	}
	return 0;
}