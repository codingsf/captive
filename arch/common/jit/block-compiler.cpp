#include <jit/block-compiler.h>
#include <jit/translation-context.h>
#include <jit/ir.h>
#include <x86/encode.h>
#include <shared-jit.h>
#include <local-memory.h>
#include <printf.h>
#include <queue>

using namespace captive::arch::jit;
using namespace captive::arch::x86;

BlockCompiler::BlockCompiler()
{

}

bool BlockCompiler::compile(shared::TranslationBlock& tb, block_txln_fn& fn)
{
	IRContext ctx;

	if (!pre_optimise(tb)) {
		printf("jit: pre-optimisation failed\n");
		return false;
	}

	if (!build(tb, ctx)) {
		printf("jit: analysis failed\n");
		return false;
	}

	if (!optimise(ctx)) {
		printf("jit: optimisation failed\n");
		return false;
	}

	uint32_t max_stack;
	if (!allocate(ctx, max_stack)) {
		printf("jit: register allocation failed\n");
		return false;
	}

	if (!lower_context(ctx, max_stack, fn)) {
		printf("jit: instruction lowering failed\n");
		return false;
	}

	return true;
}

bool BlockCompiler::pre_optimise(shared::TranslationBlock& tb)
{
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

bool BlockCompiler::allocate(IRContext& ctx, uint32_t& max_stack)
{
	max_stack = 0;

	for (auto block : ctx.blocks()) {
		for (auto insn : block->instructions()) {
			for (auto oper : insn->operands()) {
				if (oper->type() == IROperand::Register) {
					IRRegisterOperand *ro = (IRRegisterOperand *)oper;

					uint32_t stack_slot = ro->reg().id() * 4;
					ro->allocate(IRRegisterOperand::Stack, stack_slot);

					if (stack_slot > max_stack) {
						max_stack = stack_slot;
					}
				}
			}
		}
	}

	max_stack += 4;
	return true;
}

bool BlockCompiler::lower_context(IRContext& ctx, uint32_t max_stack, block_txln_fn& fn)
{
	LocalMemory allocator;
	X86Encoder encoder(allocator);
	X86Context x86(encoder);

	// Function prologue
	encoder.push(REG_RBP);
	encoder.mov(REG_RSP, REG_RBP);
	encoder.push(REG_R15);
	encoder.push(REG_RBX);
	//encoder.sub(max_stack, REG_RSP);
	encoder.mov(REG_RDI, REG_R15);

	x86.register_assignments[0] = &REG_RAX;
	x86.register_assignments[1] = &REG_RCX;
	x86.register_assignments[2] = &REG_RDX;
	x86.register_assignments[3] = &REG_RSI;

	std::map<shared::IRBlockId, uint32_t> native_block_offsets;
	for (auto block : ctx.blocks()) {
		native_block_offsets[block->id()] = encoder.current_offset();

		if (!lower_block(*block, x86)) {
			printf("jit: block encoding failed\n");
			return false;
		}
	}

	printf("jit: patching up relocations\n");

	for (auto reloc : x86.block_relocations) {
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

bool BlockCompiler::lower_block(IRBlock& block, X86Context& ctx)
{
	for (auto insn : block.instructions()) {
		ctx.encoder.nop();

		switch (insn->type()) {
		case IRInstruction::Call:
		{
			instructions::IRCallInstruction *calli = (instructions::IRCallInstruction *)insn;
			ctx.encoder.mov(X86Memory::get(REG_R15), REG_RDI);

			const std::vector<IROperand *>& args = calli->arguments();
			if (args.size() > 0)  {
				encode_operand_to_reg(ctx, *args[0], REG_RSI);
			}

			if (args.size() > 1)  {
				encode_operand_to_reg(ctx, *args[1], REG_RDX);
			}

			ctx.encoder.mov((uint64_t)((IRFunctionOperand *)calli->operands().front())->ptr(), REG_RBX);
			ctx.encoder.call(REG_RBX);
			break;
		}

		case IRInstruction::Jump:
		{
			instructions::IRJumpInstruction *jmpi = (instructions::IRJumpInstruction *)insn;

			uint32_t reloc_offset;
			ctx.encoder.jmp_reloc(reloc_offset);

			ctx.block_relocations[reloc_offset] = jmpi->target().block().id();
			break;
		}

		case IRInstruction::ReadRegister:
		{
			instructions::IRReadRegisterInstruction *rri = (instructions::IRReadRegisterInstruction *)insn;

			ctx.encoder.mov(X86Memory::get(REG_R14, 8), REG_RBX);

			if (rri->offset().type() == IROperand::Constant) {

				if (rri->storage().allocation_class() == IRRegisterOperand::Register) {
					auto f = reg_asn.find(rri->storage().allocation_data());
					ctx.encoder.mov(X86Memory::get(REG_RBX, ((IRConstantOperand&)rri->offset()).value()), *(f->second));
				} else if (rri->storage().allocation_class() == IRRegisterOperand::Stack) {
					ctx.encoder.mov(X86Memory::get(REG_RBX, ((IRConstantOperand&)rri->offset()).value()), REG_RBX);
					ctx.encoder.mov(REG_RBX, X86Memory::get(REG_RBP, rri->storage().allocation_data()));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::Move:
		{
			instructions::IRMovInstruction *movi = (instructions::IRMovInstruction *)insn;

			if (movi->source().type() == IROperand::Register) {
				// mov vreg -> vreg

				if (((IRRegisterOperand&)movi->source()).allocation_class() == IRRegisterOperand::Register) {
					if (movi->destination().allocation_class() == IRRegisterOperand::Register) {
						// mov reg -> reg
						const X86Register& src_reg = *(reg_asn.find(((IRRegisterOperand&)movi->source()).allocation_data())->second);
						const X86Register& dst_reg = *(reg_asn.find(movi->destination().allocation_data())->second);

						ctx.encoder.mov(src_reg, dst_reg);
					} else if (movi->destination().allocation_class() == IRRegisterOperand::Stack) {
						// mov reg -> stack
						const X86Register& src_reg = *(reg_asn.find(((IRRegisterOperand&)movi->source()).allocation_data())->second);
						const X86Memory dst_mem = X86Memory::get(REG_RBP, movi->destination().allocation_data());

						ctx.encoder.mov(src_reg, dst_mem);
					} else {
						assert(false);
					}
				} else if (((IRRegisterOperand&)movi->source()).allocation_class() == IRRegisterOperand::Stack) {
					if (movi->destination().allocation_class() == IRRegisterOperand::Register) {
						// mov stack -> reg
						const X86Memory src_mem = X86Memory::get(REG_RBP, ((IRRegisterOperand&)movi->source()).allocation_data());
						const X86Register& dst_reg = *(reg_asn.find(movi->destination().allocation_data())->second);

						ctx.encoder.mov(src_mem, dst_reg);
					} else if (movi->destination().allocation_class() == IRRegisterOperand::Stack) {
						// mov stack -> stack
						const X86Memory src_mem = X86Memory::get(REG_RBP, ((IRRegisterOperand&)movi->source()).allocation_data());
						const X86Memory dst_mem = X86Memory::get(REG_RBP, movi->destination().allocation_data());

						ctx.encoder.mov(src_mem, REG_RBX);
						ctx.encoder.mov(REG_RBX, dst_mem);
					} else {
						assert(false);
					}
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::BitwiseAnd:
		{
			instructions::IRBitwiseAndInstruction *andi = (instructions::IRBitwiseAndInstruction *)insn;

			if (andi->source().type() == IROperand::Register) {
				assert(false);
			} else if (andi->source().type() == IROperand::Constant) {
				if (andi->destination().allocation_class() == IRRegisterOperand::Register) {
					ctx.encoder.andd(((IRConstantOperand&)andi->source()).value(), *(reg_asn.find(andi->destination().allocation_data())->second));
				} else if (andi->destination().allocation_class() == IRRegisterOperand::Stack) {
					ctx.encoder.andd(((IRConstantOperand&)andi->source()).value(), X86Memory::get(REG_RBP, andi->destination().allocation_data()));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::Truncate:
		{
			instructions::IRBitwiseAndInstruction *andi = (instructions::IRBitwiseAndInstruction *)insn;

			if (andi->source().type() == IROperand::Register) {
				IRRegisterOperand& source = (IRRegisterOperand&)andi->source();

				if (source.allocation_class() == IRRegisterOperand::Stack && andi->destination().allocation_class() == IRRegisterOperand::Stack) {
					encode_operand_to_reg(ctx, source, REG_RBX);
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::Return:
			ctx.encoder.pop(REG_RBX);
			ctx.encoder.pop(REG_R15);
			ctx.encoder.xorr(REG_EAX, REG_EAX);
			ctx.encoder.leave();
			ctx.encoder.ret();
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

	case shared::IRInstruction::MOV:
	{
		IROperand *src = operand_from_shared(ctx, &insn->operands[0]);
		IRRegisterOperand *dst = (IRRegisterOperand *)operand_from_shared(ctx, &insn->operands[1]);

		assert(dst->type() == IROperand::Register);

		return new instructions::IRMovInstruction(*src, *dst);
	}

	case shared::IRInstruction::READ_REG:
	{
		IROperand *offset = operand_from_shared(ctx, &insn->operands[0]);
		IRRegisterOperand *storage = (IRRegisterOperand *)operand_from_shared(ctx, &insn->operands[1]);
		assert(storage->type() == IROperand::Register);

		return new instructions::IRReadRegisterInstruction(*offset, *storage);
	}

	case shared::IRInstruction::WRITE_REG:
	{
		IROperand *value = operand_from_shared(ctx, &insn->operands[0]);
		IROperand *offset = operand_from_shared(ctx, &insn->operands[1]);

		return new instructions::IRWriteRegisterInstruction(*value, *offset);
	}

	case shared::IRInstruction::ZX:
	case shared::IRInstruction::SX:
	case shared::IRInstruction::TRUNC:
	{
		IROperand *from = operand_from_shared(ctx, &insn->operands[0]);
		IRRegisterOperand *to = (IRRegisterOperand *)operand_from_shared(ctx, &insn->operands[1]);
		assert(to->type() == IROperand::Register);

		switch (insn->type) {
		case shared::IRInstruction::ZX: return new instructions::IRZXInstruction(*from, *to);
		case shared::IRInstruction::SX: return new instructions::IRSXInstruction(*from, *to);
		case shared::IRInstruction::TRUNC: return new instructions::IRTruncInstruction(*from, *to);
		}
	}

	case shared::IRInstruction::AND:
	case shared::IRInstruction::OR:
	case shared::IRInstruction::XOR:
	case shared::IRInstruction::ADD:
	case shared::IRInstruction::SUB:
	case shared::IRInstruction::MUL:
	case shared::IRInstruction::DIV:
	case shared::IRInstruction::MOD:
	case shared::IRInstruction::SHR:
	case shared::IRInstruction::SHL:
	case shared::IRInstruction::SAR:
	{
		IROperand *src = operand_from_shared(ctx, &insn->operands[0]);
		IRRegisterOperand *dst = (IRRegisterOperand *)operand_from_shared(ctx, &insn->operands[1]);

		assert(dst->type() == IROperand::Register);

		switch (insn->type) {
		case shared::IRInstruction::AND:
			return new instructions::IRBitwiseAndInstruction(*src, *dst);
		case shared::IRInstruction::OR:
			return new instructions::IRBitwiseOrInstruction(*src, *dst);
		case shared::IRInstruction::XOR:
			return new instructions::IRBitwiseXorInstruction(*src, *dst);
		case shared::IRInstruction::ADD:
			return new instructions::IRAddInstruction(*src, *dst);
		case shared::IRInstruction::SUB:
			return new instructions::IRSubInstruction(*src, *dst);
		case shared::IRInstruction::MUL:
			return new instructions::IRMulInstruction(*src, *dst);
		case shared::IRInstruction::DIV:
			return new instructions::IRDivInstruction(*src, *dst);
		case shared::IRInstruction::MOD:
			return new instructions::IRModInstruction(*src, *dst);
		case shared::IRInstruction::SHL:
			return new instructions::IRShiftLeftInstruction(*src, *dst);
		case shared::IRInstruction::SHR:
			return new instructions::IRShiftRightInstruction(*src, *dst);
		case shared::IRInstruction::SAR:
			return new instructions::IRArithmeticShiftRightInstruction(*src, *dst);
		}
	}

	case shared::IRInstruction::SET_CPU_MODE:
		return new instructions::IRSetCpuModeInstruction(*operand_from_shared(ctx, &insn->operands[0]));

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

void BlockCompiler::encode_operand_to_reg(X86Context& ctx, IROperand& operand, x86::X86Register& reg)
{
	switch (operand.type()) {
	case IROperand::Constant:
		ctx.encoder.mov(((IRConstantOperand&)operand).value(), reg);
		break;

	case IROperand::Register:
	{
		IRRegisterOperand& regop = (IRRegisterOperand&)operand;
		switch (regop.allocation_class()) {
		case IRRegisterOperand::Stack:
			ctx.encoder.mov(X86Memory::get(REG_RBP, regop.allocation_data()), reg);
			break;
		case IRRegisterOperand::Register:
			break;
		default:
			assert(false);
		}

		break;
	}

	default:
		assert(false);
	}
}
