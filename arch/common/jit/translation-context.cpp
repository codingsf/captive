#include <jit/translation-context.h>

using namespace captive::arch::jit;

TranslationContext::TranslationContext(Allocator& allocator, shared::TranslationBlock& block)
	: _allocator(allocator), _block(block), _current_block_id(0), _ir_insn_buffer_size(0x1000)
{
	block.ir_insn = (shared::IRInstruction *)allocator.allocate(_ir_insn_buffer_size);

	block.ir_block_count = 0;
	block.ir_insn_count = 0;
	block.ir_reg_count = 0;

	current_block(alloc_block());
}
