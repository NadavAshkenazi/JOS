#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/picirq.h>
#include <inc/trap.h>

// LAB 6: Your driver code here

 

volatile uint32_t *e1000Va;

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

