#include <arch/csr.h>
#include <arch/plic.h>

#include <kernel/panic.h>
#include <kernel/printf.h>
#include <kernel/serial.h>
#include <kernel/trap.h>

/* defined in src/trap_entry.S */
extern void trap_entry();

static inline u64 trap_irq_enabled(void)
{
	return csr_read(CSR_SSTATUS) & CSR_SSTATUS_SIE;
}

void handle_irq()
{
	u64 cause = csr_read(CSR_SCAUSE);
	u32 irq;

	switch (cause) {
	case TRAP_TIMER_IRQ:
		timer_irq();
		break;
	case TRAP_EXTERNAL_IRQ:
		irq = plic_hart_claim_irq(0);
		if (irq == IRQ_SERIAL) {
			serial_irq();
		}
		if (irq != 0) {
			plic_hart_complete_irq(0, irq);
		}
		break;
	default:
		warn("unexpected irq cause=%#lx\n", cause);
		break;
	}
}

void handle_exception()
{
	u64 cause = csr_read(CSR_SCAUSE);
	u64 stval = csr_read(CSR_STVAL);
	u64 sepc = csr_read(CSR_SEPC);

	error("trap: exception cause=%#lx stval=%#lx sepc=%#lx\n", cause, stval, sepc);
	BUG();
}

void trap_setup()
{
	csr_write(CSR_STVEC, (u64)trap_entry);
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
	csr_clear(CSR_SIE, CSR_SIE_STIE | CSR_SIE_SEIE | CSR_SIE_SSIE);
}

void handle_trap()
{
	u64 cause = csr_read(CSR_SCAUSE);

	if (cause & TRAP_IRQ_BIT) {
		handle_irq();
	} else {
		handle_exception();
	}
}

void hart_irq_enable()
{
	csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

u64 hart_irq_save()
{
	u64 flags = trap_irq_enabled();
	hart_irq_disable();
	return flags;
}

void hart_irq_restore(u64 flags)
{
	if (flags & CSR_SSTATUS_SIE) {
		hart_irq_enable();
	} else {
		hart_irq_disable();
	}
}

void hart_irq_disable()
{
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}
