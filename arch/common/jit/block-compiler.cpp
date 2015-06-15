#include <jit/block-compiler.h>
#include <set>
#include <list>
#include <map>

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
	if (!sort_ir()) return false;
	if (!analyse()) return false;
	if (!allocate()) return false;

	if (!lower()) {
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

static char insn_formats[][7] = {
	"XXXXXX",

	"NXXXXX",

	"XXXXXX",
	"XXXXXX",

	"IOXXXX",
	"XXXXXX",
	"OXXXXX",

	"IBXXXX",
	"IBXXXX",
	"IBXXXX",
	"IBXXXX",
	"IBXXXX",

	"IBXXXX",
	"IBXXXX",
	"IBXXXX",
	"IOXXXX",

	"IBXXXX",
	"IBXXXX",
	"IBXXXX",

	"INNXXX",
	"INNXXX",
	"INNXXX",
	"INNXXX",
	"INNXXX",
	"INNXXX",

	"IOXXXX",
	"IOXXXX",
	"IOXXXX",

	"IOXXXX",
	"IIXXXX",
	"IOXXXX",
	"IIXXXX",
	"IOXXXX",
	"IIXXXX",

	"NXXXXX",
	"NXXXXX",
	"INNXXX",
	"XXXXXX",

	"IXXXXX",
	"XXXXXX",
	"XXXXXX",
};

static const char *insn_mnemonics[] = {
	"<invalid>",

	"verify",

	"nop",
	"trap",

	"mov",
	"cmov",
	"ldpc",

	"add",
	"sub",
	"mul",
	"div",
	"mod",

	"shl",
	"shr",
	"sar",
	"clz",

	"and",
	"or",
	"xor",

	"cmp eq",
	"cmp ne",
	"cmp gt",
	"cmp gte",
	"cmp lt",
	"cmp lte",

	"mov sx",
	"mov zx",
	"mov trunc",

	"ldreg",
	"streg",
	"ldmem",
	"stmem",
	"ldumem",
	"stumem",

	"call",
	"jmp",
	"branch",
	"ret",

	"scm",
	"stdev",
	"lddev",
};

bool BlockCompiler::analyse()
{
	IRBlockId latest_block_id = -1;

	std::list<uint8_t> avail;
	avail.push_back(0);
	avail.push_back(1);
	avail.push_back(2);
	avail.push_back(3);
	avail.push_back(4);
	avail.push_back(5);

	std::map<IRRegId, uint32_t> allocation;

	std::set<IRRegId> live_ins, live_outs;
	for (int i = tb.ir_insn_count - 1; i >= 0; i--) {
		IRInstruction *insn = &tb.ir_insn[i];

		if (latest_block_id != insn->ir_block) {
			live_ins.clear();
			allocation.clear();

			latest_block_id = insn->ir_block;
			printf("block %d:\n", latest_block_id);
		}

		live_outs.clear();

		for (int i = 0; i < 6; i++) {
			if (insn->operands[i].type != IROperand::VREG) continue;

			IROperand *oper = &insn->operands[i];
			IRRegId reg = (IRRegId)oper->value;

			if (insn_formats[insn->type][i] == 'I') {
				// <in>
				live_ins.insert(reg);

				if (allocation.count(reg) == 0) {
					if (avail.size() == 0) assert(false);

					allocation[reg] = avail.front();
					avail.pop_front();
				}

				oper->allocate(IROperand::ALLOCATED_REG, allocation[reg]);
			} else if (insn_formats[insn->type][i] == 'O') {
				// <out>
				live_ins.erase(reg);
				live_outs.insert(reg);

				if (allocation.count(reg) == 0) assert(false);

				oper->allocate(IROperand::ALLOCATED_REG, allocation[reg]);
				avail.push_front(oper->alloc_data);
			} else if (insn_formats[insn->type][i] == 'B') {
				// <in/out>
				live_ins.insert(reg);
				live_outs.insert(reg);

				if (allocation.count(reg) == 0) {
					if (avail.size() == 0) assert(false);

					allocation[reg] = avail.front();
					avail.pop_front();
				}

				oper->allocate(IROperand::ALLOCATED_REG, allocation[reg]);
			}
		}

		printf("%03d: %03d: %s ", insn->ir_block, i, insn_mnemonics[insn->type]);

		for (int i = 0; i < 6; i++) {
			if (insn_formats[insn->type][i] != 'X') {
				if (i > 0) printf(", ");

				if (insn->operands[i].type == IROperand::VREG) {
					printf("r%d:%c%d",
						insn->operands[i].value,
						insn->operands[i].alloc_mode == IROperand::NOT_ALLOCATED ? 'N' : (insn->operands[i].alloc_mode == IROperand::ALLOCATED_REG ? 'R' : 'S'),
						insn->operands[i].alloc_data);
				} else if (insn->operands[i].type == IROperand::CONSTANT) {
					printf("$0x%x", insn->operands[i].value);
				} else if (insn->operands[i].type == IROperand::PC) {
					printf("pc");
				} else if (insn->operands[i].type == IROperand::BLOCK) {
					printf("b%d", insn->operands[i].value);
				}
			}
		}

		printf(" {");
		for (auto in : live_ins) {
			printf(" r%d", in);
		}
		printf(" } {");
		for (auto out : live_outs) {
			printf(" r%d", out);
		}
		printf(" }\n");
	}

	return true;
}

bool BlockCompiler::allocate()
{
	return true;
}

bool BlockCompiler::lower()
{
	bool success = true;
	X86Register& guest_regs_reg = REG_R15;

	std::map<uint32_t, IRBlockId> block_relocations;

	// Function prologue
	encoder.push(REG_RBP);
	encoder.mov(REG_RSP, REG_RBP);
	encoder.sub(0, REG_RSP);

	encoder.push(REG_R15);
	encoder.push(REG_R14);
	encoder.push(REG_RBX);

	encoder.mov(REG_RDI, REG_R14);
	load_state_field(8, REG_R15);

	for (int i = 0; i < tb.ir_insn_count; i++) {
		IRInstruction *insn = &tb.ir_insn[i];

		switch (insn->type) {
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

			// Create a relocated jump instruction, and store the relocation offset and
			// target into the relocations list.
			uint32_t reloc_offset;
			encoder.jmp_reloc(reloc_offset);

			block_relocations[reloc_offset] = target->value;
			break;
		}

		case IRInstruction::LDPC:
		{
			IROperand *target = &insn->operands[0];

			if (target->is_alloc_reg()) {
				encoder.mov(X86Memory::get(REG_R15, 60), register_from_operand(target));
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

		default:
			printf("unsupported instruction\n");
			success = false;
			break;
		}
	}

	asm volatile("out %0, $0xff\n" :: "a"(15), "D"(encoder.get_buffer()), "S"(encoder.get_buffer_size()), "d"(tb.block_addr));
	return success;
}
