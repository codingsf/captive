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

int memcmp(const void *s1, const void *s2, size_t size)
{
	uint8_t *p1 = (uint8_t *)s1;
	uint8_t *p2 = (uint8_t *)s2;

	while (size-- > 0) {
		if (*p1++ != *p2++) return 1;
	}

	return 0;
}
