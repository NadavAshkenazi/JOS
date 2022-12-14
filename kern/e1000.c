#include <kern/e1000.h>
#include <inc/error.h>

 /* ==========================================================
						E1000 Driver code
   ========================================================== */



 /* =================== Declerations======================*/

volatile uint32_t *e1000RegistersVA; //registers addr
struct tx_desc txDescriptorsArray[E1000_TX_DESC_NUM]  __attribute__((aligned(16)));
struct rx_desc rxDescriptorsArray[E1000_RX_DESC_NUM] __attribute__((aligned(16)));

static inline void e100_txDescs_init();
static inline void e100_rxDescs_init();


/* =================== Driver API ======================*/

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

    //setup registers:

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
        //??? Set the Enable (TCTL.EN) bit to 1b for normal operation. 
        //??? Set the Pad Short Packets (TCTL.PSP) bit to 1b. */

    // config TCTL
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TCTL)) |= 0x0; 
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TCTL)) = (E1000_TCTL_EN | E1000_TCTL_PSP);
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TCTL)) |= E1000_TCTL_CT; //no meaning in full-duplex
    
    //The following line has no meaning in full-duplex mode:
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TCTL)) |=  (0x40 << E1000_TCTL_COLD_SHIFT); 

    // config TIPG
    *(uint16_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TIPG)) = (uint16_t)(E1000_TIPG_IPGT << E1000_TIPG_IPGT_SHIFT |
                                                                               E1000_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT |
                                                                               E1000_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT); 
}


inline int e1000_transmit(struct PageInfo* pp, size_t size){

    if (size > SIZE_OF_PACKET)
        panic("e1000_transmit: size requested larger than packet");

    int tailIndex = *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_TDT));

    //configure transmit descriptor
    (txDescriptorsArray + tailIndex)->addr = page2pa(pp);
    (txDescriptorsArray + tailIndex)->length = size;
    (txDescriptorsArray + tailIndex)->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;

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



uint16_t
readMACFromEEPROM(uint32_t addr)
{
    /*  EERD (00014h; RW) Table 13-7. EEPROM Read Register Bit Description
        +-------+---------+------+------+------+-------+
        | 31:16 |   15:8  |  7:5 |   4  |  3:1 |   0   |
        +-------+---------+------+------+------+-------+
        |  data | address | RSV. | DONE | RSV. | START |
        +-------+---------+------+------+------+-------+ 

        The Ethernet Individual Address (IA) is a six-byte field that must be unique for each Ethernet port
        (and unique for each copy of the EEPROM image). The first three bytes are vendor specific.
         The value from this field is loaded into the Receive Address Register 0 (RAL0/RAH0)
        */

        // Request the word by address
        *(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_EERD)) = addr | E1000_EERD_START;

        //poll until done bit is up
        while (!(*(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_EERD)) & E1000_EERD_DONE))
            ;

        // return data part
        return *(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_EERD)) >> E1000_EERD_DATA;
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
    }

    // Setup registers:

    /*MAC addresses are written from lowest-order byte to highest-order byte
     */

    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RA)) = readMACFromEEPROM(E1000_EERD_MAC_MID) << 16 |
                                                                  readMACFromEEPROM(E1000_EERD_MAC_LOW); //mac addr low
    *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RA + 4)) = readMACFromEEPROM(E1000_EERD_MAC_HIGH) |
                                                                      E1000_RAH_AV; //mac addr high


    // ************************************ manual setup if necessery ******************************************
    //
    // *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RA)) = 0x12005452; //mac addr low
    // *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RA + 4)) = 0x5634 | E1000_RAH_AV; //mac addr high
    //
    // *********************************************************************************************************

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

    //enable timer interupts :
    /*Receiver Timer Interrupt
      Set when the receiver timer expires.
      The receiver timer is used for receiver descriptor packing. Timer
      expiration flushes any accumulated descriptors and sets an
      interrupt event when enabled.
      */
    *(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_IMS)) = E1000_IMS_RXT0;

    /*Setting the Packet Timer to 0b disables both the Packet Timer and the Absolute Timer and causes 
      the Receive Timer Interrupt to be generated whenever a new packet has been stored in memory. */
    *(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RDTR)) = 0;
}


inline int e1000_receive(struct PageInfo** pp_pointer){

    int nextDescIndex = (*(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_RDT)) + 1) % E1000_RX_DESC_NUM;

    /*descriptor does not hold a packet.
      allow interupts and return with an error*/
    if (!((rxDescriptorsArray + nextDescIndex)->status & E1000_RXD_STAT_DD))
    {   
        //Sets Receiver Timer Interrupt 
        //All register bits are cleared upon read, e.g needs to be set each time.
        *(uint32_t *)(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_IMS)) = E1000_IMS_RXT0;
        return -E_NET_ERROR;
    }

    /*descriptor holds a packet. insert packet into requested page*/
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

    return len;
 }


void
e1000_trap_handler(){

    // Find the input env blocked by network and wake it up
    *(e1000RegistersVA + BYTE_T0_ADDRESS(E1000_ICR)) |= E1000_ICR_RXT0; // clear interrupt
    int i = 0;
    int envsFound = 0;
    for (; i < NENV;i++)
    {
        if ((envs[i].env_status == ENV_NOT_RUNNABLE) && (envs[i].env_net_blocked == true)){
            //wake up
            envs[i].env_status = ENV_RUNNABLE;
            envs[i].env_net_blocked = false;

            // clear syscalls service request
            if (envs[i].env_tf.tf_regs.reg_eax != 0)
                envs[i].env_tf.tf_regs.reg_eax = -E_NET_ERROR;
            envsFound++;
        }
    }
}
