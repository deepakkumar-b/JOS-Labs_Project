#include "ns.h"

extern union Nsipc nsipcbuf;

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
	int length;
	memset(nsipcbuf.pkt.jp_data, 0, 2048);
	while(1){
		length = sys_packet_receive(nsipcbuf.pkt.jp_data);
                if(length < 0){
                        sys_yield();
                        continue;
                }
		nsipcbuf.pkt.jp_len = length;
		ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, (PTE_U | PTE_W | PTE_P));
                receive_sleep(10);
        }
}

void
static receive_sleep(int msec)
{
        unsigned now = sys_time_msec();
        unsigned end = now + msec*10;

        if ((int)now < 0 && (int)now > -MAXERROR)
                panic("sys_time_msec: %e", (int)now);
        if (end < now)
                panic("sleep: wrap");

        while (sys_time_msec() < end)
                sys_yield();
}
