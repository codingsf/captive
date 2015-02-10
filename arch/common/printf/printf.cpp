/*
 * printf.c
 */
#include <printf.h>
#include <stdarg.h>

static inline void putch(char c)
{
	asm volatile("out %0, $0xfe\n" : : "a"(c));
}

static inline void putnum(int v, int base, int sgn)
{
	char buffer[12];
	int buffer_idx = 0;

	if (v == 0) {
		putch('0');
		return;
	} else if (v < 0 && sgn) {
		putch('-');
		v = -v;
	}

	while (v != 0 && buffer_idx < sizeof(buffer)) {
		int val = v % base;

		switch (val) {
		case 0 ... 9:
			buffer[buffer_idx++] = '0' + val;
			break;
		case 10 ... 35:
			buffer[buffer_idx++] = 'a' + val - 10;
			break;
		}

		v /= base;
	}

	for (buffer_idx--; buffer_idx >= 0; buffer_idx--) {
		putch(buffer[buffer_idx]);
	}
}

static inline void putstr(const char *str)
{
	while (*str) {
		putch(*str++);
	}
}

void printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	while (*fmt) {
		if (*fmt == '%') {
			fmt++;
			if (!*fmt) {
				break;
			}

			switch (*fmt) {
			case 'd':
				putnum(va_arg(args, int), 10, 1);
				break;
			case 'u':
				putnum(va_arg(args, int), 10, 0);
				break;
			case 'x':
				putnum(va_arg(args, int), 16, 0);
				break;
			case 's':
				putstr(va_arg(args, const char *));
				break;
			default:
				putch(*fmt);
				break;
			}

			fmt++;
		} else {
			putch(*fmt++);
		}
	}

	va_end(args);
}
