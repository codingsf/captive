#pragma GCC diagnostic ignored "-Wtrigraphs"

#include <define.h>
#include <assert.h>
#include <printf.h>

static const char *resolve_symbol(uint64_t sym)
{
	return "<???>";
}

void dump_stack()
{
	int count = 0;

	printf("Stack Frame:\n");

	uint64_t bp;
	asm volatile("mov %%rbp, %0\n" : "=r"(bp));

	int frame_idx = 0;
	while (bp != 0 && count++ < 100) {
		uint64_t *stack = (uint64_t *)bp;
		printf("  %d: %lx: %s\n", frame_idx++, stack[1], resolve_symbol(stack[1]));
		bp = stack[0];
	}
}

void __assertion_failure(const char *filename, int lineno, const char *expression)
{
	printf("ABORT: ASSERTION FAILED: <%s> in %s:%d\n", expression, filename, lineno);

	dump_stack();
	asm volatile("out %0, $0xff\n" :: "a"(3));
}
