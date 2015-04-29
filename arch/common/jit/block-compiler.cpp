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
	IRContext ctx;

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
		shared::IRInstruction *shared_insn = &tb.ir_insn[idx];

		IRBlock& block = ctx.get_block_by_id(shared_insn->ir_block);
		IRInstruction *insn = instruction_from_shared(ctx, shared_insn);

		printf("jit: instruction: ");
		insn->dump();
		printf("\n");

		block.append_instruction(*insn);
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

IRInstruction* BlockCompiler::instruction_from_shared(IRContext& ctx, const shared::IRInstruction* insn)
{
	switch (insn->type) {
	case shared::IRInstruction::CALL:
	{
		IRFunctionOperand *fno = (IRFunctionOperand *)operand_from_shared(ctx, &insn->operands[0]);
		assert(fno->type() == IROperand::Function);

		instructions::IRCallInstruction *calli = new instructions::IRCallInstruction(*fno);

		for (int i = 1; i < 6; i++) {
			if (insn->operands[i].type != shared::IROperand::NONE) {
				calli->add_argument(*operand_from_shared(ctx, &insn->operands[i]));
			}
		}

		return calli;
	}

	default:
		printf("jit: unsupported instruction type %d\n", insn->type);
		assert(false);
	}
}

IROperand* BlockCompiler::operand_from_shared(IRContext& ctx, const shared::IROperand* operand)
{
	switch (operand->type) {
	case shared::IROperand::BLOCK:
		return new IRBlockOperand(ctx.get_block_by_id((shared::IRBlockId)operand->value));

	case shared::IROperand::CONSTANT:
		return new IRConstantOperand(operand->value, operand->size);

	case shared::IROperand::FUNC:
		return new IRFunctionOperand((void *)operand->value);

	case shared::IROperand::PC:
		return new IRPCOperand();

	case shared::IROperand::VREG:
		return new IRRegisterOperand(ctx.get_register_by_id((shared::IRRegId)operand->value, operand->size));

	default:
		printf("jit: unsupported operand type %d\n", operand->type);
		assert(false);
	}
}
