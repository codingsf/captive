#include <jit/block-compiler.h>
#include <set>
#include <list>
#include <map>

extern "C" void cpu_set_mode(void *cpu, uint8_t mode);
extern "C" void cpu_write_device(void *cpu, uint32_t devid, uint32_t reg, uint32_t val);
extern "C" void cpu_read_device(void *cpu, uint32_t devid, uint32_t reg, uint32_t& val);
extern "C" void jit_verify(void *cpu);
extern "C" void jit_rum(void *cpu);

using namespace captive::arch::jit;
using namespace captive::arch::x86;
using namespace captive::shared;

BlockCompiler::BlockCompiler(shared::TranslationBlock& tb) : tb(tb)
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
	uint32_t max_stack = 0;

	if (!sort_ir()) return false;
	if (!analyse(max_stack)) return false;
	if (!allocate()) return false;

	if (!lower(max_stack)) {
		encoder.destroy_buffer();
		return false;
	}

	fn = (block_txln_fn)encoder.get_buffer();
	return true;
}

bool BlockCompiler::sort_ir()
{
	uint32_t pos = 1;

	while (pos < tb.ir_insn_count) {
		if (tb.ir_insn[pos].ir_block >= tb.ir_insn[pos - 1].ir_block) {
			pos += 1;
		} else {
			IRInstruction tmp = tb.ir_insn[pos];
			tb.ir_insn[pos] = tb.ir_insn[pos - 1];
			tb.ir_insn[pos - 1] = tmp;

			if (pos > 1) {
				pos -=1;
			}
		}
	}

	return true;
}

struct insn_descriptor {
	const char *mnemonic;
	const char format[7];
};

static struct insn_descriptor insn_descriptors[] = {
	{ .mnemonic = "invalid",	.format = "XXXXXX" },
	{ .mnemonic = "verify",		.format = "NXXXXX" },
	{ .mnemonic = "count",		.format = "NNXXXX" },

	{ .mnemonic = "nop",		.format = "XXXXXX" },
	{ .mnemonic = "trap",		.format = "XXXXXX" },

	{ .mnemonic = "mov",		.format = "IOXXXX" },
	{ .mnemonic = "cmov",		.format = "XXXXXX" },
	{ .mnemonic = "ldpc",		.format = "OXXXXX" },
	{ .mnemonic = "inc-pc",		.format = "IXXXXX" },

	{ .mnemonic = "add",		.format = "IBXXXX" },
	{ .mnemonic = "sub",		.format = "IBXXXX" },
	{ .mnemonic = "mul",		.format = "IBXXXX" },
	{ .mnemonic = "div",		.format = "IBXXXX" },
	{ .mnemonic = "mod",		.format = "IBXXXX" },

	{ .mnemonic = "shl",		.format = "IBXXXX" },
	{ .mnemonic = "shr",		.format = "IBXXXX" },
	{ .mnemonic = "sar",		.format = "IBXXXX" },
	{ .mnemonic = "clz",		.format = "IOXXXX" },

	{ .mnemonic = "and",		.format = "IBXXXX" },
	{ .mnemonic = "or",		.format = "IBXXXX" },
	{ .mnemonic = "xor",		.format = "IBXXXX" },

	{ .mnemonic = "cmp eq",		.format = "IIOXXX" },
	{ .mnemonic = "cmp ne",		.format = "IIOXXX" },
	{ .mnemonic = "cmp gt",		.format = "IIOXXX" },
	{ .mnemonic = "cmp gte",	.format = "IIOXXX" },
	{ .mnemonic = "cmp lt",		.format = "IIOXXX" },
	{ .mnemonic = "cmp lte",	.format = "IIOXXX" },

	{ .mnemonic = "mov sx",		.format = "IOXXXX" },
	{ .mnemonic = "mov zx",		.format = "IOXXXX" },
	{ .mnemonic = "mov trunc",	.format = "IOXXXX" },

	{ .mnemonic = "ldreg",		.format = "IOXXXX" },
	{ .mnemonic = "streg",		.format = "IIXXXX" },
	{ .mnemonic = "ldmem",		.format = "IOXXXX" },
	{ .mnemonic = "stmem",		.format = "IIXXXX" },
	{ .mnemonic = "ldumem",		.format = "IOXXXX" },
	{ .mnemonic = "stumem",		.format = "IIXXXX" },

	{ .mnemonic = "call",		.format = "NIIIII" },
	{ .mnemonic = "jmp",		.format = "NXXXXX" },
	{ .mnemonic = "branch",		.format = "INNXXX" },
	{ .mnemonic = "ret",		.format = "XXXXXX" },

	{ .mnemonic = "scm",		.format = "IXXXXX" },
	{ .mnemonic = "stdev",		.format = "IIIXXX" },
	{ .mnemonic = "lddev",		.format = "IIOXXX" },
};

bool BlockCompiler::analyse(uint32_t& max_stack)
{
	/*cfg_t block_succs, block_preds;
	block_list_t exits;

	if (!build_cfg(block_succs, block_preds, exits)) return false;*/

	IRBlockId latest_block_id = -1;

	std::list<uint32_t> avail_regs;
	std::map<IRRegId, uint32_t> allocation;
	std::set<IRRegId> live_ins, live_outs;
	std::map<IRRegId, uint32_t> global_allocation;
	uint32_t next_global = 0;

	for (int ir_idx = tb.ir_insn_count - 1; ir_idx >= 0; ir_idx--) {
		IRInstruction *insn = &tb.ir_insn[ir_idx];

		assert(insn->type < ARRAY_SIZE(insn_descriptors));
		const struct insn_descriptor *descr = &insn_descriptors[insn->type];

		if (latest_block_id != insn->ir_block) {
			if (live_ins.size() != 0) {
				for (auto in : live_ins) {
					global_allocation[in] = next_global;
					next_global += 8;
				}
			}

			live_ins.clear();
			allocation.clear();

			avail_regs.clear();
			avail_regs.push_back(0);
			avail_regs.push_back(1);
			avail_regs.push_back(2);
			avail_regs.push_back(3);
			avail_regs.push_back(4);
			avail_regs.push_back(5);


			latest_block_id = insn->ir_block;
			//printf("block %d:\n", latest_block_id);
		}

		live_outs.clear();
		for (auto in : live_ins) {
			live_outs.insert(in);
		}

		for (int o = 0; o < 6; o++) {
			if (insn->operands[o].type != IROperand::VREG) continue;

			IROperand *oper = &insn->operands[o];
			IRRegId reg = (IRRegId)oper->value;

			if (descr->format[o] == 'I') {
				// <in>
				live_ins.insert(reg);
			} else if (descr->format[o] == 'O') {
				// <out>
				live_ins.erase(reg);
			} else if (descr->format[o] == 'B') {
				// <in/out>
				live_ins.insert(reg);
			}
		}

		//printf("  [%03d] %10s", ir_idx, descr->mnemonic);

		// Release LIVE-OUTS that are not LIVE-INS
		for (auto out : live_outs) {
			if (!live_ins.count(out)) {
				auto alloc = allocation.find(out);
				assert(alloc != allocation.end());

				avail_regs.push_front(alloc->second);
			}
		}

		// Allocate LIVE-INS
		for (auto in : live_ins) {
			if (allocation.count(in) == 0) {
				assert(avail_regs.size() > 0);

				uint32_t next_reg = avail_regs.front();
				avail_regs.pop_front();

				allocation[in] = next_reg;
			}
		}

		for (int op_idx = 0; op_idx < 6; op_idx++) {
			IROperand *oper = &insn->operands[op_idx];

			if (oper->is_vreg()) {
				auto alloc_reg = allocation.find((IRRegId)oper->value);

				if (alloc_reg != allocation.end()) {
					oper->allocate(IROperand::ALLOCATED_REG, alloc_reg->second);
				}
			}
		}

		for (auto out : live_outs) {
			if (!live_ins.count(out)) {
				auto alloc = allocation.find(out);
				assert(alloc != allocation.end());

				allocation.erase(alloc);
			}
		}
		
		switch (insn->type) {
		case IRInstruction::ADD:
		case IRInstruction::SUB:
		case IRInstruction::SHL:
		case IRInstruction::SHR:
		case IRInstruction::SAR:
		case IRInstruction::OR:
			if (insn->operands[0].is_constant() && insn->operands[0].value == 0) insn->type = IRInstruction::NOP;
			break;
		}

		/*for (int op_idx = 0; op_idx < 6; op_idx++) {
			IROperand *oper = &insn->operands[op_idx];

			if (descr->format[op_idx] != 'X') {
				if (descr->format[op_idx] == 'M' && !oper->is_valid()) continue;

				if (op_idx > 0) printf(", ");

				if (oper->is_vreg()) {
					printf("i%d r%d(%c%d)",
						oper->size,
						oper->value,
						oper->alloc_mode == IROperand::NOT_ALLOCATED ? 'N' : (oper->alloc_mode == IROperand::ALLOCATED_REG ? 'R' : (oper->alloc_mode == IROperand::ALLOCATED_STACK ? 'S' : '?')),
						oper->alloc_data);
				} else if (oper->is_constant()) {
					printf("i%d $0x%x", oper->size, oper->value);
				} else if (oper->is_pc()) {
					printf("i4 pc");
				} else if (oper->is_block()) {
					printf("b%d", oper->value);
				} else if (oper->is_func()) {
					printf("&%x", oper->value);
				} else {
					printf("<invalid>");
				}
			}
		}*/

		/*printf(" {");
		for (auto in : live_ins) {
			auto alloc = allocation.find(in);
			int alloc_reg = alloc == allocation.end() ? -1 : alloc->second;

			printf(" r%d:%d", in, alloc_reg);
		}
		printf(" } {");
		for (auto out : live_outs) {
			auto alloc = allocation.find(out);
			int alloc_reg = alloc == allocation.end() ? -1 : alloc->second;

			printf(" r%d:%d", out, alloc_reg);
		}
		printf(" }\n");*/
	}

	// Allocate operands
	for (int ir_idx = 0; ir_idx < tb.ir_insn_count; ir_idx++) {
		IRInstruction *insn = &tb.ir_insn[ir_idx];

		for (int op_idx = 0; op_idx < 6; op_idx++) {
			IROperand *oper = &insn->operands[op_idx];

			if (oper->is_vreg()) {
				auto g = global_allocation.find((IRRegId)oper->value);

				if (g == global_allocation.end()) {
					// If it's not globally allocated

					if (!oper->is_allocated()) {
						// If it hasn't been allocated
						insn->type = IRInstruction::NOP;
					}
				} else {
					// If it's globally allocated
					oper->allocate(IROperand::ALLOCATED_STACK, g->second);
				}
			}
		}

next_instruction:;
	}

	//printf("block %08x\n", tb.block_addr);
	//dump_ir();

	max_stack = next_global;
	return true;
}

bool BlockCompiler::build_cfg(cfg_t& succs, cfg_t& preds, block_list_t& exits)
{
	IRBlockId current_block_id = -1;

	for (int i = 0; i < tb.ir_insn_count; i++) {
		IRInstruction *insn = &tb.ir_insn[i];

		if (insn->ir_block != current_block_id) {
			current_block_id = insn->ir_block;
			printf("building block %d\n", current_block_id);
		}

		switch (insn->type) {
		case IRInstruction::BRANCH:
		{
			assert(insn->operands[1].is_block() && insn->operands[2].is_block());

			IRBlockId s1 = (IRBlockId)insn->operands[1].value;
			IRBlockId s2 = (IRBlockId)insn->operands[2].value;

			succs[current_block_id].push_back(s1);
			succs[current_block_id].push_back(s2);

			preds[s1].push_back(current_block_id);
			preds[s2].push_back(current_block_id);
			break;
		}

		case IRInstruction::JMP:
		{
			assert(insn->operands[0].is_block());

			IRBlockId s1 = (IRBlockId)insn->operands[0].value;

			succs[current_block_id].push_back(s1);
			preds[s1].push_back(current_block_id);

			break;
		}

		case IRInstruction::RET:
		{
			exits.push_back(current_block_id);
			break;
		}

		default: continue;
		}
	}

	return true;
}

bool BlockCompiler::allocate()
{
	return true;
}

bool BlockCompiler::lower(uint32_t max_stack)
{
	bool success = true;
	X86Register& guest_regs_reg = REG_R15;

	std::map<uint32_t, IRBlockId> block_relocations;
	std::map<IRBlockId, uint32_t> native_block_offsets;

	// Function prologue
	encoder.push(REG_RBP);
	encoder.mov(REG_RSP, REG_RBP);
	
	if (max_stack > 0) encoder.sub(max_stack, REG_RSP);

	encoder.push(REG_R15);
	encoder.push(REG_R14);
	encoder.push(REG_RBX);

	encoder.mov(REG_RDI, REG_R14);
	load_state_field(8, REG_R15);

	IRBlockId current_block_id = -1;
	for (int i = 0; i < tb.ir_insn_count; i++) {
		IRInstruction *insn = &tb.ir_insn[i];

		if (current_block_id != insn->ir_block) {
			current_block_id = insn->ir_block;
			native_block_offsets[current_block_id] = encoder.current_offset();
		}

		switch (insn->type) {
		case IRInstruction::NOP:
			break;

		case IRInstruction::MOV:
		{
			IROperand *source = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			if (source->type == IROperand::VREG) {
				// mov vreg -> vreg
				if (source->is_alloc_reg()) {
					const X86Register& src_reg = register_from_operand(source);

					if (dest->is_alloc_reg()) {
						// mov reg -> reg

						if (source->alloc_data != dest->alloc_data) {
							encoder.mov(src_reg, register_from_operand(dest));
						}
					} else if (dest->is_alloc_stack()) {
						// mov reg -> stack
						encoder.mov(src_reg, stack_from_operand(dest));
					} else {
						printf("UNALLOCATED @ %d %d %d\n", i, insn->ir_block, dest->alloc_mode);
						assert(false);
					}
				} else if (source->is_alloc_stack()) {
					const X86Memory src_mem = stack_from_operand(source);

					if (dest->is_alloc_reg()) {
						// mov stack -> reg
						encoder.mov(src_mem, register_from_operand(dest));
					} else if (dest->is_alloc_stack()) {
						// mov stack -> stack

						if (source->alloc_data != dest->alloc_data) {
							X86Register& tmp = get_temp(0, source->size);
							encoder.mov(src_mem, tmp);
							encoder.mov(tmp, stack_from_operand(dest));
						}
					} else {
						assert(false);
					}
				} else {
					assert(false);
				}
			} else if (source->type == IROperand::CONSTANT) {
				// mov const -> vreg

				if (dest->is_alloc_reg()) {
					// mov imm -> reg

					if (source->value == 0) {
						encoder.xorr(register_from_operand(dest), register_from_operand(dest));
					} else {
						encoder.mov(source->value, register_from_operand(dest));
					}
				} else if (dest->is_alloc_stack()) {
					// mov imm -> stack
					switch (dest->size) {
					case 1: encoder.mov1(source->value, stack_from_operand(dest)); break;
					case 2: encoder.mov2(source->value, stack_from_operand(dest)); break;
					case 4: encoder.mov4(source->value, stack_from_operand(dest)); break;
					case 8: encoder.mov8(source->value, stack_from_operand(dest)); break;
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

		case IRInstruction::MUL:
		{
			IROperand *source = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			if (source->is_vreg()) {
				if (dest->is_vreg()) {
					if (source->is_alloc_reg() && dest->is_alloc_reg()) {
						encoder.mul(register_from_operand(source), register_from_operand(dest));
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

		case IRInstruction::MOD:
		{
			IROperand *source = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			encoder.push(REG_RAX);
			encoder.push(REG_RDX);

			encoder.xorr(REG_EDX, REG_EDX);

			encode_operand_to_reg(dest, REG_EAX);
			encode_operand_to_reg(source, REG_ECX);

			encoder.div(REG_ECX);
			encoder.mov(REG_EDX, REG_ECX);

			encoder.pop(REG_RDX);
			encoder.pop(REG_RAX);

			if (dest->is_alloc_reg()) {
				encoder.mov(REG_ECX, register_from_operand(dest, 4));
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::READ_REG:
		{
			IROperand *offset = &insn->operands[0];
			IROperand *target = &insn->operands[1];

			if (offset->type == IROperand::CONSTANT) {
				// Load a constant offset guest register into the storage location
				if (target->is_alloc_reg()) {
					encoder.mov(X86Memory::get(guest_regs_reg, offset->value), register_from_operand(target));
				} else if (target->is_alloc_stack()) {
					encoder.mov(X86Memory::get(guest_regs_reg, offset->value), get_temp(0, 4));
					encoder.mov(get_temp(0, 4), stack_from_operand(target));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::RET:
			encoder.pop(REG_RBX);
			//encoder.pop(REG_R13);
			encoder.pop(REG_R14);
			encoder.pop(REG_R15);
			encoder.xorr(REG_EAX, REG_EAX);
			encoder.leave();
			encoder.ret();
			break;

		case IRInstruction::ADD:
		case IRInstruction::SUB:
		{
			IROperand *source = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			if (source->type == IROperand::VREG) {
				if (source->is_alloc_reg() && dest->is_alloc_reg()) {
					switch (insn->type) {
					case IRInstruction::ADD:
						encoder.add(register_from_operand(source), register_from_operand(dest));
						break;
					case IRInstruction::SUB:
						encoder.sub(register_from_operand(source), register_from_operand(dest));
						break;
					}
				} else if (source->is_alloc_reg() && dest->is_alloc_stack()) {
					assert(false);
				} else if (source->is_alloc_stack() && dest->is_alloc_reg()) {
					switch (insn->type) {
					case IRInstruction::ADD:
						encoder.add(stack_from_operand(source), register_from_operand(dest));
						break;
					case IRInstruction::SUB:
						encoder.sub(stack_from_operand(source), register_from_operand(dest));
						break;
					}
				} else if (source->is_alloc_stack() && dest->is_alloc_stack()) {
					assert(false);
				} else {
					assert(false);
				}
			} else if (source->type == IROperand::PC) {
				if (dest->is_alloc_reg()) {
					// TODO: FIXME: HACK HACK HACK XXX
					switch (insn->type) {
					case IRInstruction::ADD:
						encoder.add(X86Memory::get(REG_R15, 60), register_from_operand(dest));
						break;
					case IRInstruction::SUB:
						encoder.sub(X86Memory::get(REG_R15, 60), register_from_operand(dest));
						break;
					}
				} else {
					assert(false);
				}
			} else if (source->type == IROperand::CONSTANT) {
				if (dest->is_alloc_reg()) {
					if (source->size == 8) {
						X86Register& tmp = get_temp(0, 8);

						encoder.mov(source->value, tmp);

						switch (insn->type) {
						case IRInstruction::ADD:
							encoder.add(tmp, register_from_operand(dest));
							break;
						case IRInstruction::SUB:
							encoder.sub(tmp, register_from_operand(dest));
							break;
						}
					} else {
						switch (insn->type) {
						case IRInstruction::ADD:
							encoder.add(source->value, register_from_operand(dest));
							break;
						case IRInstruction::SUB:
							encoder.sub(source->value, register_from_operand(dest));
							break;
						}
					}
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::WRITE_REG:
		{
			IROperand *offset = &insn->operands[1];
			IROperand *value = &insn->operands[0];

			if (offset->type == IROperand::CONSTANT) {
				if (value->type == IROperand::CONSTANT) {
					switch (value->size) {
					case 8:
						encoder.mov8(value->value, X86Memory::get(guest_regs_reg, offset->value));
						break;
					case 4:
						encoder.mov4(value->value, X86Memory::get(guest_regs_reg, offset->value));
						break;
					case 2:
						encoder.mov2(value->value, X86Memory::get(guest_regs_reg, offset->value));
						break;
					case 1:
						encoder.mov1(value->value, X86Memory::get(guest_regs_reg, offset->value));
						break;
					default: assert(false);
					}
				} else if (value->type == IROperand::VREG) {
					if (value->is_alloc_reg()) {
						encoder.mov(register_from_operand(value), X86Memory::get(guest_regs_reg, offset->value));
					} else if (value->is_alloc_stack()) {
						X86Register& tmp = get_temp(0, value->size);
						encoder.mov(stack_from_operand(value), tmp);
						encoder.mov(tmp, X86Memory::get(guest_regs_reg, offset->value));
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

		case IRInstruction::JMP:
		{
			IROperand *target = &insn->operands[0];
			assert(target->is_block());

			IRInstruction *next_insn = (i + 1) < tb.ir_insn_count ? &tb.ir_insn[i + 1] : NULL;

			if (next_insn && next_insn->ir_block == (IRBlockId)target->value) {
				// The next instruction is in the block we're about to jump to, so we can
				// leave this as a fallthrough.
			} else {			
				// Create a relocated jump instruction, and store the relocation offset and
				// target into the relocations list.
				uint32_t reloc_offset;
				encoder.jmp_reloc(reloc_offset);

				block_relocations[reloc_offset] = target->value;
			}
			
			break;
		}

		case IRInstruction::BRANCH:
		{
			IROperand *cond = &insn->operands[0];
			IROperand *tt = &insn->operands[1];
			IROperand *ft = &insn->operands[2];

			if (cond->is_vreg()) {
				if (cond->is_alloc_reg()) {
					encoder.test(register_from_operand(cond), register_from_operand(cond));
				} else {
					X86Register& tmp = get_temp(0, 1);

					encoder.mov(stack_from_operand(cond), tmp);
					encoder.test(tmp, tmp);
				}
			} else {
				assert(false);
			}

			{
				uint32_t reloc_offset;
				encoder.jnz_reloc(reloc_offset);
				block_relocations[reloc_offset] = tt->value;
			}

			{
				uint32_t reloc_offset;
				encoder.jmp_reloc(reloc_offset);
				block_relocations[reloc_offset] = ft->value;
			}

			break;
		}

		case IRInstruction::CALL:
		{
			IROperand *target = &insn->operands[0];

			emit_save_reg_state();

			// DI, SI, DX, CX, R8, R9
			X86Register *sysv_abi[] = { &REG_RDI, &REG_RSI, &REG_RDX, &REG_RCX, &REG_R8, &REG_R9 };

			// CPU State
			load_state_field(0, *sysv_abi[0]);

			for (int i = 1; i < 6; i++) {
				if (insn->operands[i].type != IROperand::NONE) {
					encode_operand_function_argument(&insn->operands[i], *sysv_abi[i]);
				}
			}

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov(target->value, get_temp(0, 8));
			encoder.call(get_temp(0, 8));

			emit_restore_reg_state();
			break;
		}

		case IRInstruction::LDPC:
		{
			IROperand *target = &insn->operands[0];

			if (target->is_alloc_reg()) {
				// TODO: FIXME: XXX: HACK HACK HACK
				encoder.mov(X86Memory::get(REG_R15, 60), register_from_operand(target));
			} else {
				assert(false);
			}

			break;
		}
		
		case IRInstruction::INCPC:
		{
			IROperand *amount = &insn->operands[0];

			if (amount->is_constant()) {
				// TODO: FIXME: XXX: HACK HACK HACK
				encoder.add4(amount->value, X86Memory::get(REG_R15, 60));
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::READ_MEM:
		{
			IROperand *offset = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			if (offset->is_vreg()) {
				if (offset->is_alloc_reg() && dest->is_alloc_reg()) {
					// mov (reg), reg
					encoder.mov(X86Memory::get(register_from_operand(offset)), register_from_operand(dest));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::WRITE_MEM:
		{
			IROperand *value = &insn->operands[0];
			IROperand *offset = &insn->operands[1];

			if (offset->is_vreg()) {
				if (value->is_vreg()) {
					if (offset->is_alloc_reg() && value->is_alloc_reg()) {
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

		case IRInstruction::READ_MEM_USER:
		{
			IROperand *offset = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			encoder.movcs(REG_CX);
			encoder.test(3, REG_CL);
			encoder.jnz((int8_t)2);
			encoder.intt(0x81);

			if (offset->is_vreg()) {
				if (offset->is_alloc_reg() && dest->is_alloc_reg()) {
					// mov (reg), reg
					encoder.mov(X86Memory::get(register_from_operand(offset)), register_from_operand(dest));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			encoder.test(3, REG_CL);
			encoder.jnz((int8_t)2);
			encoder.intt(0x80);
			encoder.nop();

			break;
		}

		case IRInstruction::WRITE_MEM_USER:
		{
			IROperand *value = &insn->operands[0];
			IROperand *offset = &insn->operands[1];

			encoder.movcs(REG_CX);
			encoder.test(3, REG_CL);
			encoder.jnz((int8_t)2);
			encoder.intt(0x81);

			if (offset->is_vreg()) {
				if (value->is_vreg()) {
					if (offset->is_alloc_reg() && value->is_alloc_reg()) {
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

			encoder.test(3, REG_CL);
			encoder.jnz((int8_t)2);
			encoder.intt(0x80);
			encoder.nop();

			break;
		}

		case IRInstruction::WRITE_DEVICE:
		{
			IROperand *dev = &insn->operands[0];
			IROperand *reg = &insn->operands[1];
			IROperand *val = &insn->operands[2];

			emit_save_reg_state();

			load_state_field(0, REG_RDI);

			encode_operand_function_argument(dev, REG_RSI);
			encode_operand_function_argument(reg, REG_RDX);
			encode_operand_function_argument(val, REG_RCX);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&cpu_write_device, get_temp(0, 8));
			encoder.call(get_temp(0, 8));

			emit_restore_reg_state();

			break;
		}

		case IRInstruction::READ_DEVICE:
		{
			IROperand *dev = &insn->operands[0];
			IROperand *reg = &insn->operands[1];
			IROperand *val = &insn->operands[2];

			// Allocate a slot on the stack for the reference argument
			encoder.push(0);

			// Load the address of the stack slot into RCX
			encoder.mov(REG_RSP, REG_RCX);

			emit_save_reg_state();

			load_state_field(0, REG_RDI);

			encode_operand_function_argument(dev, REG_RSI);
			encode_operand_function_argument(reg, REG_RDX);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&cpu_read_device, get_temp(0, 8));
			encoder.call(get_temp(0, 8));

			emit_restore_reg_state();

			// Pop the reference argument value into the destination register
			if (val->is_alloc_reg()) {
				encoder.pop(register_from_operand(val, 8));
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::CLZ:
		{
			IROperand *source = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			if (source->is_vreg()) {
				if (dest->is_vreg()) {
					if (source->is_alloc_reg() && dest->is_alloc_reg()) {
						encoder.bsr(register_from_operand(source), register_from_operand(dest));
						encoder.xorr(0x1f, register_from_operand(dest));
					} else if (source->is_alloc_reg() && dest->is_alloc_stack()) {
						assert(false);
					} else if (source->is_alloc_stack() && dest->is_alloc_reg()) {
						assert(false);
					} else if (source->is_alloc_stack() && dest->is_alloc_stack()) {
						assert(false);
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

		case IRInstruction::AND:
		case IRInstruction::OR:
		case IRInstruction::XOR:
		{
			IROperand *source = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			if (source->is_vreg()) {
				if (source->is_alloc_reg() && dest->is_alloc_reg()) {
					// OPER reg -> reg
					switch (insn->type) {
					case IRInstruction::AND:
						if (source->alloc_data != dest->alloc_data) {
							encoder.andd(register_from_operand(source), register_from_operand(dest));
						}

						break;
					case IRInstruction::OR:
						if (source->alloc_data != dest->alloc_data) {
							encoder.orr(register_from_operand(source), register_from_operand(dest));
						}

						break;
					case IRInstruction::XOR:
						encoder.xorr(register_from_operand(source), register_from_operand(dest));
						break;
					}
				} else if (source->is_alloc_stack() && dest->is_alloc_reg()) {
					// OPER stack -> reg
					switch (insn->type) {
					case IRInstruction::AND:
						encoder.andd(stack_from_operand(source), register_from_operand(dest));
						break;
					case IRInstruction::OR:
						encoder.orr(stack_from_operand(source), register_from_operand(dest));
						break;
					case IRInstruction::XOR:
						encoder.xorr(stack_from_operand(source), register_from_operand(dest));
						break;
					}
				} else if (source->is_alloc_reg() && dest->is_alloc_stack()) {
					// OPER reg -> stack
					switch (insn->type) {
					case IRInstruction::AND:
						encoder.andd(register_from_operand(source), stack_from_operand(dest));
						break;
					case IRInstruction::OR:
						encoder.orr(register_from_operand(source), stack_from_operand(dest));
						break;
					case IRInstruction::XOR:
						encoder.xorr(register_from_operand(source), stack_from_operand(dest));
						break;
					}
				} else if (source->is_alloc_stack() && dest->is_alloc_stack()) {
					assert(false);
				} else {
					assert(false);
				}
			} else if (source->is_constant()) {
				if (dest->is_alloc_reg()) {
					// OPER const -> reg

					if (source->size == 8) {
						X86Register& tmp = get_temp(0, 8);
						encoder.mov(source->value, tmp);

						switch (insn->type) {
						case IRInstruction::AND:
							encoder.andd(tmp, register_from_operand(dest));
							break;
						case IRInstruction::OR:
							encoder.orr(tmp, register_from_operand(dest));
							break;
						case IRInstruction::XOR:
							encoder.xorr(tmp, register_from_operand(dest));
							break;
						}
					} else {
						switch (insn->type) {
						case IRInstruction::AND:
							encoder.andd(source->value, register_from_operand(dest));
							break;
						case IRInstruction::OR:
							encoder.orr(source->value, register_from_operand(dest));
							break;
						case IRInstruction::XOR:
							encoder.xorr(source->value, register_from_operand(dest));
							break;
						}
					}
				} else if (dest->is_alloc_stack()) {
					// OPER const -> stack

					if (source->size == 8) assert(false);

					switch (insn->type) {
					case IRInstruction::AND:
						encoder.andd(source->value, stack_from_operand(dest));
						break;
					case IRInstruction::OR:
						encoder.orr(source->value, stack_from_operand(dest));
						break;
					case IRInstruction::XOR:
						encoder.xorr(source->value, stack_from_operand(dest));
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

		case IRInstruction::TRUNC:
		{
			IROperand *source = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			if (source->is_vreg()) {
				if (source->is_alloc_reg() && dest->is_alloc_reg()) {
					// trunc reg -> reg
					if (source->alloc_data != dest->alloc_data) {
						encoder.mov(register_from_operand(source), register_from_operand(dest, source->size));
					}
				} else if (source->is_alloc_reg() && dest->is_alloc_stack()) {
					// trunc reg -> stack
					encoder.mov(register_from_operand(source), stack_from_operand(dest));
				} else if (source->is_alloc_stack() && dest->is_alloc_reg()) {
					// trunc stack -> reg
					encoder.mov(stack_from_operand(source), register_from_operand(dest));
				} else if (source->is_alloc_stack() && dest->is_alloc_stack()) {
					// trunc stack -> stack
					if (source->alloc_data != dest->alloc_data) {
						encoder.mov(stack_from_operand(source), get_temp(0, source->size));
						encoder.mov(get_temp(0, dest->size), stack_from_operand(dest));
					}
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::ZX:
		case IRInstruction::SX:
		{
			IROperand *source = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			if (source->is_vreg()) {
				if (source->size == 4 && dest->size == 8) {
					if (insn->type == IRInstruction::ZX) {
						if (source->is_alloc_reg() && dest->is_alloc_reg()) {
							encoder.mov(register_from_operand(source), register_from_operand(dest, 4));
						} else if (source->is_alloc_stack() && dest->is_alloc_reg()) {
							encoder.mov(stack_from_operand(source), register_from_operand(dest, 4));
						} else if (source->is_alloc_reg() && dest->is_alloc_stack()) {
							assert(false);
						} else if (source->is_alloc_stack() && dest->is_alloc_stack()) {
							assert(false);
						} else {
							assert(false);
						}
					} else {
						if (source->is_alloc_reg() && dest->is_alloc_reg()) {
							encoder.movsx(register_from_operand(source), register_from_operand(dest));
						} else {
							assert(false);
						}
					}
				} else {
					if (source->is_alloc_reg() && dest->is_alloc_reg()) {
						switch (insn->type) {
						case IRInstruction::ZX:
							encoder.movzx(register_from_operand(source), register_from_operand(dest));
							break;

						case IRInstruction::SX:
							encoder.movsx(register_from_operand(source), register_from_operand(dest));
							break;
						}
					} else if (source->is_alloc_reg() && dest->is_alloc_stack()) {
						assert(false);
					} else if (source->is_alloc_stack() && dest->is_alloc_reg()) {
						X86Register& tmpreg = unspill_temp(source, 0);

						switch (insn->type) {
						case IRInstruction::ZX:
							encoder.movzx(tmpreg, register_from_operand(dest));
							break;

						case IRInstruction::SX:
							encoder.movsx(tmpreg, register_from_operand(dest));
							break;
						}
					} else if (source->is_alloc_stack() && dest->is_alloc_stack()) {
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

		case IRInstruction::CMPEQ:
		case IRInstruction::CMPNE:
		case IRInstruction::CMPGT:
		case IRInstruction::CMPGTE:
		case IRInstruction::CMPLT:
		case IRInstruction::CMPLTE:
		{
			const IROperand *lhs = &insn->operands[0];
			const IROperand *rhs = &insn->operands[1];
			const IROperand *dest = &insn->operands[2];

			bool invert = false;

			switch (lhs->type) {
			case IROperand::VREG:
			{
				switch (rhs->type) {
				case IROperand::VREG:
				{
					if (lhs->is_alloc_reg() && rhs->is_alloc_reg()) {
						encoder.cmp(register_from_operand(rhs), register_from_operand(lhs));
					} else if (lhs->is_alloc_stack() && rhs->is_alloc_reg()) {
						assert(false);
					} else if (lhs->is_alloc_reg() && rhs->is_alloc_stack()) {
						assert(false);
					} else if (lhs->is_alloc_stack() && rhs->is_alloc_stack()) {
						invert = true;

						X86Register& tmp = unspill_temp(lhs, 0);
						encoder.cmp(tmp, stack_from_operand(rhs));
					} else {
						assert(false);
					}

					break;
				}

				case IROperand::CONSTANT: {
					if (lhs->is_alloc_reg()) {
						encoder.cmp(rhs->value, register_from_operand(lhs));
					} else if (lhs->is_alloc_stack()) {
						switch (rhs->size) {
						case 1: encoder.cmp1(rhs->value, stack_from_operand(lhs)); break;
						case 2: encoder.cmp2(rhs->value, stack_from_operand(lhs)); break;
						case 4: encoder.cmp4(rhs->value, stack_from_operand(lhs)); break;
						case 8: encoder.cmp8(rhs->value, stack_from_operand(lhs)); break;
						default: assert(false);
						}
					} else {
						assert(false);
					}

					break;
				}

				default: assert(false);
				}

				break;
			}

			case IROperand::CONSTANT:
			{
				invert = true;

				switch (rhs->type) {
				case IROperand::VREG:
				{
					if (rhs->is_alloc_reg()) {
						encoder.cmp(lhs->value, register_from_operand(rhs));
					} else if (rhs->is_alloc_stack()) {
						switch (lhs->size) {
						case 1: encoder.cmp1(lhs->value, stack_from_operand(rhs)); break;
						case 2: encoder.cmp2(lhs->value, stack_from_operand(rhs)); break;
						case 4: encoder.cmp4(lhs->value, stack_from_operand(rhs)); break;
						case 8: encoder.cmp8(lhs->value, stack_from_operand(rhs)); break;
						}
					} else {
						assert(false);
					}

					break;
				}
				default: assert(false);
				}

				break;
			}

			default: assert(false);
			}

			switch (insn->type) {
			case IRInstruction::CMPEQ:
				encoder.sete(register_from_operand(dest));
				break;

			case IRInstruction::CMPNE:
				encoder.setne(register_from_operand(dest));
				break;

			case IRInstruction::CMPLT:
				if (invert)
					encoder.setnb(register_from_operand(dest));
				else
					encoder.setb(register_from_operand(dest));
				break;

			case IRInstruction::CMPLTE:
				if (invert)
					encoder.setnbe(register_from_operand(dest));
				else
					encoder.setbe(register_from_operand(dest));
				break;

			case IRInstruction::CMPGT:
				if (invert)
					encoder.setna(register_from_operand(dest));
				else
					encoder.seta(register_from_operand(dest));
				break;

			case IRInstruction::CMPGTE:
				if (invert)
					encoder.setnae(register_from_operand(dest));
				else
					encoder.setae(register_from_operand(dest));
				break;

			default:
				assert(false);
			}

			break;
		}

		case IRInstruction::SET_CPU_MODE:
		{
			IROperand *mode = &insn->operands[0];

			emit_save_reg_state();

			load_state_field(0, REG_RDI);

			encode_operand_function_argument(mode, REG_RSI);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&cpu_set_mode, get_temp(0, 8));
			encoder.call(get_temp(0, 8));

			emit_restore_reg_state();

			break;
		}

		case IRInstruction::SHL:
		case IRInstruction::SHR:
		case IRInstruction::SAR:
		{
			IROperand *amount = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			if (amount->is_constant()) {
				if (dest->is_alloc_reg()) {
					const X86Register& operand = register_from_operand(dest);

					switch (insn->type) {
					case IRInstruction::SHL:
						encoder.shl(amount->value, operand);
						break;
					case IRInstruction::SHR:
						encoder.shr(amount->value, operand);
						break;
					case IRInstruction::SAR:
						encoder.sar(amount->value, operand);
						break;
					}
				} else {
					assert(false);
				}
			} else if (amount->is_vreg()) {
				if (amount->is_alloc_reg()) {
					if (dest->is_alloc_reg()) {
						const X86Register& operand = register_from_operand(dest);

						encoder.mov(register_from_operand(amount, 1), REG_CL);

						switch (insn->type) {
						case IRInstruction::SHL:
							encoder.shl(REG_CL, operand);
							break;
						case IRInstruction::SHR:
							encoder.shr(REG_CL, operand);
							break;
						case IRInstruction::SAR:
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

		case IRInstruction::VERIFY:
		{
			encoder.nop(X86Memory::get(REG_RAX, insn->operands[0].value));

			emit_save_reg_state();

			load_state_field(0, REG_RDI);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&jit_verify, get_temp(0, 8));
			encoder.call(get_temp(0, 8));

			emit_restore_reg_state();

			break;
		}
		
		case IRInstruction::COUNT:
		{
			break;
		}

		default:
			printf("unsupported instruction: %s\n", insn_descriptors[insn->type].mnemonic);
			success = false;
			break;
		}
	}

	for (auto reloc : block_relocations) {
		int32_t value = native_block_offsets[reloc.second];
		value -= reloc.first;
		value -= 4;

		uint32_t *slot = (uint32_t *)&encoder.get_buffer()[reloc.first];
		*slot = value;
	}

	//asm volatile("out %0, $0xff\n" :: "a"(15), "D"(encoder.get_buffer()), "S"(encoder.get_buffer_size()), "d"(tb.block_addr));
	return success;
}

void BlockCompiler::emit_save_reg_state()
{
	encoder.push(REG_RSI);
	encoder.push(REG_RDI);
	encoder.push(REG_RAX);
	encoder.push(REG_RDX);
	encoder.push(REG_R8);
	encoder.push(REG_R9);
	encoder.push(REG_R10);
}

void BlockCompiler::emit_restore_reg_state()
{
	encoder.pop(REG_R10);
	encoder.pop(REG_R9);
	encoder.pop(REG_R8);
	encoder.pop(REG_RDX);
	encoder.pop(REG_RAX);
	encoder.pop(REG_RDI);
	encoder.pop(REG_RSI);
}

void BlockCompiler::encode_operand_function_argument(IROperand *oper, const X86Register& target_reg)
{
	if (oper->is_constant() == IROperand::CONSTANT) {
		encoder.mov(oper->value, target_reg);
	} else if (oper->is_vreg()) {
		if (oper->is_alloc_reg()) {
			switch (oper->alloc_data) {
			case 0: encoder.mov(X86Memory::get(REG_RSP, 4*8), target_reg); break;		// A
			case 1: encoder.mov(X86Memory::get(REG_RSP, 3*8), target_reg); break;		// D
			case 2: encoder.mov(X86Memory::get(REG_RSP, 6*8), target_reg); break;		// SI
			case 3: encoder.mov(X86Memory::get(REG_RSP, 5*8), target_reg); break;		// DI
			case 4: encoder.mov(X86Memory::get(REG_RSP, 2*8), target_reg); break;		// R8
			case 5: encoder.mov(X86Memory::get(REG_RSP, 1*8), target_reg); break;		// R9
			case 6: encoder.mov(X86Memory::get(REG_RSP, 0*8), target_reg); break;		// R10
			default: assert(false);
			}
		} else {
			assert(false);
		}
	} else {
		assert(false);
	}
}

void BlockCompiler::dump_ir()
{
	IRBlockId current_block_id = -1;

	for (int ir_idx = 0; ir_idx < tb.ir_insn_count; ir_idx++) {
		IRInstruction *insn = &tb.ir_insn[ir_idx];

		assert(insn->type < ARRAY_SIZE(insn_descriptors));
		const struct insn_descriptor *descr = &insn_descriptors[insn->type];

		if (current_block_id != insn->ir_block) {
			current_block_id = insn->ir_block;
			printf("block %d:\n", current_block_id);
		}

		printf("  %12s", descr->mnemonic);

		for (int op_idx = 0; op_idx < 6; op_idx++) {
			IROperand *oper = &insn->operands[op_idx];

			if (descr->format[op_idx] != 'X') {
				if (descr->format[op_idx] == 'M' && !oper->is_valid()) continue;

				if (op_idx > 0) printf(", ");

				if (oper->is_vreg()) {
					printf("i%d r%d(%c%d)",
						oper->size,
						oper->value,
						oper->alloc_mode == IROperand::NOT_ALLOCATED ? 'N' : (oper->alloc_mode == IROperand::ALLOCATED_REG ? 'R' : (oper->alloc_mode == IROperand::ALLOCATED_STACK ? 'S' : '?')),
						oper->alloc_data);
				} else if (oper->is_constant()) {
					printf("i%d $0x%x", oper->size, oper->value);
				} else if (oper->is_pc()) {
					printf("i4 pc (%08x)", oper->value);
				} else if (oper->is_block()) {
					printf("b%d", oper->value);
				} else if (oper->is_func()) {
					printf("&%x", oper->value);
				} else {
					printf("<invalid>");
				}
			}
		}

		printf("\n");
	}
}

void BlockCompiler::encode_operand_to_reg(shared::IROperand *operand, const x86::X86Register& reg)
{
	switch (operand->type) {
	case IROperand::CONSTANT:
	{
		if (operand->value == 0) {
			encoder.xorr(reg, reg);
		} else {
			encoder.mov(operand->value, reg);
		}

		break;
	}

	case IROperand::VREG:
	{
		switch (operand->alloc_mode) {
		case IROperand::ALLOCATED_STACK:
			encoder.mov(stack_from_operand(operand), reg);
			break;

		case IROperand::ALLOCATED_REG:
			encoder.mov(register_from_operand(operand, reg.size), reg);
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