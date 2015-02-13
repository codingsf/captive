/*
 * start.c
 */
#include <printf.h>
#include <mm.h>
#include <env.h>

extern captive::arch::Environment *create_environment();

extern "C" {
	void __attribute__((noreturn)) start(uint64_t first_phys_page, unsigned int ep)
	{
		captive::arch::Memory mm(first_phys_page);
		captive::arch::Environment *env = create_environment();

		if (!env) {
			printf("error: unable to create environment\n");
		} else {
			if (!env->init()) {
				printf("error: unable to initialise environment\n");
			} else if (!env->run(ep)) {
				printf("error: unable to launch environment\n");
			}

			delete env;
		}

		asm volatile("out %0, $0xff\n" : : "a"(0x02));
		for(;;);
	}
}
