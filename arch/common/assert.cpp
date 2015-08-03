#pragma GCC diagnostic ignored "-Wtrigraphs"

#include <define.h>
#include <assert.h>
#include <printf.h>

static const char *resolve_symbol(uint64_t sym)
{
	return "<???>";
}

static inline void print_symbol(uint64_t sym)
{
	asm volatile("out %0, $0xff\n" :: "a"(13), "D"(sym));
}

void dump_code(unsigned long int rip)
{
	printf("Code:\n");
	
	uint8_t *p = (uint8_t *)rip;
	for (int i = 0; i < 0x100; i++) {
		printf("%02x ", p[i]);
		if (i % 10 == 0) printf("\n");
	}
	printf("\n");
}

void dump_mcontext(const struct mcontext *mctx)
{
	printf("machine context:\n");
	printf("  rax = %016lx\n", mctx->rax);
	printf("  rbx = %016lx\n", mctx->rbx);
	printf("  rcx = %016lx\n", mctx->rcx);
	printf("  rdx = %016lx\n", mctx->rdx);
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
		printf("  %d: %lx: ", frame_idx++, stack[1]);
		print_symbol(stack[1]);
		bp = stack[0];
	}
}

void __assertion_failure(const char *filename, int lineno, const char *expression)
{
	printf("ABORT: ASSERTION FAILED: <%s> in %s:%d\n", expression, filename, lineno);

	dump_stack();
	asm volatile("out %0, $0xff\n" :: "a"(3));
}
