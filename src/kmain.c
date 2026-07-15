#include <kernel/printf.h>
#include <kernel/mm.h>
#include <kernel/string.h>
#include <arch/timer.h>
#include <kernel/trap.h>
#include <kernel/serial.h>

extern int _hartid[];

static void shell_execute(char *line)
{
	char *args;
	u64 secs;
	char buf[32];

	if (strcmp(line, "uptime") == 0) {
		snprintf(buf, sizeof(buf), "%lus\n", timer_read() / TIMER_FREQ);
		serial_puts(buf);
		return;
	}

	args = strstr(line, " ");
	if (args != NULL) {
		*args = '\0';
		args++;
	}

	if (strcmp(line, "echo") == 0) {
		serial_puts(args != NULL ? args : "");
		serial_puts("\n");
		return;
	}

	if (strcmp(line, "alarm") == 0) {
		secs = args != NULL ? strtou64(args, 10) : 0;
		timer_set_alarm(secs);
		return;
	}
}

void kmain()
{
	printk_set_level(LOG_DEBUG);
	info("entered S-mode\n");
	info("booting on hart %d\n", _hartid[0]);
	info("setting up virtual memory...\n");
	vm_init();

	info("enabling traps...\n");
	trap_setup();
	info("enabling timer...\n");
	timer_irq_enable();
	info("enabling serial...\n");
	serial_init();
	serial_irq_enable();

	serial_puts("> ");

	char line[128];
	size_t line_len = 0;
	char rx[64];

	for (;;) {
		timer_poll();
		size_t n = serial_read(rx);
		for (size_t i = 0; i < n; i++) {
			char c = rx[i];
			if (c == '\r') {
				line[line_len] = '\0';
				serial_puts("\n");
				shell_execute(line);
				line_len = 0;
				serial_puts("> ");
			} else if (line_len + 1 < sizeof(line)) {
				line[line_len++] = c;
			}
		}
	}
}
