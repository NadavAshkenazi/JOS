#include "ns.h"

extern union Nsipc nsipcbuf;

static envid_t idle_envid;

#define DELAY_TIME 45
void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// 	- read a packet from the device driver
	//	- send it to the network server
	// When we IPC a page to the network server, it will be
	// reading from it for a while, so wedon't immediately receive
	// another packet in to the same physical page.


	sys_page_unmap(0, &nsipcbuf); //clear buffer 

	while (1)
	{
		sys_page_alloc(0, &nsipcbuf, PTE_U | PTE_W | PTE_P); // alloc new page with read/write prem
		
		while(true){

		int res;
		do{
			res = sys_receive(&nsipcbuf);
		}
		while(res < 0);

		// ipc to server that new packet was received
		memmove(nsipcbuf.pkt.jp_data, &nsipcbuf, res);
		nsipcbuf.pkt.jp_len = res;
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_W | PTE_U);
		int i = 0;
		for (; i < DELAY_TIME; i++){ // don't immediately receive another packet in to the same physical page
			sys_yield();
		} 
		res = sys_page_unmap(0, &nsipcbuf);
		if (res < 0)
			panic("input: could not unmap page -> %e\n", res);
		}



	}
}
