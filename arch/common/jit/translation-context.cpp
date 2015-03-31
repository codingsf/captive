#include <jit/translation-context.h>
#include <shared-memory.h>
#include <printf.h>

using namespace captive::arch::jit;

TranslationContext::TranslationContext(SharedMemory& allocator)
	: _allocator(allocator), _current_block_id(0), _buffer_size(0x100)
{
	instruction_buffer = (struct bytecode_descriptor *)allocator.allocate(_buffer_size);

	instruction_buffer->block_count = 0;
	instruction_buffer->entry_count = 0;
	instruction_buffer->vreg_count = 0;

	current_block(alloc_block());
}

GuestBasicBlock::GuestBasicBlockFn TranslationContext::compile()
{
	return NULL;
}
