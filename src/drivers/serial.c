#include <arch/csr.h>
#include <arch/io.h>
#include <arch/plic.h>
#include <arch/spinlock.h>

#include <kernel/serial.h>
#include <kernel/panic.h>

static struct {
	struct spinlock lock;
	char buf[1024];
	size_t len;
} serialdev;

static inline void serial_wait_tx_ready(void)
{
	while ((ioread8((char *)SERIAL_BASE + SERIAL_LSR) & SERIAL_LSR_THRE) == 0) {
	}
}

static inline void serial_write_raw(char c)
{
	serial_wait_tx_ready();
	iowrite8((u8)c, (char *)SERIAL_BASE + SERIAL_THR);
}

void serial_init()
{
	spin_init(&serialdev.lock);
	serialdev.len = 0;

	plic_irq_set_priority((u32)IRQ_SERIAL, 1);
	plic_hart_set_threshold(0, 0);
	plic_hart_enable_irq(0, (u32)IRQ_SERIAL);

	iowrite8(0, (char *)SERIAL_BASE + SERIAL_IER);
	iowrite8((u8)(SERIAL_FCR_FIFO_ENABLE | SERIAL_FCR_RX_FIFO_CLEAR | SERIAL_FCR_TX_FIFO_CLEAR),
		(char *)SERIAL_BASE + SERIAL_FCR);
}

void serial_irq_enable()
{
	iowrite8((u8)SERIAL_IER_ERBFI, (char *)SERIAL_BASE + SERIAL_IER);
	csr_set(CSR_SIE, CSR_SIE_SEIE);
	hart_irq_enable();
}

void serial_irq_disable()
{
	iowrite8(0, (char *)SERIAL_BASE + SERIAL_IER);
	csr_clear(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq()
{
	u64 flags = spin_lock_irqsave(&serialdev.lock);
	while ((ioread8((char *)SERIAL_BASE + SERIAL_LSR) & SERIAL_LSR_DTR) != 0) {
		char c = (char)ioread8((char *)SERIAL_BASE + SERIAL_RBR);
		if (serialdev.len < sizeof(serialdev.buf)) {
			serialdev.buf[serialdev.len++] = c;
		}
		if (c == '\r') {
			serial_write_raw('\r');
			serial_write_raw('\n');
		} else {
			serial_write_raw(c);
		}
	}
	spin_unlock_irqrestore(&serialdev.lock, flags);
}

size_t serial_read(char *buf)
{
	size_t len;
	size_t i;
	u64 flags = spin_lock_irqsave(&serialdev.lock);

	len = serialdev.len;
	for (i = 0; i < len; i++) {
		buf[i] = serialdev.buf[i];
	}
	serialdev.len = 0;

	spin_unlock_irqrestore(&serialdev.lock, flags);
	return len;
}

void serial_puts(char *str)
{
	while (*str != '\0') {
		serial_putc(*str++);
	}
}

void serial_putc(char c)
{
	if (c == '\n') {
		serial_write_raw('\r');
	}
	serial_write_raw(c);
}
