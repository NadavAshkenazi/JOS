#include <kern/e1000.h>
#include <inc/error.h>

// LAB 6: Your driver code here

 

volatile uint32_t *e1000RegistersVA;//registers addr
struct tx_desc txDescriptorsArray[E1000_TX_DESC_NUM] 
    __attribute__((aligned(16)));

// packetBuff txBuffers[E1000_TX_DESC_NUM]; 

struct rx_desc rxDescriptorsArray[E1000_RX_DESC_NUM]
     __attribute__((aligned(16)));

// packetBuff rxBuffers[E1000_TX_DESC_NUM];




static inline void e100_txDescs_init();
static inline void e100_rxDescs_init();


int e1000_attach(struct pci_func *pcif)
{
    pci_func_enable(pcif);

    // // Init MMIO Region
    e1000RegistersVA = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

    /*debug of mmio mapping */
    
    assert(0x80080783 == *(e1000RegistersVA + (BYTE_T0_ADDRESS(E1000_STATUS))));
    cprintf("E1000 status: 0x%08x should be 0x80080783\n",*(e1000RegistersVA + (BYTE_T0_ADDRESS(E1000_STATUS))));
    
    // Set the interrupts channel
    irq_setmask_8259A(irq_mask_8259A & ~(1 << IRQ_E1000));

    e100_txDescs_init();
    e100_rxDescs_init();

    return 0;
}


static inline void e100_txDescs_init(){
    /*  Allocate a region of memory for the transmit descriptor list. Software should insure this memory is
        aligned on a paragraph (16-byte) boundary */
    memset(txDescriptorsArray, 0, sizeof(txDescriptorsArray));
    // memset(txBuffers, 0, sizeof(txBuffers));

    //init txDescriptorsArray
    // int j = 0;
    // for (; j< E1000_TX_DESC_NUM; j++){
    //     txDescriptorsArray[j].cmd |= E1000_TXD_CMD_EOP | E1000_TXD_CMD_RPS | E1000_TXD_CMD_IDE;
    //     txDescriptorsArray[j].status |= E1000_TXD_STAT_DD;
    // }
    //setup registers
    /*  Program the Transmit Descriptor Base Address
        (TDBAL/TDBAH) register(s) with the address of the region */
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TDBAL)) = PADDR(txDescriptorsArray);
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TDBAH)) = 0x0;
    /*  Set the Transmit Descriptor Length (TDLEN) register to the size (in bytes) of the descriptor ring.
        This register must be 128-byte aligned - aleardy to 16-b aligned */
    *(uint16_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TDLEN)) = sizeof(txDescriptorsArray);
    /*  The Transmit Descriptor Head and Tail (TDH/TDT) registers are initialized (by hardware) to 0b
        after a power-on or a software initiated Ethernet controller reset */
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TDH)) = 0; // head index 0 at init
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TDT)) = 0; // tail index 0 at init
    /* Initialize the Transmit Control Register (TCTL) for desired operation to include the following:*/
        //• Set the Enable (TCTL.EN) bit to 1b for normal operation. 
        //• Set the Pad Short Packets (TCTL.PSP) bit to 1b. */

    // config TCTL
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TCTL)) |= 0x0; 
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TCTL)) = (E1000_TCTL_EN | E1000_TCTL_PSP);
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TCTL)) |= E1000_TCTL_CT; //no meaning in full-duplex
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TCTL)) |=  (0x40 << E1000_TCTL_COLD_SHIFT); //no meaning in full-duplex XXX

    // config TIPG
    *(uint16_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TIPG)) = (uint16_t)(E1000_TIPG_IPGT << E1000_TIPG_IPGT_SHIFT | E1000_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT | E1000_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT); //XXX

}

inline int e1000_transmit(struct PageInfo* pp, size_t size){
    if (size > SIZE_OF_PACKET)
        panic("e1000_transmit: size requested larger than packet");

    // if (!size) return -E_NET_ERROR;
    int tailIndex = *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TDT));
    //configure transmit descriptor
    (txDescriptorsArray + tailIndex)->addr = page2pa(pp);
    (txDescriptorsArray + tailIndex)->length = size;
    (txDescriptorsArray + tailIndex)->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
    // *(txBuffers + tailIndex) = page2kva(pp);

    // before spinning in busy wait, we need increase cyclicly the tail
    
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TDT)) += 1;
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TDT)) %= E1000_TX_DESC_NUM;

    //spin until array has free descriptor ( by DD Bit )
    if (!((txDescriptorsArray + tailIndex)->status & E1000_TXD_STAT_DD)){
        return -E_NET_ERROR;
    }
    

    //free descriptor
    memset((txDescriptorsArray + tailIndex), 0, sizeof(struct tx_desc));
    
    return 0;
}



static inline void e100_rxDescs_init()
{
    /*  Allocate a region of memory for the receive descriptor list. Software should insure this memory is
        aligned on a paragraph (16-byte) boundary.
        Queue should be full at idle state*/
    memset(rxDescriptorsArray, 0, sizeof(rxDescriptorsArray));
    int i = 0;
    struct PageInfo* pp;
    for (; i < E1000_RX_DESC_NUM; i++)
    {
        pp = page_alloc(ALLOC_ZERO);
        pp->pp_ref++;
        (rxDescriptorsArray + i)->addr = page2pa(pp);
        // txBuffers[i] = page2kva(pp);
    }

    // Setup registers

    /*MAC addresses are written from lowest-order byte to highest-order byte
     */
    
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RA)) = 0x12005452; //mac addr low
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RA + 4)) = 0x5634 | E1000_RAH_AV; //mac addr high

    /*  Program the receive Descriptor Base Address
        (RDBAL/RDBAH) register(s) with the address of the region */
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RDBAL)) = PADDR(rxDescriptorsArray);
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RDBAH)) = 0x0;

    /*  Set the  receive Length (RDLEN) register to the size (in bytes) of the descriptor ring.
        This register must be 128-byte aligned - aleardy to 16-b aligned */
    *(uint16_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RDLEN)) = sizeof(rxDescriptorsArray);

    /*  The receive Descriptor Head  (RDH) register is initialized (by hardware) to 0b.
        Queue should be full at idle state so tail is at end of queue */
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RDH)) = 0; // head index 0 at init
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RDT)) = E1000_RX_DESC_NUM-1; // tail index 0 at init
    

    // config RCTL
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RCTL)) = E1000_RCTL_EN | E1000_RCTL_SECRC | E1000_RCTL_BAM;

    //enable timer interupts 
    /*Receiver Timer Interrupt
      Set when the receiver timer expires.
      The receiver timer is used for receiver descriptor packing. Timer
      expiration flushes any accumulated descriptors and sets an
      interrupt event when enabled.
      */
    *(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_IMS)) = E1000_IMS_RXT0;
    /*Setting the Packet Timer to 0b disables both the Packet Timer and the Absolute Timer  and causes the Receive Timer Interrupt to be generated whenever a new packet has been
      stored in memory. */
    
    *(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RDTR)) = 0;
}


inline int e1000_receive(struct PageInfo** pp_pointer){
    // cprintf("in e1000_receive\n"); //XXX

    int nextDescIndex = (*(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RDT)) + 1) % E1000_RX_DESC_NUM;
    /*descriptor does not hold a packet.
      allow interupts and return with an error*/
    if (!((rxDescriptorsArray + nextDescIndex)->status & E1000_RXD_STAT_DD))
    {   
        //Sets Receiver Timer Interrupt 
        //All register bits are cleared upon read, e.g needs to be set each time.
        *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_IMS)) = E1000_IMS_RXT0;
        // cprintf("e1000_receive: no pakcet\n"); //XXX
        return -E_NET_ERROR;
    }

    /*descriptor holds a packet.
      insert packet into requested page*/
    (rxDescriptorsArray + nextDescIndex)->status &= ~E1000_RXD_STAT_DD; //clear DD flag in status
    size_t len =(size_t)(rxDescriptorsArray + nextDescIndex)->length;
    if (!len) return -E_NET_ERROR;
    if (len > SIZE_OF_PACKET)
        return -E_NET_ERROR;
    *pp_pointer = pa2page((rxDescriptorsArray + nextDescIndex)->addr);


    /*clear descriptor and insest it back to free descriptor queue*/
    memset((rxDescriptorsArray + nextDescIndex), 0, sizeof(struct rx_desc)); //clear desc
    struct PageInfo *temp_pp;
    temp_pp = page_alloc(ALLOC_ZERO); // allocate free page for next packet
    temp_pp->pp_ref++;
    (rxDescriptorsArray + nextDescIndex)->addr = page2pa(temp_pp);
    *(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RDT)) = nextDescIndex;

    // cprintf("e1000_receive: len: %d\n", len); //XXX
    return len;
 }



void
e1000_clear_interrupt(void)
{
    *(uint32_t *)(e1000RegistersVA + E1000_ICR) |= E1000_ICR_RXT0; 
	lapic_eoi();
	irq_eoi();
}

void
e1000_trap_handler(){
    // Find the input env blocked by network and wake it up
    cprintf("env %x: in e1000_trap_handler\n", curenv->env_id); //XXX
    *(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_ICR)) |= E1000_ICR_RXT0; // clear interrupt
    int i = 0;
    int envsFound = 0;
    for (; i < NENV;i++)
    {
        // cprintf("e1000_trap_handler: env: %x, status: %d, env_net_blocked: %d\n", envs[i].env_id, envs[i].env_status, envs[i].env_net_blocked);
        if ((envs[i].env_status == ENV_NOT_RUNNABLE) && (envs[i].env_net_blocked == true)){
            //wake up
            // // cprintf("e1000_trap_handler: found env to wake up: %x\n", envs[i].env_id);
            envs[i].env_status = ENV_RUNNABLE;
            // // cprintf("e1000_trap_handler: envs[%d].env_status: %d\n", i, envs[i].env_status);
            envs[i].env_net_blocked = false;
            // // cprintf("e1000_trap_handler: envs[%d].env_net_blocked: %d\n", i, envs[i].env_net_blocked);
            // envs[i].env_tf.tf_regs.reg_eax = -1; //XXX
            cprintf("e1000_trap_handler: envs[%d].env_tf.tf_regs.reg_eax: %d\n", i, envs[i].env_tf.tf_regs.reg_eax);
            if (envs[i].env_tf.tf_regs.reg_eax != 0)
                envs[i].env_tf.tf_regs.reg_eax = -E_NET_ERROR;
            envsFound++;
        }
       
    }
}
