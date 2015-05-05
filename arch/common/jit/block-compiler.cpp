#include <jit/block-compiler.h>
#include <jit/translation-context.h>
#include <jit/ir.h>
#include <shared-jit.h>
#include <printf.h>
#include <queue>

using namespace captive::arch::jit;
using namespace captive::arch::x86;

BlockCompiler::BlockCompiler(shared::TranslationBlock& tb) : tb(tb), encoder(memory_allocator)
{

}

bool BlockCompiler::compile(block_txln_fn& fn)
{
	if (!optimise_tb()) {
		printf("jit: optimisation of translation block failed\n");
		return false;
	}

	if (!build()) {
		printf("jit: ir analysis failed\n");
		return false;
	}

	if (!optimise_ir()) {
		printf("jit: optimisation of ir failed\n");
		return false;
	}

	ir.dump();

	uint32_t max_stack = 0;
	if (!allocate(max_stack)) {
		printf("jit: register allocation failed\n");
		return false;
	}

	if (!lower(max_stack, fn)) {
		printf("jit: instruction lowering failed\n");
		return false;
	}

	return true;
}

bool BlockCompiler::optimise_tb()
{
	for (uint32_t idx = 0; idx < tb.ir_insn_count; idx++) {
		shared::IRInstruction *insn = &tb.ir_insn[idx];

		switch (insn->type) {
		case shared::IRInstruction::SHL:
			if (insn->operands[0].type == shared::IROperand::CONSTANT && insn->operands[0].value == 0) {
				insn->type = shared::IRInstruction::NOP;
			}

			break;
		}
	}

	return true;
}

bool BlockCompiler::build()
{
	// Build blocks and instructions
	for (uint32_t idx = 0; idx < tb.ir_insn_count; idx++) {
		shared::IRInstruction *shared_insn = &tb.ir_insn[idx];

		// Don't include NOPs
		if (shared_insn->type == shared::IRInstruction::NOP) continue;

		IRBlock& block = ir.get_block_by_id(shared_insn->ir_block);
		IRInstruction *insn = instruction_from_shared(ir, shared_insn);

		block.append_instruction(*insn);
	}

	// Build block CFG
	for (auto block : ir.blocks()) {
		for (auto succ : block->terminator().successors()) {
			block->add_successor(*succ);
			succ->add_predecessor(*block);
		}
	}

	// Build instruction register liveness analysis
	for (auto block : ir.blocks()) {
		block->calculate_liveness();
	}

	return true;
}


bool BlockCompiler::optimise_ir()
{
	if (!merge_blocks()) return false;
	return true;
}

bool BlockCompiler::merge_blocks()
{
retry:
	for (auto block : ir.blocks()) {
		if (block->successors().size() == 1) {
			auto succ = *(block->successors().begin());

			if (succ->predecessors().size() == 1) {
				printf("i've decided to merge %d into %d\n", succ->id(), block->id());

				block->terminator().remove_from_parent();

				for (auto insn : succ->instructions()) {
					block->append_instruction(*insn);
				}

				block->remove_successors();

				for (auto new_succ : succ->successors()) {
					block->add_successor(*new_succ);

					new_succ->remove_predecessor(*succ);
					new_succ->add_predecessor(*block);
				}

				succ->remove_from_parent();
				goto retry;
			}
		}
	}
	return true;
}

bool BlockCompiler::allocate(uint32_t& max_stack)
{
	max_stack = 0;

	for (auto block : ir.blocks()) {
		std::map<IRRegister *, uint32_t> alloc;
		std::list<uint32_t> avail;
		avail.push_back(0);
		avail.push_back(1);
		avail.push_back(2);
		avail.push_back(3);
		avail.push_back(4);
		avail.push_back(5);

		printf("local allocator on block %d\n", block->id());
		for (auto II =  block->instructions().begin(), IE = block->instructions().end(); II != IE; ++II) {
			IRInstruction *insn = *II;

			printf("insn: ");
			insn->dump();
			printf("\n");

			for (auto use : insn->uses()) {
				uint32_t live = alloc[&use->reg()];

				printf(" use: r%d reg=%d\n", use->reg().id(), live);
				use->allocate(IRRegisterOperand::Register, live);

				/*for (auto NI = II+1; NI != IE; ++NI) {

				}*/
			}

			for (auto def : insn->defs()) {
				if (alloc.find(&def->reg()) != alloc.end()) {
					def->allocate(IRRegisterOperand::Register, alloc[&def->reg()]);
				} else {
					if (avail.size() == 0) assert(false);

					uint32_t chosen = avail.front();
					avail.pop_front();

					alloc[&def->reg()] = chosen;

					printf(" def: r%d reg=%d\n", def->reg().id(), chosen);

					def->allocate(IRRegisterOperand::Register, chosen);
				}
			}
		}
	}

	max_stack += 4;
	return true;
}

bool BlockCompiler::lower(uint32_t max_stack, block_txln_fn& fn)
{
	// Function prologue
	encoder.push(REG_RBP);
	encoder.mov(REG_RSP, REG_RBP);
	encoder.push(REG_R15);
	encoder.push(REG_RBX);
	encoder.sub(max_stack, REG_RSP);
	encoder.mov(REG_RDI, REG_R15);

	register_assignments[0] = &REG_RAX;
	register_assignments[1] = &REG_RCX;
	register_assignments[2] = &REG_RDX;
	register_assignments[3] = &REG_RSI;
	register_assignments[4] = &REG_R8;
	register_assignments[5] = &REG_R9;

	std::map<shared::IRBlockId, uint32_t> native_block_offsets;
	for (auto block : ir.blocks()) {
		native_block_offsets[block->id()] = encoder.current_offset();

		if (!lower_block(*block)) {
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

	asm volatile("out %0, $0xff\n" :: "a"(15), "D"(fn), "S"(encoder.get_buffer_size()), "d"(tb.block_addr));

	return true;
}

bool BlockCompiler::lower_block(IRBlock& block)
{
	X86Register& tmp0_8 = REG_RBX;
	X86Register& tmp0_4 = REG_EBX;
	X86Register& tmp1_4 = REG_EDX;

	for (auto insn : block.instructions()) {
		encoder.nop();

		switch (insn->type()) {
		case IRInstruction::Call:
		{
			// TODO: save registers

			instructions::IRCallInstruction *calli = (instructions::IRCallInstruction *)insn;
			load_state_field(0, REG_RDI);

			const std::vector<IROperand *>& args = calli->arguments();
			if (args.size() > 0)  {
				encode_operand_to_reg(*args[0], REG_RSI);
			}

			if (args.size() > 1)  {
				encode_operand_to_reg(*args[1], REG_RDX);
			}

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)((IRFunctionOperand *)calli->operands().front())->ptr(), tmp0_8);
			encoder.call(tmp0_8);
			break;
		}

		case IRInstruction::Jump:
		{
			instructions::IRJumpInstruction *jmpi = (instructions::IRJumpInstruction *)insn;

			// Create a relocated jump instruction, and store the relocation offset and
			// target into the relocations list.
			uint32_t reloc_offset;
			encoder.jmp_reloc(reloc_offset);

			block_relocations[reloc_offset] = jmpi->target().block().id();
			break;
		}

		case IRInstruction::ReadRegister:
		{
			instructions::IRReadRegisterInstruction *rri = (instructions::IRReadRegisterInstruction *)insn;

			// Load the register state into a temporary
			load_state_field(8, tmp0_8);

			if (rri->offset().type() == IROperand::Constant) {
				IRConstantOperand& offset = (IRConstantOperand&)rri->offset();

				// Load a constant offset guest register into the storage location
				if (rri->storage().is_allocated_reg()) {
					encoder.mov(X86Memory::get(tmp0_8, offset.value()), register_from_operand(rri->storage()));
				} else if (rri->storage().is_allocated_stack()) {
					encoder.mov(X86Memory::get(tmp0_8, offset.value()), tmp0_4);
					encoder.mov(tmp0_4, stack_from_operand(rri->storage()));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::WriteRegister:
		{
			instructions::IRWriteRegisterInstruction *wri = (instructions::IRWriteRegisterInstruction *)insn;

			load_state_field(8, tmp0_8);

			if (wri->offset().type() == IROperand::Constant) {
				IRConstantOperand& offset = (IRConstantOperand&)wri->offset();

				if (wri->value().type() == IROperand::Constant) {
					assert(false);
				} else if (wri->value().type() == IROperand::Register) {
					IRRegisterOperand& value = (IRRegisterOperand&)wri->value();

					if (value.is_allocated_reg()) {
						encoder.mov(register_from_operand(value), X86Memory::get(tmp0_8, offset.value()));
					} else if (value.is_allocated_stack()) {
						encoder.mov(stack_from_operand(value), tmp1_4);
						encoder.mov(tmp1_4, X86Memory::get(tmp0_8, offset.value()));
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

		case IRInstruction::Move:
		{
			instructions::IRMovInstruction *movi = (instructions::IRMovInstruction *)insn;

			if (movi->source().type() == IROperand::Register) {
				IRRegisterOperand& source = (IRRegisterOperand&)movi->source();

				// mov vreg -> vreg
				if (source.is_allocated_reg()) {
					const X86Register& src_reg = register_from_operand(source);

					if (movi->destination().allocation_class() == IRRegisterOperand::Register) {
						// mov reg -> reg
						encoder.mov(src_reg, register_from_operand(movi->destination()));
					} else if (movi->destination().allocation_class() == IRRegisterOperand::Stack) {
						// mov reg -> stack
						encoder.mov(src_reg, stack_from_operand(movi->destination()));
					} else {
						assert(false);
					}
				} else if (source.is_allocated_stack()) {
					const X86Memory src_mem = stack_from_operand(source);

					if (movi->destination().allocation_class() == IRRegisterOperand::Register) {
						// mov stack -> reg
						encoder.mov(src_mem, register_from_operand(movi->destination()));
					} else if (movi->destination().allocation_class() == IRRegisterOperand::Stack) {
						// mov stack -> stack
						encoder.mov(src_mem, tmp0_4);
						encoder.mov(tmp0_4, stack_from_operand(movi->destination()));
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
				IRConstantOperand& source = (IRConstantOperand&)andi->source();

				if (andi->destination().is_allocated_reg()) {
					// and const -> reg
					encoder.andd(source.value(), register_from_operand(andi->destination()));
				} else if (andi->destination().is_allocated_stack()) {
					// and const -> stack
					encoder.andd(source.value(), stack_from_operand(andi->destination()));
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
			instructions::IRTruncInstruction *trunci = (instructions::IRTruncInstruction *)insn;

			if (trunci->source().type() == IROperand::Register) {
				IRRegisterOperand& source = (IRRegisterOperand&)trunci->source();

				if (source.is_allocated_stack() && trunci->destination().is_allocated_stack()) {
					encoder.mov(stack_from_operand(source), tmp0_4);

					switch (trunci->destination().reg().width()) {
					case 1: encoder.andd(0xff, tmp0_4); break;
					case 2: encoder.andd(0xffff, tmp0_4); break;
					case 4: encoder.andd(0xffffffff, tmp0_4); break;
					default: assert(false);
					}

					encoder.mov(tmp0_4, stack_from_operand(trunci->destination()));
				} else if (source.is_allocated_reg() && trunci->destination().is_allocated_reg()) {
					encoder.mov(register_from_operand(source), register_from_operand(trunci->destination()));

					switch (trunci->destination().reg().width()) {
					case 1: encoder.andd(0xff, register_from_operand(trunci->destination())); break;
					case 2: encoder.andd(0xffff, register_from_operand(trunci->destination())); break;
					case 4: encoder.andd(0xffffffff, register_from_operand(trunci->destination())); break;
					default: assert(false);
					}
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

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

	case shared::IRInstruction::LDPC:
	{
		IRRegisterOperand *dst = (IRRegisterOperand *)operand_from_shared(ctx, &insn->operands[0]);
		assert(dst->type() == IROperand::Register);

		return new instructions::IRLoadPCInstruction(*dst);
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

void BlockCompiler::load_state_field(uint32_t slot, x86::X86Register& reg)
{
	encoder.mov(X86Memory::get(REG_R15, slot), reg);
}

void BlockCompiler::encode_operand_to_reg(IROperand& operand, x86::X86Register& reg)
{
	switch (operand.type()) {
	case IROperand::Constant:
		encoder.mov(((IRConstantOperand&)operand).value(), reg);
		break;

	case IROperand::Register:
	{
		IRRegisterOperand& regop = (IRRegisterOperand&)operand;
		switch (regop.allocation_class()) {
		case IRRegisterOperand::Stack:
			encoder.mov(stack_from_operand(regop), reg);
			break;
		case IRRegisterOperand::Register:
			encoder.mov(register_from_operand(regop), reg);
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
