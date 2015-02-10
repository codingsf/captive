/*
 * start.c
 */
#include <printf.h>

extern "C" void start(unsigned int ep)
{
	printf("Hello, world.\n");
	printf("Entry Point=0x%x\n", ep);

	for(;;);
}
