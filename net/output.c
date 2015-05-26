#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
	int ret;
        while(1){

                ret = ipc_recv(NULL, &nsipcbuf, NULL);
                if(ret < 0){
                        return;
                }
                while((ret=sys_packet_transmit(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len)) < 0){
			sys_yield();
		}
	}
}
