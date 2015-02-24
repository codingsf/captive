#include <string.h>

void *memset(void *dest, int c, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		((uint8_t *)dest)[i] = c;
	}

	return dest;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		((uint8_t *)dest)[i] = ((uint8_t *)src)[i];
	}

	return dest;
}

void *bzero(void *dest, size_t n)
{
	return memset(dest, 0, n);
}
