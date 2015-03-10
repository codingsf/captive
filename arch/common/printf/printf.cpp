/*
 * printf.c
 */
#include <printf.h>

static inline void putch(char c)
{
	asm volatile("out %0, $0xfe\n" : : "a"(c));
}

static char *fast_buffer;
static int fast_buffer_size;

void printf_init(char *_fast_buffer, int _fast_buffer_size)
{
	fast_buffer = _fast_buffer;
	fast_buffer_size = _fast_buffer_size;
}

int printf(const char *fmt, ...)
{
	if (!fast_buffer) {
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
	} else {
		int rc;
		va_list args;

		va_start(args, fmt);
		*fast_buffer = '\0';
		rc = vsnprintf(fast_buffer, fast_buffer_size, fmt, args);
		va_end(args);

		asm volatile("out %0, $0xfe\n" : : "a"(0));
		return rc;
	}
}
