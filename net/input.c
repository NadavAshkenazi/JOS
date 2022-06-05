#include "ns.h"

extern union Nsipc nsipcbuf;

static envid_t idle_envid;

#define DELAY_TIME 45
void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.


	sys_page_unmap(0, &nsipcbuf); //clear buffer //XXX

	while (1)
	{
		// cprintf("in input\n"); //XXX
		sys_page_alloc(0, &nsipcbuf, PTE_U | PTE_W | PTE_P); // aloc new page with read/write prem
		
		int res = sys_receive(&nsipcbuf);
		if (res < 0)
			panic("input: %e\n", res);

		else if (res > PGSIZE - sizeof(struct jif_pkt))
			panic("input: packet too big for page -> %e\n", res);

		//	ipc to server that new packet was received
		memmove(nsipcbuf.pkt.jp_data, &nsipcbuf, res);
		nsipcbuf.pkt.jp_len = res;
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_W | PTE_U);
		// cprintf("input sent ipc\n"); //XXX
		int i = 0;
		for (; i < DELAY_TIME; i++){// don't immediately receive another packet in to the same physical page
			sys_yield();
		} 
		res = sys_page_unmap(0, &nsipcbuf);
		if (res < 0)
			panic("input: could not unmap page -> %e\n", res);

	}
}
