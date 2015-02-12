/*
 * start.c
 */
#include <printf.h>
#include <env.h>

extern captive::arch::Environment *create_environment();

extern "C" {
	void __attribute__((noreturn)) start(unsigned int ep)
	{
		captive::arch::Environment *env = create_environment();
		if (!env) {
			printf("error: unable to create environment\n");
		} else {
			if (!env->run(ep)) {
				printf("error: unable to launch environment\n");
			}
			
			delete env;
		}

		asm volatile("out %0, $0xff\n" : : "a"(0x02));
		for(;;);
	}
}
