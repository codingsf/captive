/*
 * start.c
 */
#include <printf.h>
#include <env.h>

extern captive::arch::Environment *create_environment();

extern "C" {
	void __attribute__((noreturn)) start(unsigned int ep)
	{
		printf("start\n");

		captive::arch::Environment *env = create_environment();
		env->run(ep);

		asm volatile("out %0, $0xff\n" : : "a"(0x02));
	}
}
