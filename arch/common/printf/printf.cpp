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

static inline void sputnum(char **ob, unsigned long long int v, int base, int sgn, int pad, char pad_char)
{
	char buffer[16];
	int buffer_idx = 0;

	if (v == 0) {
		if (pad > 0) {
			for (int i = 0; i < pad; i++) {
				**ob = pad_char;
				*ob = *ob + 1;
			}
		} else {
			**ob = '0';
			*ob = *ob + 1;
		}

		return;
	} else if ((signed long long int)v < 0 && sgn) {
		**ob = '-';
		*ob = *ob + 1;
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
		**ob = buffer[buffer_idx];
		*ob = *ob + 1;
	}
}

static inline void putnum(unsigned long long int v, int base, int sgn, int pad, char pad_char)
{
	char buffer[16];
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
	} else if ((signed long long int)v < 0 && sgn) {
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

static inline void sputstr(char **buffer, const char *str, int pad, char pad_char)
{
	int len = strlen(str);
	while (*str) {
		**buffer = *str++;
		*buffer = *buffer + 1;
	}

	for (int i = 0; i < pad - len; i++) {
		**buffer = pad_char;
		*buffer = *buffer + 1;
	}
}

int printf(const char *fmt, ...)
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
				putnum(va_arg(args, long long int), 16, 0, pad, pad_char);
				break;
			case 'p':
				putnum((unsigned long long int)va_arg(args, void *), 16, 0, pad, pad_char);
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

	return 0;
}

void sprintf(char *dest, const char *fmt, ...)
{
	char *dest_buffer = dest;
	*dest_buffer = 0;

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
				sputnum(&dest_buffer, va_arg(args, int), 10, 1, pad, pad_char);
				break;
			case 'u':
				sputnum(&dest_buffer, va_arg(args, int), 10, 0, pad, pad_char);
				break;
			case 'x':
				sputnum(&dest_buffer, va_arg(args, int), 16, 0, pad, pad_char);
				break;
			case 's':
				sputstr(&dest_buffer, va_arg(args, const char *), pad, pad_char);
				break;
			default:
				*dest_buffer++ = *fmt;
				break;
			}

			fmt++;
		} else {
			*dest_buffer++ = *fmt++;
		}
	}

	*dest_buffer = '0';
	va_end(args);
}
