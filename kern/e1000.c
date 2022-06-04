#include <kern/e1000.h>


// LAB 6: Your driver code here

 

volatile uint32_t *e1000Va;//registers addr
struct tx_desc txDescriptorsArray[E1000_TX_DESC_NUM] 
    __attribute__((aligned(16)));

// packetBuff txBuffers[E1000_TX_DESC_NUM]; 

struct rx_desc rxDescriptorsArray[E1000_RX_DESC_NUM]
     __attribute__((aligned(16)));

// packetBuff rxBuffers[E1000_TX_DESC_NUM];




static inline void e100_txDescs_init();


int e1000_attach(struct pci_func *pcif)
{
    pci_func_enable(pcif);

    // // Init MMIO Region
    e1000Va = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

    /*debug of mmio mapping */
    
    assert(0x80080783 == *(e1000Va + (BYTE_T0_ADDRESS(E1000_STATUS))));
    cprintf("E1000 status: 0x%08x should be 0x80080783\n",*(e1000Va + (BYTE_T0_ADDRESS(E1000_STATUS))));
    
    
    e100_txDescs_init();
    // // Set the interrupts channel
    // irq_setmask_8259A(irq_mask_8259A & ~(1 << IRQ_E1000));

    // e1000_load_mac_addr();
    // e1000_tx_init();
    // e1000_rx_init();

    return 0;
}


static inline void e100_txDescs_init(){
    /*  Allocate a region of memory for the transmit descriptor list. Software should insure this memory is
        aligned on a paragraph (16-byte) boundary */
    memset(txDescriptorsArray, 0, sizeof(txDescriptorsArray));
    // memset(txBuffers, 0, sizeof(txBuffers));

    //init txDescriptorsArray
    int j = 0;
    for (; j< E1000_TX_DESC_NUM; j++){
        txDescriptorsArray[j].cmd |= E1000_TXD_CMD_EOP | E1000_TXD_CMD_RPS | E1000_TXD_CMD_IDE;
        txDescriptorsArray[j].status |= E1000_TXD_STAT_DD;
    }
    //setup registers
    /*  Program the Transmit Descriptor Base Address
        (TDBAL/TDBAH) register(s) with the address of the region */
    *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TDBAL)) = PADDR(txDescriptorsArray);
    *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TDBAH)) = 0x0;
    /*  Set the Transmit Descriptor Length (TDLEN) register to the size (in bytes) of the descriptor ring.
        This register must be 128-byte aligned - aleardy to 16-b aligned */
    *(uint16_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TDLEN)) = sizeof(txDescriptorsArray);
    /*  The Transmit Descriptor Head and Tail (TDH/TDT) registers are initialized (by hardware) to 0b
        after a power-on or a software initiated Ethernet controller reset */
    *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TDH)) = 0; // head index 0 at init
    *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TDT)) = 0; // tail index 0 at init
    /* Initialize the Transmit Control Register (TCTL) for desired operation to include the following:*/
        //• Set the Enable (TCTL.EN) bit to 1b for normal operation. 
        //• Set the Pad Short Packets (TCTL.PSP) bit to 1b. */

    // config TCTL
    *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TCTL)) |= 0x0; 
    *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TCTL)) = (E1000_TCTL_EN | E1000_TCTL_PSP);
    *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TCTL)) |= E1000_TCTL_CT; //no meaning in full-duplex
    *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TCTL)) |=  (0x40 << E1000_TCTL_COLD_SHIFT); //no meaning in full-duplex XXX

    // config TIPG
    *(uint16_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TIPG)) = (uint16_t)(E1000_TIPG_IPGT << E1000_TIPG_IPGT_SHIFT | E1000_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT | E1000_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT); //XXX

}

inline int e1000_transmit(struct PageInfo* pp, size_t size){
    if (size > SIZE_OF_PACKET)
        panic("e1000_transmit: size requested larger than packet");


    int tailIndex = *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TDT));
    //configure transmit descriptor
    (txDescriptorsArray + tailIndex)->addr = page2pa(pp);
    (txDescriptorsArray + tailIndex)->length = size;
    (txDescriptorsArray + tailIndex)->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RPS | E1000_TXD_CMD_IDE;
    // *(txBuffers + tailIndex) = page2kva(pp);

    // before spinning in busy wait, we need increase cyclicly the tail
    
    *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TDT)) += 1;
    *(uint32_t *)(e1000Va + BYTE_T0_ADDRESS(E1000_TDT)) %= E1000_TX_DESC_NUM;

    //spin until array has free descriptor ( by DD Bit )
    while (!((txDescriptorsArray + tailIndex)->status & E1000_TXD_STAT_DD));
    
    //free descriptor
    memset((txDescriptorsArray + tailIndex), 0, sizeof(struct tx_desc));
    
    return 0;
}
