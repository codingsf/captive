#include <jit/translation-context.h>
#include <printf.h>

using namespace captive::arch::jit;

TranslationContext::TranslationContext(void *buffer_a, void *buffer_b)
	: _buffer_a(buffer_a), _buffer_b(buffer_b), _current_block_id(0), _ir_block_count(0), _ir_reg_count(0), _ir_insns(NULL), _ir_insn_count(0), _ir_insn_buffer_size(0)
{
	set_buffer_a();
	current_block(alloc_block());
}

TranslationContext::~TranslationContext()
{
	//captive::arch::free(_ir_insns);
}
