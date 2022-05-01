// program to test instruction single step kernel monitor command

#include <inc/lib.h>
#include <inc/x86.h>

void
umain(int argc, char **argv)
{
	cprintf("Test stepInto Command\n");
	cprintf("breakpoint #1\n");
    breakpoint();
	//use stepInto
	int i;
	for(i=1; i <= 5; i++){
		cprintf("step %v%s\n", 0x0100, i);
	}
	
    cprintf("breakpoint #2\n");
    breakpoint();
	
	//use continue
    cprintf("Continued!\n");
	
	for(i=1; i <= 5; i++){
		cprintf("step %v%s\n", 0x0100, i);
	}

    cprintf("Test 'stepInto': %v%s\n", 0x0200, "PASS");
}

