#include <jit/block-compiler.h>
#include <jit/translation-context.h>
#include <jit/ir.h>
#include <shared-jit.h>
#include <printf.h>
#include <queue>

extern "C" void cpu_set_mode(void *cpu, uint8_t mode);
extern "C" void cpu_write_device(void *cpu, uint32_t devid, uint32_t reg, uint32_t val);
extern "C" void cpu_read_device(void *cpu, uint32_t devid, uint32_t reg, uint32_t& val);
extern "C" void jit_verify(void *cpu);

using namespace captive::arch::jit;
using namespace captive::arch::x86;

BlockCompiler::BlockCompiler(shared::TranslationBlock& tb) : tb(tb), encoder(memory_allocator)
{
	assign(0, REG_RAX, REG_EAX, REG_AX, REG_AL);
	assign(1, REG_RDX, REG_EDX, REG_DX, REG_DL);
	assign(2, REG_RSI, REG_ESI, REG_SI, REG_SIL);
	assign(3, REG_RDI, REG_EDI, REG_DI, REG_DIL);
	assign(4, REG_R8, REG_R8D, REG_R8W, REG_R8B);
	assign(5, REG_R9, REG_R9D, REG_R9W, REG_R9B);
	assign(6, REG_R10, REG_R10D, REG_R10W, REG_R10B);
}

bool BlockCompiler::compile(block_txln_fn& fn)
{
	if (!optimise_tb()) {
		printf("jit: optimisation of translation block failed\n");
		return false;
	}

	if (!build()) {
		printf("jit: ir creation failed\n");
		return false;
	}

	if (!optimise_ir()) {
		printf("jit: optimisation of ir failed\n");
		return false;
	}

	if (!analyse()) {
		printf("jit: analysis of ir failed\n");
		return false;
	}

	uint32_t max_stack = 0;
	if (!allocate(max_stack)) {
		printf("jit: register allocation failed\n");
		return false;
	}

	if (!die()) {
		printf("jit: dead instruction elimination failed\n");
		return false;
	}

	ir.dump();

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

	return true;
}

bool BlockCompiler::analyse()
{
	std::vector<IRBlock *> exit_blocks;

	// Invalidate liveness on all blocks, and detect exit blocks (i.e. blocks without successors)
	for (auto block : ir.blocks()) {
		block->invalidate_liveness();

		if (block->successors().size() == 0)
			exit_blocks.push_back(block);
	}

	//printf("*** here we go\n");
	// Calculate the liveness on every exit block, which will recursively calculate
	// liveness on their predecessors.
	for (auto block : exit_blocks) {
		block->calculate_liveness();
	}

	return true;
}

bool BlockCompiler::optimise_ir()
{
	if (!merge_blocks()) return false;
	if (!thread_jumps()) return false;
	if (!thread_rets()) return false;
	//if (!dse()) return false;

	return true;
}

bool BlockCompiler::die()
{
retry:
	for (auto block : ir.blocks()) {
		for (auto insn : block->instructions()) {
			for (auto oper : insn->operands()) {
				if (oper->type() == IROperand::Register) {
					IRRegisterOperand& regop = (IRRegisterOperand&)*oper;
					if (regop.allocation_class() == IRRegisterOperand::None) {
						printf("whoa there! this instruction has an unallocated register!\n");
						insn->remove_from_parent();
						goto retry;
					}
				}
			}
		}
	}

	return true;
}

bool BlockCompiler::dse()
{
retry:
	analyse();

	bool changed = false;
	for (auto block : ir.blocks()) {
		for (auto insn : block->instructions()) {
			for (auto def : insn->defs()) {
				if (insn->live_outs().count(&def->reg()) == 0) {
					printf("i think we can safely eliminate this instruction: ");
					insn->dump();
					printf("\n");

					insn->remove_from_parent();
					goto retry;
				}
			}
		}
	}

	return true;
}

bool BlockCompiler::thread_jumps()
{
	return true;
}

bool BlockCompiler::thread_rets()
{
	for (auto block : ir.blocks()) {
		if (block->terminator().type() == IRInstruction::Jump) {
			assert(block->terminator().successors().size() == 1);
			IRBlock *successor = block->terminator().successors().front();

			if (successor->instructions().front()->type() == IRInstruction::Return) {
				block->terminator().remove_from_parent();
				// TODO: Delete

				block->append_instruction(*new instructions::IRRetInstruction());
				block->remove_successors();
				successor->remove_predecessor(*block);

				if (successor->predecessors().size() == 0) {
					successor->remove_from_parent();
					// TODO: Delete
				}
			}
		}
	}

	return true;
}

//#define DEBUG_MERGE_BLOCKS

bool BlockCompiler::merge_blocks()
{
retry:
	for (auto block : ir.blocks()) {
		if (block->successors().size() == 1) {
			auto succ = *(block->successors().begin());

			if (succ->predecessors().size() == 1) {
#ifdef DEBUG_MERGE_BLOCKS
				printf("i've decided to merge %d into %d\n", succ->id(), block->id());
#endif

				// Detach the terminator instruction from the new parent block.
				block->terminator().remove_from_parent();

				// TODO: delete terminator

				// Re-parent all instructions in the sucessor block into the
				// new parent block.
				for (auto insn : succ->instructions()) {
					block->append_instruction(*insn);
				}

				// Remove the successors from the parent block.
				block->remove_successors();

				// Copy the successors from the child block into the
				// successors of the parent block, and fix up the predecessors
				// in the child block's successors.
				for (auto new_succ : succ->successors()) {
					block->add_successor(*new_succ);

					new_succ->remove_predecessor(*succ);
					new_succ->add_predecessor(*block);
				}

				// Detach the child block from the parent.
				succ->remove_from_parent();

				// TODO: delete child block
				goto retry;
			}
		}
	}

	return true;
}

//#define DEBUG_ALLOCATOR

bool BlockCompiler::allocate(uint32_t& max_stack)
{
	max_stack = 0;

	std::set<IRRegister *> global_live;
	std::map<IRRegister *, uint32_t> global_regs;
	uint32_t next_global = 0;

	for (auto block : ir.blocks()) {
		std::set<IRRegister *> live;
		std::map<IRRegister *, uint32_t> alloc;
		std::list<uint32_t> avail;
		avail.push_back(0);
		avail.push_back(1);
		avail.push_back(2);
		avail.push_back(3);
		avail.push_back(4);
		avail.push_back(5);
		avail.push_back(6);

#ifdef DEBUG_ALLOCATOR
		printf("local allocator on block %d ", block->id());
		printf("IN={ ");
#endif
		for (auto in : block->live_ins()) {
#ifdef DEBUG_ALLOCATOR
			printf("r%d ", in->id());
#endif

			if (global_live.count(in) == 0) {
				global_live.insert(in);
				global_regs[in] = next_global++;
			}
		}
#ifdef DEBUG_ALLOCATOR
		printf("} OUT={ ");
#endif
		for (auto out : block->live_outs()) {
#ifdef DEBUG_ALLOCATOR
			printf("r%d ", out->id());
#endif
			if (global_live.count(out) == 0) {
				global_live.insert(out);
				global_regs[out] = next_global++;
			}
		}
#ifdef DEBUG_ALLOCATOR
		printf("}\n");
#endif

		for (auto II =  block->instructions().begin(), IE = block->instructions().end(); II != IE; ++II) {
			IRInstruction *insn = *II;

#ifdef DEBUG_ALLOCATOR
			printf("  insn: ");
			insn->dump();
			printf("\n");
#endif

			for (auto in : insn->live_ins()) {
				if (global_live.count(in)) continue;

				if (insn->live_outs().count(in) == 0) {
					uint32_t host_reg = alloc[in];

#ifdef DEBUG_ALLOCATOR
					printf("    register v%d dead, h%d returned\n", in->id(), host_reg);
#endif

					for (auto x : avail) {
						if (x == host_reg) {
							printf("nope!\n");
							abort();
						}
					}

					avail.push_front(host_reg);
					live.erase(in);
				}
			}

			for (auto out : insn->live_outs()) {
				if (global_live.count(out)) continue;

				if (live.count(out) == 0) {
					if (avail.size() == 0) assert(false);

					uint32_t chosen = avail.front();
					avail.pop_front();

					alloc[out] = chosen;
					live.insert(out);

#ifdef DEBUG_ALLOCATOR
					printf("    register v%d allocated to h%d\n", out->id(), chosen);
#endif
				} else {
#ifdef DEBUG_ALLOCATOR
					printf("    register v%d already h%d\n", out->id(), alloc[out]);
#endif
				}
			}

			for (auto oper : insn->operands()) {
				if (oper->type() == IROperand::Register) {
					IRRegisterOperand *regop = (IRRegisterOperand *)oper;

					if (global_live.count(&regop->reg())) {
						regop->allocate(IRRegisterOperand::Stack, global_regs[&regop->reg()]);
					} else if (alloc.count(&regop->reg())) {
						regop->allocate(IRRegisterOperand::Register, alloc[&regop->reg()]);
					}
				}
			}
		}
	}

	max_stack = (next_global * 4) + 4;
	return true;
}

bool BlockCompiler::lower(uint32_t max_stack, block_txln_fn& fn)
{
	// Function prologue
	encoder.push(REG_RBP);
	encoder.mov(REG_RSP, REG_RBP);
	encoder.sub(max_stack, REG_RSP);

	encoder.push(REG_R15);
	encoder.push(REG_R14);
	encoder.push(REG_RBX);

	encoder.mov(REG_RDI, REG_R14);
	load_state_field(8, REG_R15);

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

	printf("jit: encoding complete, block=%08x, buffer=%p, size=%d\n", tb.block_addr, encoder.get_buffer(), encoder.get_buffer_size());
	fn = (block_txln_fn)encoder.get_buffer();

	asm volatile("out %0, $0xff\n" :: "a"(15), "D"(fn), "S"(encoder.get_buffer_size()), "d"(tb.block_addr));

	return true;
}

bool BlockCompiler::lower_block(IRBlock& block)
{
	X86Register& guest_regs_reg = REG_R15;
	X86Register& tmp0_8 = REG_RBX;
	X86Register& tmp0_4 = REG_EBX;
	X86Register& tmp0_2 = REG_BX;
	X86Register& tmp0_1 = REG_BL;
	X86Register& tmp1_4 = REG_ECX;

	for (auto insn : block.instructions()) {
		switch (insn->type()) {
		case IRInstruction::Call:
		{
			emit_save_reg_state();

			instructions::IRCallInstruction *calli = (instructions::IRCallInstruction *)insn;
			load_state_field(0, REG_RDI);

			const std::vector<IROperand *>& args = calli->arguments();
			if (args.size() > 0)  {
				encode_operand_to_reg(*args[0], REG_ESI);
			}

			if (args.size() > 1)  {
				encode_operand_to_reg(*args[1], REG_EDX);
			}

			if (args.size() > 2)  {
				encode_operand_to_reg(*args[1], REG_ECX);
			}

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)((IRFunctionOperand *)calli->operands().front())->ptr(), tmp0_8);
			encoder.call(tmp0_8);

			emit_restore_reg_state();
			break;
		}

		case IRInstruction::SetCPUMode:
		{
			emit_save_reg_state();

			instructions::IRSetCpuModeInstruction *scmi = (instructions::IRSetCpuModeInstruction *)insn;
			load_state_field(0, REG_RDI);

			encode_operand_to_reg(scmi->new_mode(), REG_ESI);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&cpu_set_mode, tmp0_8);
			encoder.call(tmp0_8);

			emit_restore_reg_state();

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

		case IRInstruction::Branch:
		{
			instructions::IRBranchInstruction *branchi = (instructions::IRBranchInstruction *)insn;

			if (branchi->condition().type() == IROperand::Register) {
				IRRegisterOperand& cond = (IRRegisterOperand&)branchi->condition();

				if (cond.is_allocated_reg()) {
					encoder.test(register_from_operand(cond), register_from_operand(cond));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			{
				uint32_t reloc_offset;
				encoder.jnz_reloc(reloc_offset);
				block_relocations[reloc_offset] = branchi->true_target().block().id();
			}

			{
				uint32_t reloc_offset;
				encoder.jmp_reloc(reloc_offset);
				block_relocations[reloc_offset] = branchi->false_target().block().id();
			}

			break;
		}

		case IRInstruction::ReadRegister:
		{
			instructions::IRReadRegisterInstruction *rri = (instructions::IRReadRegisterInstruction *)insn;

			if (rri->offset().type() == IROperand::Constant) {
				IRConstantOperand& offset = (IRConstantOperand&)rri->offset();

				// Load a constant offset guest register into the storage location
				if (rri->storage().is_allocated_reg()) {
					encoder.mov(X86Memory::get(guest_regs_reg, offset.value()), register_from_operand(rri->storage()));
				} else if (rri->storage().is_allocated_stack()) {
					encoder.mov(X86Memory::get(guest_regs_reg, offset.value()), tmp0_4);
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

			if (wri->offset().type() == IROperand::Constant) {
				IRConstantOperand& offset = (IRConstantOperand&)wri->offset();

				if (wri->value().type() == IROperand::Constant) {
					IRConstantOperand& value = (IRConstantOperand&)wri->value();

					switch (value.width()) {
					case 8:
						encoder.mov8(value.value(), X86Memory::get(guest_regs_reg, offset.value()));
						break;
					case 4:
						encoder.mov4(value.value(), X86Memory::get(guest_regs_reg, offset.value()));
						break;
					case 2:
						encoder.mov2(value.value(), X86Memory::get(guest_regs_reg, offset.value()));
						break;
					case 1:
						encoder.mov1(value.value(), X86Memory::get(guest_regs_reg, offset.value()));
						break;
					default: assert(false);
					}
				} else if (wri->value().type() == IROperand::Register) {
					IRRegisterOperand& value = (IRRegisterOperand&)wri->value();

					if (value.is_allocated_reg()) {
						encoder.mov(register_from_operand(value), X86Memory::get(guest_regs_reg, offset.value()));
					} else if (value.is_allocated_stack()) {
						encoder.mov(stack_from_operand(value), tmp1_4);
						encoder.mov(tmp1_4, X86Memory::get(guest_regs_reg, offset.value()));
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

						X86Register& dst_reg = register_from_operand(movi->destination());
						if (src_reg != dst_reg) {
							encoder.mov(src_reg, dst_reg);
						}
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
			} else if (movi->source().type() == IROperand::Constant) {
				IRConstantOperand& source = (IRConstantOperand&)movi->source();

				if (movi->destination().is_allocated_reg()) {
					// mov imm -> reg
					encoder.mov(source.value(), register_from_operand(movi->destination()));
				} else if (movi->destination().is_allocated_stack()) {
					// mov imm -> stack
					switch (movi->destination().reg().width()) {
					case 1: encoder.mov1(source.value(), stack_from_operand(movi->destination())); break;
					case 2: encoder.mov2(source.value(), stack_from_operand(movi->destination())); break;
					case 4: encoder.mov4(source.value(), stack_from_operand(movi->destination())); break;
					case 8: encoder.mov8(source.value(), stack_from_operand(movi->destination())); break;
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

		case IRInstruction::ZeroExtend:
		case IRInstruction::SignExtend:
		{
			instructions::IRChangeSizeInstruction *csi = (instructions::IRChangeSizeInstruction *)insn;

			if (csi->source().type() == IROperand::Register) {
				IRRegisterOperand& source = (IRRegisterOperand&)csi->source();

				if (source.reg().width() == 4 && csi->destination().reg().width() == 8) {
					if (csi->type() == IRInstruction::SignExtend) {
						if (source.is_allocated_reg() && csi->destination().is_allocated_reg()) {
							encoder.mov(register_from_operand(source), register_from_operand(csi->destination(), 4));
						} else {
							assert(false);
						}
					} else {
						if (source.is_allocated_reg() && csi->destination().is_allocated_reg()) {
							encoder.movsx(register_from_operand(source), register_from_operand(csi->destination()));
						} else {
							assert(false);
						}
					}
				} else {
					if (source.is_allocated_reg() && csi->destination().is_allocated_reg()) {
						switch (csi->type()) {
						case IRInstruction::ZeroExtend:
							encoder.movzx(register_from_operand(source), register_from_operand(csi->destination()));
							break;

						case IRInstruction::SignExtend:
							encoder.movsx(register_from_operand(source), register_from_operand(csi->destination()));
							break;
						}
					} else if (source.is_allocated_reg() && csi->destination().is_allocated_stack()) {
						assert(false);
					} else if (source.is_allocated_stack() && csi->destination().is_allocated_reg()) {
						unspill(source, tmp0_4);

						X86Register *tmpreg;
						switch (source.reg().width()) {
						case 1:	tmpreg = &tmp0_1; break;
						case 2:	tmpreg = &tmp0_2; break;
						default: assert(false);
						}

						switch (csi->type()) {
						case IRInstruction::ZeroExtend:
							encoder.movzx(*tmpreg, register_from_operand(csi->destination()));
							break;

						case IRInstruction::SignExtend:
							encoder.movsx(*tmpreg, register_from_operand(csi->destination()));
							break;
						}
					} else if (source.is_allocated_stack() && csi->destination().is_allocated_stack()) {
						assert(false);
					} else {
						assert(false);
					}
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::BitwiseAnd:
		case IRInstruction::BitwiseOr:
		case IRInstruction::BitwiseXor:
		{
			instructions::IRArithmeticInstruction *ai = (instructions::IRArithmeticInstruction *)insn;

			if (ai->source().type() == IROperand::Register) {
				IRRegisterOperand& source = (IRRegisterOperand&)ai->source();

				if (source.is_allocated_reg() && ai->destination().is_allocated_reg()) {
					// OPER reg -> reg
					switch (ai->type()) {
					case IRInstruction::BitwiseAnd:
						encoder.andd(register_from_operand(source), register_from_operand(ai->destination()));
						break;
					case IRInstruction::BitwiseOr:
						encoder.orr(register_from_operand(source), register_from_operand(ai->destination()));
						break;
					case IRInstruction::BitwiseXor:
						encoder.xorr(register_from_operand(source), register_from_operand(ai->destination()));
						break;
					}
				} else {
					assert(false);
				}
			} else if (ai->source().type() == IROperand::Constant) {
				IRConstantOperand& source = (IRConstantOperand&)ai->source();

				if (ai->destination().is_allocated_reg()) {
					// OPER const -> reg

					switch (ai->type()) {
					case IRInstruction::BitwiseAnd:
						encoder.andd(source.value(), register_from_operand(ai->destination()));
						break;
					case IRInstruction::BitwiseOr:
						encoder.orr(source.value(), register_from_operand(ai->destination()));
						break;
					case IRInstruction::BitwiseXor:
						encoder.xorr(source.value(), register_from_operand(ai->destination()));
						break;
					}
				} else if (ai->destination().is_allocated_stack()) {
					// OPER const -> stack

					switch (ai->type()) {
					case IRInstruction::BitwiseAnd:
						encoder.andd(source.value(), stack_from_operand(ai->destination()));
						break;
					case IRInstruction::BitwiseOr:
						encoder.orr(source.value(), stack_from_operand(ai->destination()));
						break;
					case IRInstruction::BitwiseXor:
						encoder.xorr(source.value(), stack_from_operand(ai->destination()));
						break;
					}
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::Add:
		case IRInstruction::Sub:
		{
			instructions::IRArithmeticInstruction *ai = (instructions::IRArithmeticInstruction *)insn;

			if (ai->source().type() == IROperand::Register) {
				IRRegisterOperand& source = (IRRegisterOperand&)ai->source();

				if (source.is_allocated_reg() && ai->destination().is_allocated_reg()) {
					switch (ai->type()) {
					case IRInstruction::Add:
						encoder.add(register_from_operand(source), register_from_operand(ai->destination()));
						break;
					case IRInstruction::Sub:
						encoder.sub(register_from_operand(source), register_from_operand(ai->destination()));
						break;
					}
				} else if (source.is_allocated_reg() && ai->destination().is_allocated_stack()) {
					assert(false);
				} else if (source.is_allocated_stack() && ai->destination().is_allocated_reg()) {
					switch (ai->type()) {
					case IRInstruction::Add:
						encoder.add(stack_from_operand(source), register_from_operand(ai->destination()));
						break;
					case IRInstruction::Sub:
						encoder.sub(stack_from_operand(source), register_from_operand(ai->destination()));
						break;
					}
				} else if (source.is_allocated_stack() && ai->destination().is_allocated_stack()) {
					assert(false);
				} else {
					assert(false);
				}
			} else if (ai->source().type() == IROperand::PC) {
				IRPCOperand& source = (IRPCOperand&)ai->source();

				if (ai->destination().is_allocated_reg()) {
					uint32_t pc = source.value();

					switch (ai->type()) {
					case IRInstruction::Add:
						encoder.add(pc, register_from_operand(ai->destination()));
						break;
					case IRInstruction::Sub:
						encoder.sub(pc, register_from_operand(ai->destination()));
						break;
					}
				} else {
					assert(false);
				}
			} else if (ai->source().type() == IROperand::Constant) {
				IRConstantOperand& source = (IRConstantOperand&)ai->source();

				if (ai->destination().is_allocated_reg()) {
					encoder.add(source.value(), register_from_operand(ai->destination()));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::LoadPC:
		{
			instructions::IRLoadPCInstruction *ldpci = (instructions::IRLoadPCInstruction *)insn;

			if (ldpci->destination().is_allocated_reg()) {
				encoder.mov(X86Memory::get(REG_R15, 60), register_from_operand(ldpci->destination()));
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

				if (source.is_allocated_reg() && trunci->destination().is_allocated_reg()) {
					// trunc reg -> reg
					encoder.mov(register_from_operand(source), register_from_operand(trunci->destination(), source.reg().width()));
				} else if (source.is_allocated_reg() && trunci->destination().is_allocated_stack()) {
					// trunc reg -> stack
					encoder.mov(register_from_operand(source), stack_from_operand(trunci->destination()));
				} else if (source.is_allocated_stack() && trunci->destination().is_allocated_reg()) {
					// trunc stack -> reg
					encoder.mov(stack_from_operand(source), register_from_operand(trunci->destination()));
				} else if (source.is_allocated_stack() && trunci->destination().is_allocated_stack()) {
					// trunc stack -> stack
					encoder.mov(stack_from_operand(source), tmp0_4);
					encoder.mov(tmp0_4, stack_from_operand(trunci->destination()));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::ShiftLeft:
		case IRInstruction::ShiftRight:
		case IRInstruction::ArithmeticShiftRight:
		{
			instructions::IRShiftInstruction *si = (instructions::IRShiftInstruction *)insn;

			if (si->amount().type() == IROperand::Constant) {
				IRConstantOperand& amount = (IRConstantOperand&)si->amount();

				if (si->operand().is_allocated_reg()) {
					X86Register& operand = register_from_operand(si->operand());

					switch (si->type()) {
					case IRInstruction::ShiftLeft:
						encoder.shl(amount.value(), operand);
						break;
					case IRInstruction::ShiftRight:
						encoder.shr(amount.value(), operand);
						break;
					case IRInstruction::ArithmeticShiftRight:
						encoder.sar(amount.value(), operand);
						break;
					}
				} else {
					assert(false);
				}
			} else if (si->amount().type() == IROperand::Register) {
				IRRegisterOperand& amount = (IRRegisterOperand&)si->amount();

				if (amount.is_allocated_reg()) {
					if (si->operand().is_allocated_reg()) {
						X86Register& operand = register_from_operand(si->operand());

						encoder.mov(register_from_operand(amount, 1), REG_CL);

						switch (si->type()) {
						case IRInstruction::ShiftLeft:
							encoder.shl(REG_CL, operand);
							break;
						case IRInstruction::ShiftRight:
							encoder.shr(REG_CL, operand);
							break;
						case IRInstruction::ArithmeticShiftRight:
							encoder.sar(REG_CL, operand);
							break;
						}
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

		case IRInstruction::Return:
			encoder.pop(REG_RBX);
			encoder.pop(REG_R14);
			encoder.pop(REG_R15);
			encoder.xorr(REG_EAX, REG_EAX);
			encoder.leave();
			encoder.ret();
			break;

		case IRInstruction::CompareEqual:
		case IRInstruction::CompareNotEqual:
		case IRInstruction::CompareLessThan:
		case IRInstruction::CompareLessThanOrEqual:
		case IRInstruction::CompareGreaterThan:
		case IRInstruction::CompareGreaterThanOrEqual:
		{
			instructions::IRComparisonInstruction *ci = (instructions::IRComparisonInstruction *)insn;

			switch (ci->lhs().type()) {
			case IROperand::Register:
			{
				IRRegisterOperand& lhs = (IRRegisterOperand&)ci->lhs();

				switch (ci->rhs().type()) {
				case IROperand::Register: {
					IRRegisterOperand& rhs = (IRRegisterOperand&)ci->rhs();

					if (lhs.is_allocated_reg() && rhs.is_allocated_reg()) {
						encoder.cmp(register_from_operand(lhs), register_from_operand(rhs));
					} else {
						assert(false);
					}

					break;
				}

				case IROperand::Constant: {
					IRConstantOperand& rhs = (IRConstantOperand&)ci->rhs();

					if (lhs.is_allocated_reg()) {
						encoder.cmp(rhs.value(), register_from_operand(lhs));
					} else if (lhs.is_allocated_stack()) {
						encoder.cmp(rhs.value(), stack_from_operand(lhs));
					} else {
						assert(false);
					}

					break;
				}

				default: assert(false);
				}

				break;
			}

			case IROperand::Constant:
				switch (ci->rhs().type()) {
				case IROperand::Register: assert(false);
				default: assert(false);
				}

				break;

			default: assert(false);
			}

			switch (ci->type()) {
			case IRInstruction::CompareEqual:
				encoder.sete(register_from_operand(ci->destination()));
				break;

			case IRInstruction::CompareNotEqual:
				encoder.setne(register_from_operand(ci->destination()));
				break;

			case IRInstruction::CompareLessThan:
				encoder.setl(register_from_operand(ci->destination()));
				break;

			case IRInstruction::CompareLessThanOrEqual:
				encoder.setle(register_from_operand(ci->destination()));
				break;

			case IRInstruction::CompareGreaterThan:
				encoder.setg(register_from_operand(ci->destination()));
				break;

			case IRInstruction::CompareGreaterThanOrEqual:
				encoder.setge(register_from_operand(ci->destination()));
				break;

			default:
				assert(false);
			}

			break;
		}

		case IRInstruction::ReadMemory:
		{
			instructions::IRReadMemoryInstruction *rmi = (instructions::IRReadMemoryInstruction *)insn;

			if (rmi->offset().type() == IROperand::Register) {
				IRRegisterOperand& offset = (IRRegisterOperand&)rmi->offset();

				if (offset.is_allocated_reg() && rmi->storage().is_allocated_reg()) {
					// mov (reg), reg
					encoder.mov(X86Memory::get(register_from_operand(offset)), register_from_operand(rmi->storage()));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::WriteMemory:
		{
			instructions::IRWriteMemoryInstruction *wmi = (instructions::IRWriteMemoryInstruction *)insn;

			if (wmi->offset().type() == IROperand::Register) {
				IRRegisterOperand& offset = (IRRegisterOperand&)wmi->offset();

				if (wmi->value().type() == IROperand::Register) {
					IRRegisterOperand& value = (IRRegisterOperand&)wmi->value();

					if (offset.is_allocated_reg() && value.is_allocated_reg()) {
						// mov reg, (reg)

						encoder.mov(register_from_operand(value), X86Memory::get(register_from_operand(offset)));
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

		case IRInstruction::WriteDevice:
		{
			instructions::IRWriteDeviceInstruction *wdi = (instructions::IRWriteDeviceInstruction *)insn;

			emit_save_reg_state();

			load_state_field(0, REG_RDI);

			encode_operand_to_reg(wdi->device(), REG_RSI);
			encode_operand_to_reg(wdi->offset(), REG_RDX);
			encode_operand_to_reg(wdi->data(), REG_RCX);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&cpu_write_device, tmp0_8);
			encoder.call(tmp0_8);

			emit_restore_reg_state();

			break;
		}

		case IRInstruction::ReadDevice:
		{
			instructions::IRReadDeviceInstruction *rdi = (instructions::IRReadDeviceInstruction *)insn;

			emit_save_reg_state();

			load_state_field(0, REG_RDI);

			encode_operand_to_reg(rdi->device(), REG_RSI);
			encode_operand_to_reg(rdi->offset(), REG_RDX);

			// Allocate a slot on the stack for the reference argument
			encoder.push(0);

			// Load the address of the stack slot into RCX
			encoder.lea(X86Memory::get(REG_RSP), REG_RCX);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&cpu_read_device, tmp0_8);
			encoder.call(tmp0_8);

			// Pop the reference argument value into the destination register
			if (rdi->storage().is_allocated_reg()) {
				encoder.pop(register_from_operand(rdi->storage(), 8));
			} else {
				assert(false);
			}

			emit_restore_reg_state();

			break;
		}

		case IRInstruction::Verify:
		{
			encoder.nop(X86Memory::get(REG_RAX, ((instructions::IRVerifyInstruction *)insn)->pc().value()));

			emit_save_reg_state();

			load_state_field(0, REG_RDI);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&jit_verify, tmp0_8);
			encoder.call(tmp0_8);

			emit_restore_reg_state();

			break;
		}

		default:
			printf("cannot lower instruction: ");
			insn->dump();
			printf("\n");
			abort();
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

	case shared::IRInstruction::BRANCH:
	{
		IROperand *cond = operand_from_shared(ctx, &insn->operands[0]);

		IRBlockOperand *tt = (IRBlockOperand *)operand_from_shared(ctx, &insn->operands[1]);
		IRBlockOperand *ft = (IRBlockOperand *)operand_from_shared(ctx, &insn->operands[2]);
		assert(tt->type() == IROperand::Block && ft->type() == IROperand::Block);

		return new instructions::IRBranchInstruction(*cond, *tt, *ft);
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

	case shared::IRInstruction::CMPEQ:
	case shared::IRInstruction::CMPNE:
	case shared::IRInstruction::CMPLT:
	case shared::IRInstruction::CMPLTE:
	case shared::IRInstruction::CMPGT:
	case shared::IRInstruction::CMPGTE:
	{
		IROperand *lhs = operand_from_shared(ctx, &insn->operands[0]);
		IROperand *rhs = (IROperand *)operand_from_shared(ctx, &insn->operands[1]);

		IRRegisterOperand *dst = (IRRegisterOperand *)operand_from_shared(ctx, &insn->operands[2]);
		assert(dst->type() == IROperand::Register);

		switch (insn->type) {
		case shared::IRInstruction::CMPEQ: return new instructions::IRCompareEqualInstruction(*lhs, *rhs, *dst);
		case shared::IRInstruction::CMPNE: return new instructions::IRCompareNotEqualInstruction(*lhs, *rhs, *dst);
		case shared::IRInstruction::CMPLT: return new instructions::IRCompareLessThanInstruction(*lhs, *rhs, *dst);
		case shared::IRInstruction::CMPLTE: return new instructions::IRCompareLessThanOrEqualInstruction(*lhs, *rhs, *dst);
		case shared::IRInstruction::CMPGT: return new instructions::IRCompareGreaterThanInstruction(*lhs, *rhs, *dst);
		case shared::IRInstruction::CMPGTE: return new instructions::IRCompareGreaterThanOrEqualInstruction(*lhs, *rhs, *dst);
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

	case shared::IRInstruction::WRITE_DEVICE:
	{
		IROperand *dev = (IROperand *)operand_from_shared(ctx, &insn->operands[0]);
		IROperand *off = (IROperand *)operand_from_shared(ctx, &insn->operands[1]);
		IROperand *data = (IROperand *)operand_from_shared(ctx, &insn->operands[2]);

		return new instructions::IRWriteDeviceInstruction(*dev, *off, *data);
	}

	case shared::IRInstruction::READ_DEVICE:
	{
		IROperand *dev = (IROperand *)operand_from_shared(ctx, &insn->operands[0]);
		IROperand *off = (IROperand *)operand_from_shared(ctx, &insn->operands[1]);
		IRRegisterOperand *data = (IRRegisterOperand *)operand_from_shared(ctx, &insn->operands[2]);
		assert(data->type() == IROperand::Register);

		return new instructions::IRReadDeviceInstruction(*dev, *off, *data);
	}

	case shared::IRInstruction::WRITE_MEM:
	{
		IROperand *val = (IROperand *)operand_from_shared(ctx, &insn->operands[0]);
		IROperand *off = (IROperand *)operand_from_shared(ctx, &insn->operands[1]);

		return new instructions::IRWriteMemoryInstruction(*val, *off);
	}

	case shared::IRInstruction::READ_MEM:
	{
		IROperand *off = (IROperand *)operand_from_shared(ctx, &insn->operands[0]);
		IRRegisterOperand *dst = (IRRegisterOperand *)operand_from_shared(ctx, &insn->operands[1]);
		assert(dst->type() == IROperand::Register);

		return new instructions::IRReadMemoryInstruction(*off, *dst);
	}

	case shared::IRInstruction::VERIFY:
	{
		IRPCOperand *pc = (IRPCOperand *)operand_from_shared(ctx, &insn->operands[0]);
		assert(pc->type() == IROperand::PC);

		return new instructions::IRVerifyInstruction(*pc);
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
		return new IRPCOperand(operand->value);

	case shared::IROperand::VREG:
		return new IRRegisterOperand(ctx.get_register_by_id((shared::IRRegId)operand->value, operand->size));

	default:
		printf("jit: unsupported operand type %d\n", operand->type);
		assert(false);
	}
}

void BlockCompiler::load_state_field(uint32_t slot, x86::X86Register& reg)
{
	encoder.mov(X86Memory::get(REG_R14, slot), reg);
}

void BlockCompiler::encode_operand_to_reg(IROperand& operand, x86::X86Register& reg)
{
	switch (operand.type()) {
	case IROperand::Constant:
	{
		IRConstantOperand& constant = (IRConstantOperand&)operand;
		if (constant.value() == 0) {
			encoder.xorr(reg, reg);
		} else {
			encoder.mov(constant.value(), reg);
		}

		break;
	}

	case IROperand::Register:
	{
		IRRegisterOperand& regop = (IRRegisterOperand&)operand;
		switch (regop.allocation_class()) {
		case IRRegisterOperand::Stack:
			encoder.mov(stack_from_operand(regop), reg);
			break;

		case IRRegisterOperand::Register:
			encoder.mov(register_from_operand(regop, reg.size), reg);
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

void BlockCompiler::emit_save_reg_state()
{
	encoder.push(REG_RSI);
	encoder.push(REG_RDI);
	encoder.push(REG_RAX);
	encoder.push(REG_RCX);
	encoder.push(REG_RDX);
	encoder.push(REG_R8);
	encoder.push(REG_R9);
}

void BlockCompiler::emit_restore_reg_state()
{
	encoder.pop(REG_R9);
	encoder.pop(REG_R8);
	encoder.pop(REG_RDX);
	encoder.pop(REG_RCX);
	encoder.pop(REG_RAX);
	encoder.pop(REG_RDI);
	encoder.pop(REG_RSI);
}
