#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>
#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/picirq.h>
#include <inc/trap.h>
#include <inc/string.h>
#include <kern/env.h>


/* ==========================================================
		        E1000 API
   ========================================================== */

int e1000_attach(struct pci_func *pcif);
int e1000_transmit(struct PageInfo* pp, size_t size);
int e1000_receive(struct PageInfo** pp_pointer);
void e1000_trap_handler();
uint16_t readMACFromEEPROM(uint32_t addr);

// Table 5-1. Component Identification  - in section 5.2 of intel's manual - page 109
/*      +------------+-----------+-----------+-------------+
        |  Stepping  | Vendor ID | Device ID | Description |
        +------------+-----------+-----------+-------------+
        | 82541GI-B1 |   8086h   |   1076h   |    Cooper   |
        +------------+-----------+-----------+-------------+
        | 82541GI-B1 | 8086h     | 1077h     | Mobile      |
        +------------+-----------+-----------+-------------+
        | 82541PI-C0 | 8086h     | 1076h     | Cooper      |
        +------------+-----------+-----------+-------------+
        | 82541ER-C0 | 8086h     | 1078h     | Cooper      |
        +------------+-----------+-----------+-------------+
        | 82540EP-A  | 8086h     | 1017      | Desktop     |
        +------------+-----------+-----------+-------------+
        | 82540EP-A  | 8086h     | 1016      | Mobile      |
        +------------+-----------+-----------+-------------+
        | 82540EM-A  | 8086h     | 100E      | Desktop     | --> USED
        +------------+-----------+-----------+-------------+
        | 82540EM-A  | 8086h     | 1015      | Mobile      |
        +------------+-----------+-----------+-------------+ */

#define PCI_E1000_VENDOR 0x8086
#define PCI_E1000_DEVICE 0x100E


#define BYTE_T0_ADDRESS(BYTE_NUM) ((BYTE_NUM) / (4))

//E1000 REGISTERS
#define E1000_STATUS   0x00008  /* Device Status - RO */
#define E1000_EERD     0x00014  /* EEPROM Read - RW */


#define SIZE_OF_PACKET 1518 // size in bytes of a tcp packet
typedef void * packetBuff; // debug - void* /*ptr of va to packet page*/


/* EERD bits*/
#define E1000_EERD_START        0x00000001
#define E1000_EERD_DONE         0x00000010
#define E1000_EERD_DATA         16
#define E1000_EERD_MAC_LOW      0x00000000
#define E1000_EERD_MAC_MID      0x00000100
#define E1000_EERD_MAC_HIGH     0x00000200




/* Register Set. (82543, 82544)
 *
 * Registers are defined to be 32 bits and  should be accessed as 32 bit values.
 * These registers are physically located on the NIC, but are mapped into the
 * host memory address space.
 *
 * RW - register is both readable and writable
 * RO - register is read only
 * WO - register is write only
 * R/clr - register is read only and is cleared when read
 * A - register array
 */

#define E1000_EERD     0x00014  /* EEPROM Read - RW */
#define E1000_ICR      0x000C0  /* Interrupt Cause Read - R/clr */
#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */
#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */
#define E1000_RDTR     0x02820  /* RX Delay Timer - RW */
#define E1000_MTA      0x05200  /* Multicast Table Array - RW Array */
#define E1000_RA       0x05400  /* Receive Address - RW Array */
#define E1000_ICR_RXT0          0x00000080 /* rx timer intr (ring 0) */


// Interrupt Mask Set 

#define E1000_IMS_RXT0      E1000_ICR_RXT0      /* rx timer intr */



// =================================== Transmiter definitions ===================================

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
#define E1000_TXD_CMD_EOP    0x01       /* End of Packet */
#define E1000_TXD_CMD_IFCS   0x02       /* Insert FCS (Ethernet CRC) */
#define E1000_TXD_CMD_IC     0x04       /* Insert Checksum */
#define E1000_TXD_CMD_RS     0x08       /* Report Status */
#define E1000_TXD_CMD_RPS    0x10       /* Report Packet Sent */
#define E1000_TXD_CMD_DEXT   0x20       /* Descriptor extension (0 = legacy) */
#define E1000_TXD_CMD_VLE    0x40       /* Add VLAN tag */
#define E1000_TXD_CMD_IDE    0x80       /* Enable Tidv register */
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
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */

//TCTL bit shifts
#define E1000_TCTL_COLD_SHIFT   12

// IPG Register
#define E1000_TIPG_IPGT   10
#define E1000_TIPG_IPGT_SHIFT  0
#define E1000_TIPG_IPGR1 10
#define E1000_TIPG_IPGR1_SHIFT  10
#define E1000_TIPG_IPGR2 10
#define E1000_TIPG_IPGR2_SHIFT  20



// =================================== Receiver definitions ===================================

struct rx_desc
{
        uint64_t addr; 
        uint16_t length; 
        uint16_t checksum;  
        uint8_t status;  
        uint8_t erros;   
        uint16_t special; 
} __attribute__((packed)); 

#define E1000_RX_DESC_NUM 128 //number of rx descriptors


/* Receive Descriptor bit definitions */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */

/* Receive Address */
#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */

/* Receive Control */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */


#endif	// JOS_KERN_E1000_H
