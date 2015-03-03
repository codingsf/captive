#include <jit/translation-context.h>
#include <printf.h>
#include <shmem.h>

extern volatile captive::shmem_data *shmem;

using namespace captive::arch::jit;

TranslationContext::TranslationContext(void *instruction_buffer)
	: current_block_id(0),
	instruction_buffer((bytecode_descriptor *)instruction_buffer)
{

}

GuestBasicBlock::GuestBasicBlockFn TranslationContext::compile()
{
	assert((uint64_t)instruction_buffer >= (uint64_t)shmem);

	uint64_t addr, shmem_buffer_offset;

	shmem_buffer_offset = (uint64_t)instruction_buffer - (uint64_t)shmem;
	asm volatile("out %1, $0xff" : "=a"(addr) : "a"((uint8_t)6), "D"(shmem_buffer_offset));

	addr += 0x220000000;
	return (GuestBasicBlock::GuestBasicBlockFn)(addr);
}
