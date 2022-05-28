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




int e1000_attach(struct pci_func *pcif); 
// int e1000_transmit(struct PageInfo *payload_pp, size_t nbytes);
// int e1000_receive(struct PageInfo **dest_pp);
// void e1000_receive_interrupt_handler();


#endif	// JOS_KERN_E1000_H
