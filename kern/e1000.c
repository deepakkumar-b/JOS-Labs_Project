#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>
// LAB 6: Your driver code here

volatile uint32_t *em82450_mmio_addr;

struct tx_desc desc_list[DESC_SIZE];
char packet_buffer[DESC_SIZE][PACKET_SIZE];

struct rx_desc rx_desc_list[RX_DESC_SIZE];
char rx_packet_buf[RX_DESC_SIZE][RX_PACKET_SIZE];

int em82540_attach(struct pci_func *pcifunc)
{
	pci_func_enable(pcifunc);
	//exe4 from here
	em82450_mmio_addr = mmio_map_region(pcifunc->reg_base[0],pcifunc->reg_size[0]); 
//	cprintf("Status Register = ",*(em82450_mmio_addr + 2));
//	memset(desc_list, 0x0, sizeof(desc_list));
//	memset(packet_buffer, 0x0, sizeof(packet_buffer));
//	memset(rx_desc_list, 0x0, sizeof(rx_desc_list));
//      memset(rx_packet_buf, 0x0, sizeof(rx_packet_buf));
	em_initialization();	
	return 1;
}

void em_initialization()
{
	int i;
	em82450_mmio_addr[E1000_TDBAL] = PADDR(desc_list);
	em82450_mmio_addr[E1000_TDLEN] = sizeof(desc_list);
	em82450_mmio_addr[E1000_TDH] = 0x0;
        em82450_mmio_addr[E1000_TDT] = 0x0;
	em82450_mmio_addr[E1000_TCTL] |= E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT | E1000_TCTL_COLD;
	em82450_mmio_addr[E1000_TIPG] = 0x0060200A;
	for(i=0; i < DESC_SIZE; i++ ){
                desc_list[i].addr = PADDR(packet_buffer[i]);
                desc_list[i].status |= E1000_TXD_STAT_DD;
        }
	//Receiver Side from here
        em82450_mmio_addr[E1000_RAL] = 0x12005452;
        em82450_mmio_addr[E1000_RAH] = 0x80005634;

        em82450_mmio_addr[E1000_RDBAL] = PADDR(rx_desc_list);
        em82450_mmio_addr[E1000_RDLEN] = sizeof(rx_desc_list);

        em82450_mmio_addr[E1000_RDH] = 0x0;
        em82450_mmio_addr[E1000_RDT] = 0x0;
        em82450_mmio_addr[E1000_MTA] = 0;
        em82450_mmio_addr[E1000_RCTL] |= E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC;
	for(i=0; i<RX_DESC_SIZE; i++){
                rx_desc_list[i].addr = PADDR(rx_packet_buf[i]);
        }
}

int em_transmit(char *data, size_t len)
{

	uint32_t tdt = em82450_mmio_addr[E1000_TDT];
	if(len > PACKET_SIZE){
		cprintf("Packet size too long \n");
		return -1;
	}
	if(!(desc_list[tdt].status & E1000_TXD_STAT_DD)){
		return -1;
	}
	memmove(packet_buffer[tdt], data, len);
	desc_list[tdt].length = len;
	desc_list[tdt].cmd |= E1000_TXD_CMD_RS || E1000_TXD_CMD_EOP;
//	desc_list[tdt].status &= ~E1000_TXD_STAT_DD;
	em82450_mmio_addr[E1000_TDT] = ((tdt + 1) % DESC_SIZE);
	return 0;
}

int em_receive(char *data)
{
	uint32_t rdt;
        int len;
        rdt = em82450_mmio_addr[E1000_RDT];
        if(!(rx_desc_list[rdt].status & E1000_RXD_STAT_DD)){
                return -1;
        }
        len=rx_desc_list[rdt].length;
        memmove(data,rx_packet_buf[rdt],len);
        rx_desc_list[rdt].status = 0x0;
        em82450_mmio_addr[E1000_RDT] = ((rdt+1) % RX_DESC_SIZE);
        return len;
}
