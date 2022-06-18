#include "ns.h"
// #include <lib/ns.h>

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// 	- read a packet from the network server
	//	- send the packet to the device driver
	int len;
	envid_t output_env;

	while (true)
	{
		int res = sys_page_alloc(0, &nsipcbuf, PTE_U|PTE_W|PTE_P);
		if (res < 0)
			panic("output: allocation problem -> %e", res);


		// spin until ipc req comes
		while (ipc_recv(&output_env, &nsipcbuf, NULL) != NSREQ_OUTPUT || output_env != ns_envid)
			;

		// send the packet to the device driver
		// Will send the page given via polling
		len = nsipcbuf.pkt.jp_len;
		memmove(&nsipcbuf, nsipcbuf.pkt.jp_data, len);

		while(sys_transmit(&nsipcbuf, len) <0){
			sys_yield();
		};

		res = sys_page_unmap(0, &nsipcbuf);
		if (res < 0)
			panic("output: could not unmap page -> %e", res);


		sys_yield();
	}
}
