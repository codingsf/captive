/*
 * printf.c
 */
#include <printf.h>

static inline void putch(char c)
{
	asm volatile("out %0, $0xfe\n" : : "a"(c));
}

int printf(const char *fmt, ...)
{
	char buffer[0x1000];

	int rc;
	va_list args;

	va_start(args, fmt);
	rc = vsnprintf(buffer, 0x1000, fmt, args);
	va_end(args);

	for (int i = 0; i < 0x1000; i++) {
		if (buffer[i] == 0) {
			break;
		}

		putch(buffer[i]);
	}

	return rc;
}
