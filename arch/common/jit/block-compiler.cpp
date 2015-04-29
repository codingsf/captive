#include <jit/block-compiler.h>
#include <jit/translation-context.h>
#include <jit/ir.h>
#include <x86/encode.h>
#include <shared-jit.h>
#include <local-memory.h>
#include <printf.h>

using namespace captive::arch::jit;
using namespace captive::arch::x86;

BlockCompiler::BlockCompiler()
{

}

bool BlockCompiler::compile(shared::TranslationBlock& tb, block_txln_fn& fn)
{
	IRContext& ctx;

	if (!build(tb, ctx)) {
		printf("jit: analysis failed\n");
		return false;
	}

	if (!optimise(ctx)) {
		printf("jit: optimisation failed\n");
		return false;
	}

	if (!allocate(ctx)) {
		printf("jit: register allocation failed\n");
		return false;
	}

	if (!lower(ctx, fn)) {
		printf("jit: instruction lowering failed\n");
		return false;
	}

	return true;
}

bool BlockCompiler::build(shared::TranslationBlock& tb, IRContext& ctx)
{
	for (uint32_t idx = 0; idx < tb.ir_insn_count; idx++) {
		shared::IRInstruction *insn = &tb.ir_insn[idx];

		IRBlock& block = ctx.get_block_by_id(insn->ir_block);
		switch (insn->type) {
		case shared::IRInstruction::CALL:
			block.append_instruction(new instructions::IRCallInstruction(new IRFunctionOperand((void *)insn->operands[0].value)));
			break;

		default:
			printf("jit: unsupported instruction type %d\n", insn->type);
			return false;
		}
	}

	return false;
}


bool BlockCompiler::optimise(IRContext& ctx)
{
	return false;
}

bool BlockCompiler::allocate(IRContext& ctx)
{
	return true;
}

bool BlockCompiler::lower(IRContext& ctx, block_txln_fn& fn)
{
	LocalMemory allocator;
	X86Encoder encoder(allocator);

	encoder.push(REG_RBP);
	encoder.mov(REG_RSP, REG_RBP);

	encoder.push(REG_R14);
	encoder.mov(REG_RDI, REG_R14);



	printf("jit: encoding complete, buffer=%p, size=%d\n", encoder.get_buffer(), encoder.get_buffer_size());
	fn = (block_txln_fn)encoder.get_buffer();

	uint8_t *p = (uint8_t *)fn, *end = encoder.get_buffer() + encoder.get_buffer_size();
	printf("code:\n");
	while (p < end) {
		printf("  .byte 0x%02x\n", *p);
		p++;
	}

	return true;
}
