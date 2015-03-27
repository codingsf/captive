#include <jit/translation-context.h>
#include <printf.h>

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
	/*uint64_t addr;

	asm volatile("out %1, $0xff" : "=a"(addr) : "a"((uint8_t)6), "D"(instruction_buffer_offset));

	if (addr == 0) {
		return NULL;
	}

	addr += code_buffer_offset;
	return (GuestBasicBlock::GuestBasicBlockFn)(addr);*/

	return NULL;
}
