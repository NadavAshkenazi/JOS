// hello, world
#include <inc/lib.h>


/* ==========================================================
				optional: priority fork test
   ========================================================== */


// void
// umain(int argc, char **argv)
// {
// 	int i = 1;
// 	for (; i <= 4; ++i){
// 		int pid = priorityFork(i);
// 		if (pid == 0) {
// 			cprintf("created child - %d\n", i);
// 			int j;
// 			for (j = 0; j < 3; ++j) {
// 				cprintf("child %d - sys_yield\n", i);
// 				sys_yield();
// 			}
// 			break;
// 		}
// 	}
// }



void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
	cprintf("thisenv: %x\n", thisenv);
	cprintf("i am environment %08x\n", thisenv->env_id);
}
