#include "ns.h"
// #include <lib/ns.h>

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	envid_t sender;
	int length;

	while (1)
	{
		int res = sys_page_alloc(0, &nsipcbuf, PTE_U|PTE_W|PTE_P);
		if (res < 0)
			panic("output: allocation problem -> %e", res);

		// 	- read a packet from the network server
		// Output is on a constant listen for output requests 
		// solely from the network server
		// REMEMBER: ipc_recv is a blocking call
		while (ipc_recv(&sender, &nsipcbuf, NULL) != NSREQ_OUTPUT || sender != ns_envid)
			;

		//	- send the packet to the device driver
		// Will send the page given via polling
		length = nsipcbuf.pkt.jp_len;
		memmove(&nsipcbuf, nsipcbuf.pkt.jp_data, length);

		sys_transmit(&nsipcbuf, length);

		if (sys_page_unmap(0, &nsipcbuf) < 0)
			panic("Failed to unmap sent nsipcbuf page");


		sys_yield();
	}
}
