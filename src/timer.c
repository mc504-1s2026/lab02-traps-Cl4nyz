#include <arch/timer.h>
#include <arch/csr.h>
#include <arch/io.h>

#include <kernel/printf.h>
#include <kernel/mm.h>
#include <kernel/panic.h>
#include <kernel/trap.h>


extern void serial_puts(char *str);

static bool alarm_armed;
static u64 alarm_deadline;

#define CLINT_MTIMECMP(hart) ((u64 *)pa_to_va(0x2004000UL + 8UL * (hart)))
#define CLINT_MTIME ((u64 *)pa_to_va(0x200bff8UL))

static inline void timer_set_mtimecmp(u64 value)
{
	iowrite64(value, CLINT_MTIMECMP(0));
}

u64 timer_read()
{
	return ioread64(CLINT_MTIME);
}

void timer_irq_enable()
{
	timer_set_mtimecmp(~0ULL);
	csr_set(CSR_SIE, CSR_SIE_STIE);
	hart_irq_enable();
}

void timer_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_STIE);
}

void timer_set_alarm(u64 secs)
{
	u64 now = timer_read();
	alarm_deadline = now + secs * TIMER_FREQ;

	alarm_armed = true;
	timer_set_mtimecmp(alarm_deadline);
}

void timer_poll()
{
	if (alarm_armed && timer_read() >= alarm_deadline) {
		alarm_armed = false;
		serial_puts("alarm\n");
	}
}

void timer_irq()
{
	timer_poll();

	timer_set_mtimecmp(~0ULL);
}
