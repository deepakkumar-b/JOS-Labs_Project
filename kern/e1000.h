#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>
//For transmit
#define DESC_SIZE	64
#define PACKET_SIZE	1518
//For receive
#define RX_DESC_SIZE   64
#define RX_PACKET_SIZE 2048

//Transmit REGISTERS
#define E1000_TDBAL    0x03800/4  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804/4  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808/4  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810/4  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818/4  /* TX Descripotr Tail - RW */
#define E1000_TCTL     0x00400/4  /* TX Control - RW */
#define E1000_TIPG     0x00410/4  /* TX Inter-packet gap -RW */
#define E1000_STATUS   0x00008/4  /* Device Status - RO */

#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */
#define E1000_TXD_CMD_EOP    0x01000000 /* End of Packet */
#define E1000_TXD_CMD_RS     0x08000000 /* Report Status */

#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x10000000    /* collision distance */

//Receive Registers
#define E1000_RDBAL    0x02800/4  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804/4  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808/4  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810/4  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818/4  /* RX Descriptor Tail - RW */
#define E1000_RAL      0x05400/4
#define E1000_RAH      0x05404/4
#define E1000_RCTL     0x00100/4  /* RX Control - RW */
#define E1000_MTA      0x05200/4 /* Multicast Table Array - RW Array */

#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

struct rx_desc
{
        uint64_t addr;
        uint16_t length;
        uint16_t pktchecksum;
        uint8_t status;
        uint8_t errors;
        uint16_t special;
};

int em82540_attach(struct pci_func *); //minesh
void em_initialization();
int em_transmit(char *data, size_t len);
int em_receive(char *data);

#endif	// JOS_KERN_E1000_H
