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

int showMappings(int argc, char **argv, struct Trapframe *tf);
int changePremissions(int argc, char **argv, struct Trapframe *tf);
int dumpMem(int argc, char **argv, struct Trapframe *tf);

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
	{"showmappings", "Display in a useful and easy-to-read format all of the physical page mapping", showMappings},
	{"changepremissions", "Explicitly set, clear, or change the permissions of any mapping in the current address space", changePremissions},
	{"dumpmem", "Dump the contents of a range of memory given either a virtual or physical address range", dumpMem}
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

uint32_t string2address(char* input){
	uint32_t address = 0;
	if ((input[0] != '0') || (input[1] != 'x')){
		return -1;
	}

	input++;
	while (*(++input) != 0){
		uint32_t translation = 0;
		if('a' <= *input && *input <= 'f'){
			translation = *input-'a'+10;
		}
		else if('A' <= *input && *input <= 'F'){
			translation = *input-'A'+10;
		}
		else if('0' <= *input && *input <= '9'){
			translation = *input-'0';
		}
		else{
			return (uint32_t)0xFFFFFFFF;
		}
		address = address*16 + translation;
	}
	return address;
}

int 
bitExtracted(int number, int k, int p)
{
    return (((1 << k) - 1) & (number >> (p - 1)));
}

void 
printPTE(pte_t *pte){
	cprintf("P_Flag(Present bit): %d, W_Flag(Writing bit): %d, U_Flag(User bit): %d\n",
		 bitExtracted((int)(*pte&PTE_P), 1, 1), bitExtracted((int)(*pte&PTE_W), 1, 2), bitExtracted((int)(*pte&PTE_U), 1, 3));
}

int
showMappings(int argc, char **argv, struct Trapframe *tf){
	if (argc < 3){
		cprintf("to use showmappings run the following line: \n");
		cprintf("showmappings start(hex) end(hex)\n");
		return 1;
	}

	uint32_t start = string2address(argv[1]);
	uint32_t end = string2address(argv[2]);
	
	if ((start == -1) || (end == -1)){
		cprintf("please use addresses in hex format: <0x********>\n");
		return 1;
	}
	cprintf("start: %x, end: %x\n", string2address(argv[1]), string2address(argv[2]));

	for (; start <= end; start += PGSIZE) {
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


int 
stringCompare(const char *s1, const char *s2) // 1 if equel, 0 otherwise;
{
    while (*s1)
    {
        // if characters differ, or end of the second string is reached
        if (*s1 != *s2) {
            break;
        }
 
        // move to the next pair of characters
        s1++;
        s2++;
    }
 
    // return the ASCII difference after converting `char*` to `unsigned char*`
    int res = *(const unsigned char*)s1 - *(const unsigned char*)s2;
	
	return (res == 0);
}

int
changePremissions(int argc, char **argv, struct Trapframe *tf){
	if (argc < 4){
		cprintf("to use changepremissions run the following line: \n");
		cprintf("changepremissions address(hex) <set|clear> <P|W|U>\n");
		return 1;
	}

	uint32_t address = string2address(argv[1]);
	if (address == -1){
		cprintf("please use addresses in hex format: <0x********>\n");
		return 1;
	}

	char* action = argv[2];
	int newPremVal = 0; // default clear;

	if (!(stringCompare(action, "set")) && !(stringCompare(action, "clear"))){
		cprintf("please enter a valid action: <set|clear>\n");
		return 1;
	}
	else if (stringCompare(action, "set")){
		newPremVal = 1; // if set: 1 else clear
	}

	char* prem = argv[3];
	if ((prem[0] != 'P') && (prem[0] != 'U') && (prem[0] != 'W')){
		cprintf(prem);
		cprintf("\n");
		cprintf("prem is U: %d\n", stringCompare(prem, "U"));

		cprintf("please enter a valid premission flag: <P|W|U>\n");
		return 1;
	}

	pte_t *pte = pgdir_walk(kern_pgdir, (void *) address, 1);	
	cprintf("PTE of %x changed from:\n", address);
	printPTE(pte);
	switch (*prem){
		case 'P':
			if (newPremVal == 1)
				*pte =  *pte | PTE_P;
			else
				*pte =  *pte & ~PTE_P;
			break;
		case 'U':
			if (newPremVal == 1)
				*pte =  *pte | PTE_U;
			else
				*pte =  *pte & ~PTE_U;
			break;
		case 'W':
			if (newPremVal == 1)
				*pte =  *pte | PTE_W;
			else
				*pte =  *pte & ~PTE_W;
			break;
		default:
			panic("changePremissions: Invalid action");
			break;
	}

	cprintf("to:\n");
	printPTE(pte);
	return 0;
}


int stringToInt(char* s)
{
    int res = 0;
	int i = 0;
    for (; s[i] != '\0'; ++i)
        res = res * 10 + s[i] - '0';
 
    return res;
}


void
dumpVMem_aux(uintptr_t address, uint32_t range){

	uintptr_t* va = (uintptr_t*) address;
	cprintf("V addresss|content \n");

	int i = 0;
	for (; i< range; i++){
		cprintf("  %x|%x\n", va + i, *(va + i));
	}
	
}

void
dumpPMem_aux(uintptr_t address, uint32_t range){

	uintptr_t* pa = (uintptr_t*) address;
	uintptr_t* va;
	cprintf("  %x|%x\n");

	int i = 0;
	for (; i< range; i++){
		va = KADDR((physaddr_t)(pa + i));
		cprintf("V adrress: %x, content: %x\n", pa + i, *va);
	}
}



int
dumpMem(int argc, char **argv, struct Trapframe *tf){
	if (argc < 4){
		cprintf("to use dumpmem run the following line: \n");
		cprintf("dumpmem memType:<V|P> address(hex) range of addresses(int)\n");
		return 1;
	}

	char* type = argv[1];
	if ((type[0] != 'V') && (type[0] != 'P')){
		cprintf("please use valid mem type <V|P>\n");
		return 1;
	}

	uintptr_t address = (uintptr_t) strtol(argv[2], NULL, 16);
	if (address == -1){
		cprintf("please use addresses in hex format: <0x********>\n");
		return 1;
	}

	if (address % 4){
		cprintf("please use addresses aligned to 32b\n");
		return 1;
	}

	uint32_t i = 0;
	uint32_t range = stringToInt(argv[3]);

	if (type[0] == 'V')
		dumpVMem_aux(address, range);
	else
		dumpPMem_aux(address, range);

	return 0;
}