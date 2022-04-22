
#include <inc/lib.h>
#include <inc/x86.h>

void
umain(int argc, char **argv)
{
	cprintf("Test Continue Command\n");
	cprintf("breakpoint #1\n");
    breakpoint();

    cprintf("Continued!\n");
	cprintf("running some code...")

    int x = 1;
	x = 2;
	x = 3;
	
	cprintf("done.\n")

    cprintf("breakpoint #2\n");
    breakpoint();

    cprintf("Continued!\n");

    cprintf("Test 'Continue': %v%s\n", 0x0200, "PASS");
}

