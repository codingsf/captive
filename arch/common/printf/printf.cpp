/*
 * printf.c
 */
#include <printf.h>
#include <string.h>
#include <stdarg.h>

static inline void putch(char c)
{
	asm volatile("out %0, $0xfe\n" : : "a"(c));
}

static inline void putnum(unsigned int v, int base, int sgn, int pad, char pad_char)
{
	char buffer[12];
	int buffer_idx = 0;

	if (v == 0) {
		if (pad > 0) {
			for (int i = 0; i < pad; i++) {
				putch(pad_char);
			}
		} else {
			putch('0');
		}

		return;
	} else if ((int)v < 0 && sgn) {
		putch('-');
		v = -v;
	}

	for (int i = 0; i < sizeof(buffer); i++) {
		buffer[i] = pad_char;
	}

	while (v > 0 && buffer_idx < sizeof(buffer)) {
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

	//43210000

	if (buffer_idx < pad)
		buffer_idx += pad - buffer_idx;

	for (buffer_idx--; buffer_idx >= 0; buffer_idx--) {
		putch(buffer[buffer_idx]);
	}
}

static inline void putstr(const char *str, int pad, char pad_char)
{
	int len = strlen(str);
	while (*str) {
		putch(*str++);
	}

	for (int i = 0; i < pad - len; i++) {
		putch(pad_char);
	}
}

void printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	while (*fmt) {
		if (*fmt == '%') {
			char pad_char = ' ';
			int pad = 0;

retry_format:
			fmt++;
			if (!*fmt) {
				break;
			}

			switch (*fmt) {
			case '0':
				if (pad == 0) {
					pad_char = '0';
				} else {
					pad *= 10;
				}
				goto retry_format;
			case '1' ... '9':
				pad *= 10;
				pad += *fmt - '0';
				goto retry_format;
			case 'd':
				putnum(va_arg(args, int), 10, 1, pad, pad_char);
				break;
			case 'u':
				putnum(va_arg(args, int), 10, 0, pad, pad_char);
				break;
			case 'x':
				putnum(va_arg(args, int), 16, 0, pad, pad_char);
				break;
			case 's':
				putstr(va_arg(args, const char *), pad, pad_char);
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

void sprintf(char *dest, const char *fmt, ...)
{
	*dest++ = '?';
	*dest = 0;
}
