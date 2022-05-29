#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

// 82540EM (DESKTOP)  - in section 5.2 of intel's manual - page 109
#define PCI_E1000_VENDOR 0x8086
#define PCI_E1000_DEVICE 0x100E


#define BYTE_T0_ADDRESS(BYTE_NUM) ((BYTE_NUM) / (4))

//E1000 REGISTERS
#define E1000_STATUS   0x00008  /* Device Status - RO */
#define E1000_EERD     0x00014  /* EEPROM Read - RW */


#define SIZE_OF_PACKET 1518 // size in bytes of a tcp packet
typedef uintptr_t packetBuf; // debug - void* /*ptr of va to packet page*/
//tx

struct tx_desc
{
        uint64_t addr; //buffer_address - Address of the transmit descriptor in the host memory
        uint16_t length; //Length is per segment
        uint8_t cso; // Checksum Offset
        uint8_t cmd; // Command field
        uint8_t status; // Status field
        uint8_t css; // Checksum Start Field
        uint16_t special; // Special Field
} 
__attribute__((packed)); // compact without padding

#define E1000_TX_DESC_NUM 64 //number of tx descriptors

/* Transmit Descriptor bit definitions */
#define E1000_TXD_DTYP_D     0x00100000 /* Data Descriptor */
#define E1000_TXD_DTYP_C     0x00000000 /* Context Descriptor */
#define E1000_TXD_POPTS_IXSM 0x01       /* Insert IP checksum */
#define E1000_TXD_POPTS_TXSM 0x02       /* Insert TCP/UDP checksum */
#define E1000_TXD_CMD_EOP    0x01000000 /* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02000000 /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC     0x04000000 /* Insert Checksum */
#define E1000_TXD_CMD_RS     0x08000000 /* Report Status */
#define E1000_TXD_CMD_RPS    0x10000000 /* Report Packet Sent */
#define E1000_TXD_CMD_DEXT   0x20000000 /* Descriptor extension (0 = legacy) */
#define E1000_TXD_CMD_VLE    0x40000000 /* Add VLAN tag */
#define E1000_TXD_CMD_IDE    0x80000000 /* Enable Tidv register */
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */
#define E1000_TXD_STAT_EC    0x00000002 /* Excess Collisions */
#define E1000_TXD_STAT_LC    0x00000004 /* Late Collisions */
#define E1000_TXD_STAT_TU    0x00000008 /* Transmit underrun */
#define E1000_TXD_CMD_TCP    0x01000000 /* TCP packet */
#define E1000_TXD_CMD_IP     0x02000000 /* IP packet */
#define E1000_TXD_CMD_TSE    0x04000000 /* TCP Seg enable */
#define E1000_TXD_STAT_TC    0x00000004 /* Tx Underrun */

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

// #define E1000_TXCW     0x00178  /* TX Configuration Word - RW */
// #define E1000_TCTL     0x00400  /* TX Control - RW */
// #define E1000_TCTL_EXT 0x00404  /* Extended TX Control - RW */
// #define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
// #define E1000_TBT      0x00448  /* TX Burst Timer - RW */
// #define E1000_TXDMAC   0x03000  /* TX DMA Control - RW */
// #define E1000_TDFH     0x03410  /* TX Data FIFO Head - RW */
// #define E1000_TDFT     0x03418  /* TX Data FIFO Tail - RW */
// #define E1000_TDFHS    0x03420  /* TX Data FIFO Head Saved - RW */
// #define E1000_TDFTS    0x03428  /* TX Data FIFO Tail Saved - RW */
// #define E1000_TDFPC    0x03430  /* TX Data FIFO Packet Count - RW */
// #define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
// #define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
// #define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
// #define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
// #define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
// #define E1000_TIDV     0x03820  /* TX Interrupt Delay Value - RW */
// #define E1000_TXDCTL   0x03828  /* TX Descriptor Control - RW */
// #define E1000_TADV     0x0382C  /* TX Interrupt Absolute Delay Val - RW */
// #define E1000_TARC0    0x03840  /* TX Arbitration Count (0) */
// #define E1000_TDBAL1   0x03900  /* TX Desc Base Address Low (1) - RW */
// #define E1000_TDBAH1   0x03904  /* TX Desc Base Address High (1) - RW */
// #define E1000_TDLEN1   0x03908  /* TX Desc Length (1) - RW */
// #define E1000_TDH1     0x03910  /* TX Desc Head (1) - RW */
// #define E1000_TDT1     0x03918  /* TX Desc Tail (1) - RW */
// #define E1000_TXDCTL1  0x03928  /* TX Descriptor Control (1) - RW */
// #define E1000_TARC1    0x03940  /* TX Arbitration Count (1) */



//rx

struct rx_desc
{
        uint64_t addr; 
        uint16_t length; 
        uint8_t checksum;  
        uint8_t status;  
        uint8_t erros;   
        uint16_t special; 
} __attribute__((packed)); 

#define E1000_RX_DESC_NUM 128 //number of rx descriptors


int e1000_attach(struct pci_func *pcif); 
// int e1000_transmit(struct PageInfo *payload_pp, size_t nbytes);
// int e1000_receive(struct PageInfo **dest_pp);
// void e1000_receive_interrupt_handler();


#endif	// JOS_KERN_E1000_H
