#include <jit/translation-context.h>
#include <printf.h>
#include <shmem.h>

extern volatile captive::shmem_data *shmem;

using namespace captive::arch::jit;

TranslationContext::TranslationContext(void *_instruction_buffer, uint64_t _instruction_buffer_size)
	: current_block_id(0),
	instruction_buffer_size(_instruction_buffer_size),
	instruction_buffer((bytecode_descriptor *)_instruction_buffer)
{
	instruction_buffer->block_count = 0;
	instruction_buffer->entry_count = 0;
	instruction_buffer->vreg_count = 0;

	current_block(alloc_block());
}

GuestBasicBlock::GuestBasicBlockFn TranslationContext::compile()
{
	assert((uint64_t)instruction_buffer >= (uint64_t)shmem);

	uint64_t addr, shmem_buffer_offset;

	shmem_buffer_offset = (uint64_t)instruction_buffer - (uint64_t)shmem;
	asm volatile("out %1, $0xff" : "=a"(addr) : "a"((uint8_t)6), "D"(shmem_buffer_offset));

	if (addr == 0)
		return NULL;

	addr += 0x220000000;
	return (GuestBasicBlock::GuestBasicBlockFn)(addr);
}
