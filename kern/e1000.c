#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/picirq.h>
#include <inc/trap.h>

// LAB 6: Your driver code here

 

volatile uint32_t *e1000Va;//registers addr
struct tx_desc txDescriptorsArray[E1000_TX_DESC_NUM] 
    __attribute__((aligned(16)));

packetBuff txBuffers[E1000_TX_DESC_NUM]; 

struct rx_desc rxDescriptorsArray[E1000_RX_DESC_NUM]
     __attribute__((aligned(16)));

packetBuff rxBuffers[E1000_TX_DESC_NUM];

;
int e1000_attach(struct pci_func *pcif)
{
    pci_func_enable(pcif);

    // // Init MMIO Region
    e1000Va = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

    /*debug of mmio mapping */
    
    assert(0x80080783 == *(e1000Va + (BYTE_T0_ADDRESS(E1000_STATUS))));
    cprintf("E1000 status: 0x%08x should be 0x80080783\n",*(e1000Va + (BYTE_T0_ADDRESS(E1000_STATUS))));
    // // Set the interrupts channel
    // irq_setmask_8259A(irq_mask_8259A & ~(1 << IRQ_E1000));

    // e1000_load_mac_addr();
    // e1000_tx_init();
    // e1000_rx_init();

    return 0;
}


static inline int e100_txDescs_init(){
    /*  Allocate a region of memory for the transmit descriptor list. Software should insure this memory is
        aligned on a paragraph (16-byte) boundary */
    memset(txDescriptorsArray, 0, sizeof(txDescriptorsArray));
    memset(txBuffers, 0, sizeof(txBuffers));

    //setup registers
    /*  Program the Transmit Descriptor Base Address
        (TDBAL/TDBAH) register(s) with the address of the region */
    *(e1000Va + BYTE_T0_ADDRESS(E1000_TDBAL)) = PADDR(txDescriptorsArray);
    *(e1000Va + BYTE_T0_ADDRESS(E1000_TDBAH)) = 0x0;
    /*  Set the Transmit Descriptor Length (TDLEN) register to the size (in bytes) of the descriptor ring.
        This register must be 128-byte aligned - aleardy to 16-b aligned */
    *(e1000Va + BYTE_T0_ADDRESS(E1000_TDBAH)) = sizeof(txDescriptorsArray);
    /*  The Transmit Descriptor Head and Tail (TDH/TDT) registers are initialized (by hardware) to 0b
        after a power-on or a software initiated Ethernet controller reset */
    *(e1000Va + BYTE_T0_ADDRESS(E1000_TDH)) = 0; // head index 0 at init
    *(e1000Va + BYTE_T0_ADDRESS(E1000_TDT)) = 0; // tail index 0 at init



}
