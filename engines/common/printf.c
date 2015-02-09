/*
 * printf.c
 */

static inline void putch(char c)
{
	asm volatile("out %0, $0xfe\n" : : "a"(c));
}

void printf(const char *fmt, ...)
{
	while (*fmt) {
		putch(*fmt++);
	}
}
