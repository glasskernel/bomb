#include <platform/debug.h>
#include "uart_simple.h"

extern unsigned int globalUartBase;

void platform_dputc(char c)
{
	if (globalUartBase != 0) {
		uart_simple_char_out(c);
	}
}

int platform_dgetc(char *c, bool wait)
{
	if (globalUartBase != 0) {
		uart_simple_char_in(c);
	}
	return 0;
}
