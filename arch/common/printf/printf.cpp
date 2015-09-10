/*
 * printf.c
 */
#include <define.h>
#include <printf.h>
#include <shmem.h>

static inline void putch(char c)
{
	asm volatile("out %0, $0xfe\n" : : "a"(c));
}

static char *fast_buffer;
static int fast_buffer_size;

void printf_init(captive::MemoryVector& printf_buffer)
{
	fast_buffer = (char *)printf_buffer.base_address;
	fast_buffer_size = printf_buffer.size;
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

int fatal(const char *fmt, ...)
{
	char buffer[0x1000];
	va_list args;

	va_start(args, fmt);
	int rc = vsnprintf(buffer, 0x1000, fmt, args);
	va_end(args);
	
	buffer[rc] = 0;
	
	printf("fatal: %s", buffer);
	abort();
}
