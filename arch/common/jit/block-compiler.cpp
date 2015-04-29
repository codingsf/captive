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

	if (!lower_context(ctx, fn)) {
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

		block.append_instruction(*insn);
	}

	ctx.dump();

	return true;
}


bool BlockCompiler::optimise(IRContext& ctx)
{
	return true;
}

bool BlockCompiler::allocate(IRContext& ctx)
{
	return true;
}

bool BlockCompiler::lower_context(IRContext& ctx, block_txln_fn& fn)
{
	LocalMemory allocator;
	X86Encoder encoder(allocator);

	// Function prologue
	encoder.push(REG_RBP);
	encoder.mov(REG_RSP, REG_RBP);
	encoder.push(REG_R15);
	encoder.push(REG_RBX);
	encoder.mov(REG_RDI, REG_R15);

	std::map<shared::IRBlockId, uint32_t> native_block_offsets;
	std::map<uint32_t, shared::IRBlockId> block_relocations;
	for (auto block : ctx.blocks()) {
		native_block_offsets[block->id()] = encoder.current_offset();

		if (!lower_block(*block, encoder, block_relocations)) {
			printf("jit: block encoding failed\n");
			return false;
		}
	}

	printf("jit: patching up relocations\n");

	for (auto reloc : block_relocations) {
		int32_t value = native_block_offsets[reloc.second];
		value -= reloc.first;
		value -= 4;

		printf("jit: offset=%x value=%d\n", reloc.first, value);
		uint32_t *slot = (uint32_t *)&encoder.get_buffer()[reloc.first];
		*slot = value;
	}

	encoder.hlt();
	printf("jit: encoding complete, buffer=%p, size=%d\n", encoder.get_buffer(), encoder.get_buffer_size());
	fn = (block_txln_fn)encoder.get_buffer();

	asm volatile("out %0, $0xff\n" :: "a"(15), "D"(fn), "S"(encoder.get_buffer_size()));

	return true;
}

bool BlockCompiler::lower_block(IRBlock& block, x86::X86Encoder& encoder, std::map<uint32_t, shared::IRBlockId>& block_relocations)
{
	for (auto insn : block.instructions()) {
		switch (insn->type()) {
		case IRInstruction::Call:
		{
			instructions::IRCallInstruction *calli = (instructions::IRCallInstruction *)insn;
			encoder.mov(X86Memory::get(REG_R15), REG_RDI);

			const std::vector<IROperand *>& args = calli->arguments();
			if (args.size() > 0)  {
				encode_operand_to_reg(encoder, *args[0], REG_RSI);
			}

			if (args.size() > 1)  {
				encode_operand_to_reg(encoder, *args[1], REG_RDX);
			}

			encoder.mov((uint64_t)((IRFunctionOperand *)calli->operands().front())->ptr(), REG_RBX);
			encoder.call(REG_RBX);
			break;
		}

		case IRInstruction::Jump:
		{
			instructions::IRJumpInstruction *jmpi = (instructions::IRJumpInstruction *)insn;

			uint32_t reloc_offset;
			encoder.jmp_reloc(reloc_offset);

			block_relocations[reloc_offset] = jmpi->target().block().id();
			break;
		}

		case IRInstruction::Return:
			encoder.pop(REG_RBX);
			encoder.pop(REG_R15);
			encoder.xorr(REG_EAX, REG_EAX);
			encoder.leave();
			encoder.ret();
			break;
		}
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

	case shared::IRInstruction::JMP:
	{
		IRBlockOperand *target = (IRBlockOperand *)operand_from_shared(ctx, &insn->operands[0]);
		assert(target->type() == IROperand::Block);

		return new instructions::IRJumpInstruction(*target);
	}

	case shared::IRInstruction::RET:
		return new instructions::IRRetInstruction();

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

void BlockCompiler::encode_operand_to_reg(X86Encoder& encoder, IROperand& operand, x86::X86Register& reg)
{
	switch (operand.type()) {
	case IROperand::Constant:
		encoder.mov(((IRConstantOperand&)operand).value(), reg);
		break;

	default:
		assert(false);
	}
}
