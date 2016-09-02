#include <jit/block-compiler.h>
#include <jit/ir-sorter.h>

#include <profile/region.h>

#include <algorithm>
#include <set>
#include <list>
#include <map>
#include <queue>

#include <small-set.h>
#include <maybe-set.h>
#include <tick-timer.h>

#include <shmem.h>

#define NOP_BLOCK (IRBlockId)0x7fffffff

#define SYSCALL_CALL_GATE
//#define TIMER
//#define BLOCK_ENTRY_TRACKING

extern "C" void cpu_set_mode(void *cpu, uint8_t mode);
extern "C" void cpu_write_device(void *cpu, uint32_t devid, uint32_t reg, uint32_t val);
extern "C" void cpu_read_device(void *cpu, uint32_t devid, uint32_t reg, uint32_t& val);
extern "C" void jit_rum(void *cpu);
extern "C" void jit_trace(void *cpu, uint8_t opcode, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4);

extern uint32_t interpret_ir(void *, void *, uint32_t);

using namespace captive::arch::jit;
using namespace captive::arch::jit::algo;
using namespace captive::arch::x86;
using namespace captive::shared;

static void dump_insn(IRInstruction *insn);

#define REG_OFFSET_OF(__reg_name) ((uint64_t)tagged_regs.__reg_name - (uint64_t)tagged_regs.base)

/* Register Mapping
 *
 * RAX  Allocatable			0
 * RBX  Allocatable			1
 * RCX  Temporary			t0
 * RDX  Allocatable			2
 * RSI  Allocatable			3
 * RDI  Allocatable
 * RBP	Register File
 * R8   Allocatable			4
 * R9   Allocatable			5
 * R10  Allocatable			6
 * R11  Allocatable			7
 * R12  Allocatable			8
 * R13  Frame Pointer
 * R14  Temporary			t1
 * R15  PC
 * FS	Base Pointer to JIT STATE structure
 * GS	Base Pointer to emulated VMEM
 */

BlockCompiler::BlockCompiler(TranslationContext& ctx, malloc::Allocator& allocator, profile::Region *rgn, uint8_t isa_mode, gpa_t pa, const CPU::TaggedRegisters& tagged_regs, uint64_t sim_events)
: ctx(ctx),
encoder(allocator),
region(rgn),
isa_mode(isa_mode),
pa(pa),
tagged_regs(tagged_regs),
sim_events(sim_events) {
	int i = 0;
	assign(i++, REG_RAX, REG_EAX, REG_AX, REG_AL);
	assign(i++, REG_RDX, REG_EDX, REG_DX, REG_DL);
	assign(i++, REG_RSI, REG_ESI, REG_SI, REG_SIL);
	assign(i++, REG_RDI, REG_EDI, REG_DI, REG_DIL);
	assign(i++, REG_RBX, REG_EBX, REG_BX, REG_BL);
	assign(i++, REG_R8, REG_R8D, REG_R8W, REG_R8B);
	assign(i++, REG_R9, REG_R9D, REG_R9W, REG_R9B);
	assign(i++, REG_R10, REG_R10D, REG_R10W, REG_R10B);
	assign(i++, REG_R11, REG_R11D, REG_R11W, REG_R11B);
	assign(i++, REG_R12, REG_R12D, REG_R12W, REG_R12B);
}

//#define VERIFY_IR

bool BlockCompiler::compile(block_txln_fn& fn) {
	uint32_t max_stack = 0;

	tick_timer<false> timer;
	timer.reset();

#ifdef VERIFY_IR
	if (!verify()) {
		printf("**** FAILED IR BEFORE OPTIMISATION\n");
		dump_ir();
		return false;
	}
#endif

	if (!reorder_blocks()) return false;
	timer.tick("block-reorder");

	if (!thread_jumps()) return false;
	timer.tick("jump-threading");

	if (!dbe()) return false;
	timer.tick("dead-block-elimination");

	sort_ir();

	if (!merge_blocks()) return false;
	timer.tick("block-merging");

	if (!constant_prop()) return false;
	timer.tick("constant-propagation");

	if (!peephole()) return false;
	timer.tick("peepholer");

	//if (!value_merging()) return false;

	timer.tick("value-merging");

	sort_ir();
	if (!reg_value_reuse()) return false;

	timer.tick("reg-val-reuse");

	if (!sort_ir()) return false;
	timer.tick("sort");

#ifdef VERIFY_IR
	if (!verify()) {
		printf("**** FAILED IR BEFORE ALLOCATION\n");
		dump_ir();
		return false;
	}
#endif

	if (!analyse(max_stack)) return false;
	timer.tick("reg-allocation");

#ifndef VERIFY_IR
	if (!post_allocate_peephole()) return false;
	timer.tick("post-alloc-peepholer");
#endif

	if (!lower_stack_to_reg()) return false;
	timer.tick("mem-2-reg");

	sort_ir();

#ifdef VERIFY_IR
	if (!verify()) {
		printf("**** FAILED IR BEFORE LOWERING\n");
		dump_ir();
		return false;
	}
#endif

	if (!lower(max_stack)) {
		encoder.destroy_buffer();
		return false;
	}

	/*if (!peeplower(max_stack)) {
		encoder.destroy_buffer();
		return false;
	}*/

	/*if (!lower_to_interpreter()) {
		encoder.destroy_buffer();
		return false;
	}*/

	timer.tick("lower");
	timer.dump("compile ");

	encoder.finalise();

	fn = (block_txln_fn) encoder.get_buffer();
	return true;
}

bool BlockCompiler::sort_ir() {
	bool not_sorted = false;
	IRBlockId id = 0;
	for (unsigned int i = 0; i < ctx.count(); ++i) {
		IRInstruction *insn = ctx.at(i);
		if (insn->ir_block < id) {
			not_sorted = true;
			break;
		}
		id = insn->ir_block;
	}
	if (!not_sorted) return true;

	MergeSort sorter(ctx);
	sorter.perform_sort();

	return true;
}

// If this is an add of a negative value, replace it with a subtract of a positive value

static bool peephole_add(IRInstruction &insn) {
	IROperand &op1 = insn.operands[0];
	if (op1.is_constant()) {
		uint64_t unsigned_negative = 0;
		switch (op1.size) {
		case 1: unsigned_negative = 0x80;
			break;
		case 2: unsigned_negative = 0x8000;
			break;
		case 4: unsigned_negative = 0x80000000;
			break;
		case 8: unsigned_negative = 0x8000000000000000;
			break;
		default: assert(false);
		}

		if (op1.value >= unsigned_negative) {
			insn.type = IRInstruction::SUB;
			switch (op1.size) {
			case 1: op1.value = -(int8_t) op1.value;
				break;
			case 2: op1.value = -(int16_t) op1.value;
				break;
			case 4: op1.value = -(int32_t) op1.value;
				break;
			case 8: op1.value = -(int64_t) op1.value;
				break;
			default: assert(false);
			}
		}
	}
	return true;
}

// If we shift an n-bit value by n bits, then replace this instruction with a move of 0.
// We explicitly move in 0, rather than XORing out, in order to avoid creating spurious use-defs

bool peephole_shift(IRInstruction &insn) {
	IROperand &op1 = insn.operands[0];
	IROperand &op2 = insn.operands[1];
	if (op1.is_constant()) {
		bool zeros_out = false;
		switch (op2.size) {
		case 1: zeros_out = op1.value == 8;
			break;
		case 2: zeros_out = op1.value == 16;
			break;
		case 4: zeros_out = op1.value == 32;
			break;
		case 8: zeros_out = op1.value == 64;
			break;
		}

		if (zeros_out) {
			insn.type = IRInstruction::MOV;
			op1.type = IROperand::CONSTANT;
			op1.value = 0;
		}
	}

	return true;
}

static void make_instruction_nop(IRInstruction *insn, bool set_block) {
	insn->type = IRInstruction::NOP;
	insn->operands[0].type = IROperand::NONE;
	insn->operands[1].type = IROperand::NONE;
	insn->operands[2].type = IROperand::NONE;
	insn->operands[3].type = IROperand::NONE;
	insn->operands[4].type = IROperand::NONE;
	insn->operands[5].type = IROperand::NONE;
	if (set_block) insn->ir_block = NOP_BLOCK;
}

static void peephole_mov(IRInstruction *insn, IRInstruction *next) {
	/*if (next->type == IRInstruction::OR) {
		if (insn->operands[0].type == IROperand::CONSTANT && insn->operands[0].value == 0) {
			printf("got a candidate\n", insn->type, next->type);
		}
	}*/
}

// Perform some basic optimisations

bool BlockCompiler::peephole() {
	IRInstruction *prev_pc_inc = NULL;
	IRBlockId prev_block = INVALID_BLOCK_ID;

	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);

		if (insn->ir_block == NOP_BLOCK) continue;

		if (insn->ir_block != prev_block) {
			prev_pc_inc = NULL;
			prev_block = insn->ir_block;
		}

		switch (insn->type) {
		case IRInstruction::ADD:
			if (insn->operands[0].is_constant() && insn->operands[0].value == 0) {
				make_instruction_nop(insn, true);
			} else {
				peephole_add(*insn);
			}

			break;

		case IRInstruction::SHR:
		case IRInstruction::SHL:
			if (insn->operands[0].is_constant() && insn->operands[0].value == 0) {
				make_instruction_nop(insn, true);
			} else {
				peephole_shift(*insn);
			}
			break;

		case IRInstruction::SUB:
		case IRInstruction::SAR:
		case IRInstruction::ROR:
		case IRInstruction::OR:
		case IRInstruction::XOR:
			if (insn->operands[0].is_constant() && insn->operands[0].value == 0) {
				make_instruction_nop(insn, true);
			}
			break;


		case IRInstruction::INCPC:
			if (0) {//prev_pc_inc) {
				insn->operands[0].value += prev_pc_inc->operands[0].value;
				prev_pc_inc->type = IRInstruction::NOP;
			}
			prev_pc_inc = insn;

			break;

		case IRInstruction::READ_REG:
			if (insn->operands[0].value == REG_OFFSET_OF(PC)) {
				prev_pc_inc = NULL;
			}
			break;

		case IRInstruction::RET:
		case IRInstruction::DISPATCH:
		case IRInstruction::READ_MEM:
		case IRInstruction::WRITE_MEM:
		case IRInstruction::READ_MEM_USER:
		case IRInstruction::WRITE_MEM_USER:
		case IRInstruction::LDPC:
		case IRInstruction::READ_DEVICE:
		case IRInstruction::WRITE_DEVICE:
			prev_pc_inc = NULL;
			break;

		case IRInstruction::MOV:
			if (ir_idx < ctx.count() - 1) {
				peephole_mov(insn, ctx.at(ir_idx + 1));
			}

			break;

		default:
			break;
		}

	}

	return true;
}

static bool is_mov_nop(IRInstruction *insn, IRInstruction *next, bool set_block) {
	if (insn->type == IRInstruction::MOV) {
		if ((insn->operands[0].alloc_mode == insn->operands[1].alloc_mode) && (insn->operands[0].alloc_data == insn->operands[1].alloc_data)) {
			make_instruction_nop(insn, set_block);
			return true;
		}

		if (next->type == IRInstruction::MOV) {
			if (insn->operands[0].alloc_mode == next->operands[1].alloc_mode &&
					insn->operands[0].alloc_data == next->operands[1].alloc_data &&
					insn->operands[1].alloc_mode == next->operands[0].alloc_mode &&
					insn->operands[1].alloc_data == next->operands[0].alloc_data) {

				make_instruction_nop(next, set_block);
				return false;
			}
		}

		return false;
	} else {
		return false;
	}
}

// We can merge the instruction if it is an add or subtract of a constant value into a register

static bool is_add_candidate(IRInstruction *insn) {
	return ((insn->type == IRInstruction::ADD) || (insn->type == IRInstruction::SUB))
			&& insn->operands[0].is_constant() && insn->operands[1].is_alloc_reg();
}

// We can merge the instructions if the mem instruction is a load, from the add's target, into the add's target

static bool is_mem_candidate(IRInstruction *mem, IRInstruction *add) {
	if (mem->type != IRInstruction::READ_MEM) return false;

	IROperand *add_target = &add->operands[1];

	// We should only be here if the add is a candidate
	assert(add_target->is_alloc_reg());

	IROperand *mem_source = &mem->operands[0];
	IROperand *mem_target = &mem->operands[2];

	if (!mem_source->is_alloc_reg() || !mem_target->is_alloc_reg()) return false;
	if (mem_source->alloc_data != mem_target->alloc_data) return false;
	if (mem_source->alloc_data != add_target->alloc_data) return false;

	return true;
}

static bool is_breaker(IRInstruction *add, IRInstruction *test) {
	assert(add->type == IRInstruction::ADD || add->type == IRInstruction::SUB);

	if (test->ir_block == NOP_BLOCK) return false;

	if ((add->ir_block != test->ir_block) && (test->ir_block != NOP_BLOCK)) {
		return true;
	}

	// If the instruction under test touches the target of the add, then it is a breaker
	if (test->type != IRInstruction::READ_MEM) {
		IROperand *add_target = &add->operands[1];
		for (int op_idx = 0; op_idx < 6; ++op_idx) {
			if ((test->operands[op_idx].alloc_mode == add_target->alloc_mode) && (test->operands[op_idx].alloc_data == add_target->alloc_data)) return true;
		}
	}
	return false;
}

bool BlockCompiler::post_allocate_peephole() {
	for (uint32_t ir_idx = 0; ir_idx < ctx.count() - 1; ++ir_idx) {
		is_mov_nop(ctx.at(ir_idx), ctx.at(ir_idx + 1), true);
	}

	uint32_t add_insn_idx = 0;
	uint32_t mem_insn_idx = 0;
	IRInstruction *add_insn, *mem_insn;

	//~ printf("*****\n");
	//~ dump_ir();

	while (true) {
		// get the next add instruction
		do {
			if (add_insn_idx >= ctx.count()) goto exit;
			add_insn = ctx.at(add_insn_idx++);
		} while (!is_add_candidate(add_insn));

		//~ printf("Considering add=%u\n", add_insn_idx-1);

		mem_insn_idx = add_insn_idx;
		bool broken = false;
		do {
			if (mem_insn_idx >= ctx.count()) goto exit;
			mem_insn = ctx.at(mem_insn_idx++);
			if (is_breaker(add_insn, mem_insn)) {
				//~ printf("Broken by %u\n", mem_insn_idx);
				broken = true;
				break;
			}

		} while (!is_mem_candidate(mem_insn, add_insn));

		if (broken) continue;

		//~ printf("Considering mem=%u\n", mem_insn_idx-1);

		assert(mem_insn->operands[1].is_constant());
		if (add_insn->type == IRInstruction::ADD)
			mem_insn->operands[1].value += add_insn->operands[0].value;
		else
			mem_insn->operands[1].value -= add_insn->operands[0].value;

		make_instruction_nop(add_insn, true);
	}
exit:
	return true;
}

struct insn_descriptor {
	const char *mnemonic;
	const char format[7];
	bool has_side_effects;
};

/**
 * Descriptor codes:
 *
 * X - No Operand
 * N - No Direction (e.g. constant only)
 * B - Input & Output
 * I - Input
 * O - Output
 */
static struct insn_descriptor insn_descriptors[] = {
	{ .mnemonic = "invalid", .format = "XXXXXX", .has_side_effects = false},
	{ .mnemonic = "verify", .format = "NXXXXX", .has_side_effects = true},
	{ .mnemonic = "count", .format = "NNXXXX", .has_side_effects = true},
	{ .mnemonic = "fetch", .format = "NXXXXX", .has_side_effects = true},

	{ .mnemonic = "nop", .format = "XXXXXX", .has_side_effects = false},
	{ .mnemonic = "trap", .format = "XXXXXX", .has_side_effects = true},

	{ .mnemonic = "mov", .format = "IOXXXX", .has_side_effects = false},
	{ .mnemonic = "cmov", .format = "XXXXXX", .has_side_effects = false},
	{ .mnemonic = "ldpc", .format = "OXXXXX", .has_side_effects = false},
	{ .mnemonic = "stpc", .format = "IXXXXX", .has_side_effects = true},
	{ .mnemonic = "inc-pc", .format = "IXXXXX", .has_side_effects = true},

	{ .mnemonic = "vector insert", .format = "IIIXXX", .has_side_effects = false},
	{ .mnemonic = "vector extract", .format = "IIOXXX", .has_side_effects = false},

	{ .mnemonic = "add", .format = "IBXXXX", .has_side_effects = false},
	{ .mnemonic = "adc", .format = "IBIXXX", .has_side_effects = false},
	{ .mnemonic = "sub", .format = "IBXXXX", .has_side_effects = false},
	{ .mnemonic = "sbc", .format = "IBIXXX", .has_side_effects = false},
	{ .mnemonic = "mul", .format = "IBXXXX", .has_side_effects = false},
	{ .mnemonic = "div", .format = "IBXXXX", .has_side_effects = false},
	{ .mnemonic = "mod", .format = "IBXXXX", .has_side_effects = false},

	{ .mnemonic = "abs", .format = "BXXXXX", .has_side_effects = false},
	{ .mnemonic = "neg", .format = "BXXXXX", .has_side_effects = false},
	{ .mnemonic = "sqrt", .format = "BXXXXX", .has_side_effects = false},
	{ .mnemonic = "is qnan", .format = "IOXXXX", .has_side_effects = false},
	{ .mnemonic = "is snan", .format = "IOXXXX", .has_side_effects = false},

	{ .mnemonic = "shl", .format = "IBXXXX", .has_side_effects = false},
	{ .mnemonic = "shr", .format = "IBXXXX", .has_side_effects = false},
	{ .mnemonic = "sar", .format = "IBXXXX", .has_side_effects = false},
	{ .mnemonic = "ror", .format = "IBXXXX", .has_side_effects = false},
	{ .mnemonic = "clz", .format = "IOXXXX", .has_side_effects = false},

	{ .mnemonic = "not", .format = "BXXXXX", .has_side_effects = false},
	{ .mnemonic = "and", .format = "IBXXXX", .has_side_effects = false},
	{ .mnemonic = "or", .format = "IBXXXX", .has_side_effects = false},
	{ .mnemonic = "xor", .format = "IBXXXX", .has_side_effects = false},

	{ .mnemonic = "cmp eq", .format = "IIOXXX", .has_side_effects = false},
	{ .mnemonic = "cmp ne", .format = "IIOXXX", .has_side_effects = false},
	{ .mnemonic = "cmp gt", .format = "IIOXXX", .has_side_effects = false},
	{ .mnemonic = "cmp gte", .format = "IIOXXX", .has_side_effects = false},
	{ .mnemonic = "cmp lt", .format = "IIOXXX", .has_side_effects = false},
	{ .mnemonic = "cmp lte", .format = "IIOXXX", .has_side_effects = false},

	{ .mnemonic = "mov sx", .format = "IOXXXX", .has_side_effects = false},
	{ .mnemonic = "mov zx", .format = "IOXXXX", .has_side_effects = false},
	{ .mnemonic = "mov trunc", .format = "IOXXXX", .has_side_effects = false},

	{ .mnemonic = "ldreg", .format = "IOXXXX", .has_side_effects = false},
	{ .mnemonic = "streg", .format = "IIXXXX", .has_side_effects = true},
	{ .mnemonic = "ldmem", .format = "IIOXXX", .has_side_effects = true},
	{ .mnemonic = "stmem", .format = "IIIXXX", .has_side_effects = true},
	{ .mnemonic = "ldumem", .format = "IOXXXX", .has_side_effects = true},
	{ .mnemonic = "stumem", .format = "IIXXXX", .has_side_effects = true},

	{ .mnemonic = "atomic-write", .format = "IBXXXX", .has_side_effects = true},

	{ .mnemonic = "call", .format = "NIIIII", .has_side_effects = true},
	{ .mnemonic = "jmp", .format = "NXXXXX", .has_side_effects = true},
	{ .mnemonic = "branch", .format = "INNXXX", .has_side_effects = true},
	{ .mnemonic = "ret", .format = "XXXXXX", .has_side_effects = true},
	{ .mnemonic = "loop", .format = "XXXXXX", .has_side_effects = true},
	{ .mnemonic = "dispatch", .format = "NNNNXX", .has_side_effects = true},

	{ .mnemonic = "scm", .format = "IXXXXX", .has_side_effects = true},
	{ .mnemonic = "stdev", .format = "IIIXXX", .has_side_effects = true},
	{ .mnemonic = "lddev", .format = "IIOXXX", .has_side_effects = false},

	{ .mnemonic = "trigger irq", .format = "XXXXXX", .has_side_effects = true}, // TODO: Does it have side effects?

	{ .mnemonic = "flush", .format = "XXXXXX", .has_side_effects = true},
	{ .mnemonic = "flush itlb", .format = "XXXXXX", .has_side_effects = true},
	{ .mnemonic = "flush dtlb", .format = "XXXXXX", .has_side_effects = true},
	{ .mnemonic = "flush itlb", .format = "IXXXXX", .has_side_effects = true},
	{ .mnemonic = "flush dtlb", .format = "IXXXXX", .has_side_effects = true},
	{ .mnemonic = "flush ctxid", .format = "IXXXXX", .has_side_effects = true},
	{ .mnemonic = "invd i$", .format = "XXXXXX", .has_side_effects = true},
	{ .mnemonic = "invd i$", .format = "IXXXXX", .has_side_effects = true},
	{ .mnemonic = "set ctxid", .format = "IXXXXX", .has_side_effects = true},
	{ .mnemonic = "pgt change", .format = "XXXXXX", .has_side_effects = true},

	{ .mnemonic = "chmd kernel", .format = "XXXXXX", .has_side_effects = true},
	{ .mnemonic = "chmd user", .format = "XXXXXX", .has_side_effects = true},

	{ .mnemonic = "adc flags", .format = "IBIXXX", .has_side_effects = true},
	{ .mnemonic = "sbc flags", .format = "IBIXXX", .has_side_effects = true},

	{ .mnemonic = "set flags zn", .format = "IXXXXX", .has_side_effects = true},

	{ .mnemonic = "barrier", .format = "NNXXXX", .has_side_effects = true},
	{ .mnemonic = "trace", .format = "NIIIII", .has_side_effects = true},
};

bool BlockCompiler::analyse(uint32_t& max_stack) {
	tick_timer<false> timer;
	timer.reset();

	IRBlockId latest_block_id = INVALID_BLOCK_ID;

	used_phys_regs.clear();

	std::vector<int32_t> allocation(ctx.reg_count());
	maybe_map<IRRegId, uint32_t, 128> global_allocation(ctx.reg_count()); // global register allocation

	typedef std::set<IRRegId> live_set_t;
	live_set_t live_ins, live_outs;
	std::vector<live_set_t::iterator> to_erase;

	PopulatedSet<9> avail_regs; // Register indicies that are available for allocation.
	uint32_t next_global = 0; // Next stack location for globally allocated register.

	std::vector<int32_t> vreg_seen_block(ctx.reg_count(), -1);

	timer.tick("Init");

	// Build up a map of which vregs have been seen in which blocks, to detect spanning vregs.
	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);

		if (insn->ir_block == NOP_BLOCK) continue;
		if (insn->type == IRInstruction::BARRIER) next_global = 0;

		for (int op_idx = 0; op_idx < 6; op_idx++) {
			IROperand *oper = &insn->operands[op_idx];
			if (!oper->is_valid()) break;

			// If the operand is a vreg, and is not already a global...
			if (oper->is_vreg() && (global_allocation.count(oper->value) == 0)) {
				auto seen_in_block = vreg_seen_block[oper->value];

				// If we have already seen this operand, and not in the same block, then we
				// must globally allocate it.
				if (seen_in_block != -1 && seen_in_block != (int32_t) insn->ir_block) {
					global_allocation[oper->value] = next_global;
					next_global += 8;
					if (next_global > max_stack) max_stack = next_global;
				}

				vreg_seen_block[oper->value] = insn->ir_block;
			}
		}
	}

	timer.tick("Globals");

	for (int ir_idx = ctx.count() - 1; ir_idx >= 0; ir_idx--) {
		// Grab a pointer to the instruction we're looking at.
		IRInstruction *insn = ctx.at(ir_idx);
		if (insn->ir_block == NOP_BLOCK) continue;

		// Make sure it has a descriptor.
		assert(insn->type < ARRAY_SIZE(insn_descriptors));
		const struct insn_descriptor *descr = &insn_descriptors[insn->type];

		// Detect a change in block
		if (latest_block_id != insn->ir_block) {
			// Clear the live-in working set and current allocations.
			live_ins.clear();
			allocation.assign(ctx.reg_count(), -1);

			// Reset the available register bitfield
			avail_regs.fill(0x1ff);

			// Update the latest block id.
			latest_block_id = insn->ir_block;
		}

		if (insn->type == IRInstruction::BARRIER) {
			next_global = 0;
		}

		// Clear the live-out set, and make every current live-in a live-out.
		live_outs = live_ins;

		// Loop over the VREG operands and update the live-in set accordingly.
		for (int o = 0; o < 6; o++) {
			if (!insn->operands[o].is_valid()) break;
			if (insn->operands[o].type != IROperand::VREG) continue;

			IROperand *oper = &insn->operands[o];
			IRRegId reg = (IRRegId) oper->value;

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

		// Release LIVE-OUTs that are not LIVE-INs
		for (auto out : live_outs) {
			if (global_allocation.count(out)) continue;
			if (!live_ins.count(out)) {
				assert(allocation[out] != -1);

				// Make the released register available again.
				avail_regs.set(allocation[out]);
			}
		}

		// Allocate LIVE-INs
		for (auto in : live_ins) {
			// If the live-in is not already allocated, allocate it.
			if (allocation[in] == -1 && global_allocation.count(in) == 0) {
				int32_t next_reg = avail_regs.next_avail();

				if (next_reg == -1) {
					global_allocation[in] = next_global;
					next_global += 8;
					if (max_stack < next_global) max_stack = next_global;

				} else {
					allocation[in] = next_reg;

					avail_regs.clear(next_reg);
					used_phys_regs.set(next_reg);
				}
			}
		}

		// If an instruction has no output or both operands which are live outs then the instruction is dead
		bool not_dead = false;
		bool can_be_dead = !descr->has_side_effects;

		// Loop over operands to update the allocation information on VREG operands.
		for (int op_idx = 0; op_idx < 6; op_idx++) {
			IROperand *oper = &insn->operands[op_idx];
			if (!oper->is_valid()) break;

			if (oper->is_vreg()) {
				if (descr->format[op_idx] != 'I' && live_outs.count(oper->value)) not_dead = true;

				// If this vreg has been allocated to the stack, then fill in the stack entry location here
				//auto global_alloc = global_allocation.find(oper->value);
				if (global_allocation.count(oper->value)) {
					oper->allocate(IROperand::ALLOCATED_STACK, global_allocation[oper->value]);
					if (descr->format[op_idx] == 'O' || descr->format[op_idx] == 'B') not_dead = true;
				} else {

					// Otherwise, if the value has been locally allocated, fill in the local allocation
					//auto alloc_reg = allocation.find((IRRegId)oper->value);

					if (allocation[oper->value] != -1) {
						oper->allocate(IROperand::ALLOCATED_REG, allocation[oper->value]);
					}

				}
			}
		}

		// Remove allocations that are not LIVE-INs
		for (auto out : live_outs) {
			if (global_allocation.count(out)) continue;
			if (!live_ins.count(out)) {
				assert(allocation[out] != -1);

				allocation[out] = -1;
			}
		}

		// If this instruction is dead, remove any live ins which are not live outs
		// in order to propagate dead instruction elimination information
		if (!not_dead && can_be_dead) {
			make_instruction_nop(insn, true);

			to_erase.clear();
			to_erase.reserve(live_ins.size());
			for (auto i = live_ins.begin(); i != live_ins.end(); ++i) {
				const auto &in = *i;
				if (global_allocation.count(in)) continue;
				if (live_outs.count(in) == 0) {
					assert(allocation[in] != -1);

					avail_regs.set(allocation[in]);

					allocation[in] = -1;
					to_erase.push_back(i);
				}
			}
			for (auto e : to_erase)live_ins.erase(e);
		}

#if 0

		printf("  [%03d] %10s ", ir_idx, insn_descriptors[insn->type].mnemonic);
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
					printf("i4 pc");
				} else if (oper->is_block()) {
					printf("b%d", oper->value);
				} else if (oper->is_func()) {
					printf("&%x", oper->value);
				} else {
					printf("<invalid>");
				}
			}
		}

		printf(" {");
		for (auto in : live_ins) {
			//auto alloc = allocation.find(in);
			int alloc_reg = allocation[in]; //alloc == allocation.end() ? -1 : alloc->second;

			printf(" r%d:%d", in, alloc_reg);
		}
		printf(" } {");
		for (auto out : live_outs) {
			//auto alloc = allocation.find(out);
			int alloc_reg = allocation[out]; //alloc == allocation.end() ? -1 : alloc->second;

			printf(" r%d:%d", out, alloc_reg);
		}
		printf(" }\n");
#endif
	}

	//printf("block %08x\n", tb.block_addr);
	//	dump_ir();
	timer.tick("Allocation");
	//printf("lol ");
	timer.dump("Analysis");
	return true;
}

bool BlockCompiler::thread_jumps() {
	tick_timer<false> timer;
	timer.reset();

	std::vector<IRInstruction*> first_instructions(ctx.block_count(), NULL);
	std::vector<IRInstruction*> last_instructions(ctx.block_count(), NULL);

	timer.tick("Init");

	// Build up a list of the first instructions in each block.
	IRBlockId current_block_id = INVALID_BLOCK_ID;
	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);

		if (insn->ir_block != current_block_id) {
			if (ir_idx > 0) {
				last_instructions[current_block_id] = ctx.at(ir_idx - 1);
			}

			current_block_id = insn->ir_block;
			first_instructions[current_block_id] = insn;
		}
	}

	timer.tick("Analysis");

	for (unsigned int block_id = 0; block_id < last_instructions.size(); ++block_id) {
		IRInstruction *source_instruction = last_instructions[block_id];
		if (!source_instruction) continue;

		switch (source_instruction->type) {
		case IRInstruction::JMP:
		{
			IRInstruction *target_instruction = first_instructions[source_instruction->operands[0].value];

			while (target_instruction->type == IRInstruction::JMP) {
				target_instruction = first_instructions[target_instruction->operands[0].value];
			}

			if (target_instruction->type == IRInstruction::RET) {
				//*source_instruction = *target_instruction;
				//source_instruction->ir_block = block_id;
			} else if (target_instruction->type == IRInstruction::BRANCH) {
				*source_instruction = *target_instruction;
				source_instruction->ir_block = block_id;
				goto do_branch_thread;
				//} else if (target_instruction->type == IRInstruction::DISPATCH) {
				//	*source_instruction = *target_instruction;
				//	source_instruction->ir_block = block_id;
			} else {
				source_instruction->operands[0].value = target_instruction->ir_block;
			}

			break;
		}

		case IRInstruction::BRANCH:
		{
do_branch_thread:
			IRInstruction *true_target = first_instructions[source_instruction->operands[1].value];
			IRInstruction *false_target = first_instructions[source_instruction->operands[2].value];

			while (true_target->type == IRInstruction::JMP) {
				true_target = first_instructions[true_target->operands[0].value];
			}

			while (false_target->type == IRInstruction::JMP) {
				false_target = first_instructions[false_target->operands[0].value];
			}

			source_instruction->operands[1].value = true_target->ir_block;
			source_instruction->operands[2].value = false_target->ir_block;

			break;
		}

		case IRInstruction::DISPATCH:
		case IRInstruction::RET:
			break;
		default:
			fatal("last instruction was not control flow (it was a %s)\n", insn_descriptors[source_instruction->type].mnemonic);
		}
	}

	timer.tick("Threading");
	timer.dump("Jump Threading");

	return true;
}

bool BlockCompiler::dbe() {
	tick_timer<false> timer;
	timer.reset();

	std::vector<bool> live_blocks(ctx.block_count(), false);
	live_blocks[0] = true;

	timer.tick("Init");

	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);

		switch (insn->type) {
		case IRInstruction::JMP:
			live_blocks[insn->operands[0].value] = true;
			break;
		case IRInstruction::BRANCH:
			live_blocks[insn->operands[1].value] = true;
			live_blocks[insn->operands[2].value] = true;
			break;
		default:
			break;
		}
	}

	timer.tick("Liveness");

	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);
		if (!live_blocks[insn->ir_block]) make_instruction_nop(insn, true);
	}

	timer.tick("Killing");
	timer.dump("DBE");

	return true;
}

bool BlockCompiler::merge_blocks() {
	tick_timer<false> timer;
	timer.reset();
	std::vector<IRBlockId> succs(ctx.block_count(), -1);
	std::vector<int> pred_count(ctx.block_count(), 0);

	std::vector<IRBlockId> work_list;
	work_list.reserve(ctx.block_count());

	timer.tick("Init");

	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);
		if (insn->ir_block == NOP_BLOCK) continue;

		switch (insn->type) {
		case IRInstruction::JMP:
			//If a block ends in a jump, we should consider it a candidate for merging into
			work_list.push_back(insn->ir_block);
			succs[insn->ir_block] = insn->operands[0].value;
			pred_count[insn->operands[0].value]++;

			break;
		case IRInstruction::BRANCH:
			pred_count[insn->operands[1].value]++;
			pred_count[insn->operands[2].value]++;
			break;
		default:
			break;
		}
	}

	timer.tick("Identify");

	while (work_list.size()) {
		IRBlockId block = work_list.back();
		work_list.pop_back();

		// This block is only in the work list if it has only one successor, so we don't need to check for that

		// Look up the single successor of this block
		IRBlockId block_successor = succs[block];

		// If the successor has multiple predecessors, we can't merge it
		if (pred_count[block_successor] != 1) continue;

		if (!merge_block(block_successor, block)) continue;

		succs[block] = succs[block_successor];

		// If, post merging, the block has one successor, then put it on the work list
		if (succs[block] != (IRBlockId) - 1) work_list.push_back(block);
	}

	timer.tick("Merge");
	timer.dump("MB");
	return true;
}

bool BlockCompiler::merge_block(IRBlockId merge_from, IRBlockId merge_into) {
	// Don't try to merge 'backwards' yet since it's a bit more complicated
	if (merge_from < merge_into) return false;

	unsigned int ir_idx = 0;
	// Nop out the terminator instruction from the merge_into block
	for (; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);

		// We can only merge if the terminator is a jmp
		if (insn->ir_block == merge_into && insn->type == IRInstruction::JMP) {
			make_instruction_nop(insn, true);
			break;
		}
	}

	for (; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);
		if (insn->ir_block == NOP_BLOCK) continue;

		// Move instructions from the 'from' block to the 'to' block
		if (insn->ir_block == merge_from) insn->ir_block = merge_into;
		if (insn->ir_block > merge_from) break;
	}

	return true;
}

bool BlockCompiler::build_cfg(block_list_t& blocks, cfg_t& succs, cfg_t& preds, block_list_t& exits) {
	IRBlockId current_block_id = INVALID_BLOCK_ID;

	blocks.reserve(ctx.block_count());
	exits.reserve(ctx.block_count());

	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);
		if (insn->ir_block == NOP_BLOCK) continue;

		if (insn->ir_block != current_block_id) {
			current_block_id = insn->ir_block;
			blocks.push_back(current_block_id);
		}

		switch (insn->type) {
		case IRInstruction::BRANCH:
		{
			assert(insn->operands[1].is_block() && insn->operands[2].is_block());

			IRBlockId s1 = (IRBlockId) insn->operands[1].value;
			IRBlockId s2 = (IRBlockId) insn->operands[2].value;

			succs[current_block_id].push_back(s1);
			succs[current_block_id].push_back(s2);

			preds[s1].push_back(current_block_id);
			preds[s2].push_back(current_block_id);
			break;
		}

		case IRInstruction::JMP:
		{
			assert(insn->operands[0].is_block());

			IRBlockId s1 = (IRBlockId) insn->operands[0].value;

			succs[current_block_id].push_back(s1);
			preds[s1].push_back(current_block_id);

			break;
		}

		case IRInstruction::DISPATCH:
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

bool BlockCompiler::allocate() {
	return true;
}

bool BlockCompiler::lower_to_interpreter() {
	encoder.mov((uint64_t) & interpret_ir, REG_RAX);
	load_state_field(0, REG_RDI);
	encoder.mov((uint64_t) ctx.get_ir_buffer(), REG_RSI);
	encoder.mov((uint64_t) ctx.count(), REG_RDX);
	encoder.jmp(REG_RAX);

	asm volatile("out %0, $0xff\n" ::"a"(15), "D"(encoder.get_buffer()), "S"(encoder.get_buffer_size()), "d"(pa));
	return true;
}

bool BlockCompiler::lower(uint32_t max_stack) {
	bool success = true, dump_this_shit = false;

	std::vector<std::pair<uint32_t, IRBlockId> > block_relocations;
	block_relocations.reserve(ctx.block_count());
	std::vector<uint32_t> native_block_offsets(ctx.block_count(), 0);

	stack_map_t stack_map;

	// Function prologue

	// TODO: Check ISA

	/*encoder.mov(X86Memory::get(REGSTATE_REG, REG_OFFSET_OF(ISA)), REG_CL);
	
	if (isa_mode == 0) {
		encoder.test(REG_CL, REG_CL);
		encoder.jz(6);
		encoder.mov(1, REG_EAX);
		encoder.ret();
	} else {
		encoder.cmp(isa_mode, REG_CL);
		encoder.je(6);
		encoder.mov(1, REG_EAX);
		encoder.ret();
	}*/

	//encoder.mov(0, REG_EAX);
	//encoder.intt(0x90);

	//	uint32_t prologue_offset = encoder.current_offset();
	if (max_stack > 0x40)
		encoder.sub(max_stack - 0x40, REG_RSP);

#ifdef BLOCK_ENTRY_TRACKING
	encoder.mov(PC_REG, X86Memory::get(REG_FS, 0x58));
#endif

	uint32_t block_start_offset = encoder.current_offset();

	IRBlockId current_block_id = INVALID_BLOCK_ID;
	for (uint32_t ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);

		if (insn->ir_block == NOP_BLOCK) continue;

		// Record the native offset of an IR block
		if (current_block_id != insn->ir_block) {
			current_block_id = insn->ir_block;
			native_block_offsets[current_block_id] = encoder.current_offset();
		}

		switch (insn->type) {
		case IRInstruction::BARRIER:
		case IRInstruction::NOP:
			break;

		case IRInstruction::FETCH:
			if (sim_events & SIM_EVENT_FETCH) {
				encoder.out(REG_EAX, 0xef);
			}

			break;

		case IRInstruction::CMOV:
		{
			printf("not implemented cmov\n");
			return false;
		}

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
					if (source->value == 0) {
						x86::X86Register& temp_reg = get_temp(0, dest->size);
						encoder.xorr(temp_reg, temp_reg);
						encoder.mov(temp_reg, stack_from_operand(dest));
					} else {
						// mov imm -> stack
						switch (dest->size) {
						case 1: encoder.mov1(source->value, stack_from_operand(dest));
							break;
						case 2: encoder.mov2(source->value, stack_from_operand(dest));
							break;
						case 4: encoder.mov4(source->value, stack_from_operand(dest));
							break;
						case 8: encoder.mov8(source->value, stack_from_operand(dest));
							break;
						default: assert(false);
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

			if (offset->is_constant() && offset->value == REG_OFFSET_OF(PC)) {
				if (target->is_alloc_reg()) {
					IRInstruction *next = insn + 1;

					if (next->type == IRInstruction::ADD && next->operands[0].is_constant() && next->operands[1].is_alloc_reg() && next->operands[1].alloc_data == target->alloc_data) {
						encoder.lea(X86Memory::get(PC_REG64, next->operands[0].value), register_from_operand(target));
						ir_idx++;
					} else {
						encoder.mov(PC_REG, register_from_operand(target));
					}
				} else {
					fatal("WHOA");
				}
			} else {
				IRInstruction *next = insn + 1;

				if (target->is_alloc_reg() &&
						offset->is_constant() &&
						next->type == IRInstruction::CMPEQ &&
						next->operands[0].is_constant() &&
						next->operands[1].is_alloc_reg() &&
						next->operands[1].alloc_data == target->alloc_data &&
						next->operands[2].is_alloc_reg() &&
						next->operands[2].alloc_data == target->alloc_data) {

					IRInstruction *br = next + 1;

					if (br->type == IRInstruction::BRANCH && br->operands[0].is_alloc_reg() && br->operands[0].alloc_data == target->alloc_data) {
						switch (target->size) {
						case 1:
							encoder.cmp1(next->operands[0].value, X86Memory::get(REGSTATE_REG, offset->value));
							break;
						case 2:
							encoder.cmp2(next->operands[0].value, X86Memory::get(REGSTATE_REG, offset->value));
							break;
						case 4:
							encoder.cmp4(next->operands[0].value, X86Memory::get(REGSTATE_REG, offset->value));
							break;
						default:
							assert(false);
						}

						IROperand *tt = &br->operands[1];
						IROperand *ft = &br->operands[2];

						{
							uint32_t reloc_offset;
							encoder.jz_reloc(reloc_offset);
							block_relocations.push_back({reloc_offset, (IRBlockId) tt->value});
						}

						{
							uint32_t reloc_offset;
							encoder.jmp_reloc(reloc_offset);
							block_relocations.push_back({reloc_offset, (IRBlockId) ft->value});
						}

						//dump_this_shit = true;
						ir_idx += 2;
					} else {
						switch (target->size) {
						case 1:
							encoder.cmp1(next->operands[0].value, X86Memory::get(REGSTATE_REG, offset->value));
							break;
						case 2:
							encoder.cmp2(next->operands[0].value, X86Memory::get(REGSTATE_REG, offset->value));
							break;
						case 4:
							encoder.cmp4(next->operands[0].value, X86Memory::get(REGSTATE_REG, offset->value));
							break;
						default:
							assert(false);
						}

						encoder.sete(register_from_operand(&next->operands[2], 1));

						ir_idx++;
					}
				} else {
					IRInstruction *mod_insn, *store_insn, *potential_killer;
					mod_insn = insn + 1;
					store_insn = insn + 2;

					potential_killer = insn + 3;
					bool notfound = true;
					while (notfound) {
						switch (potential_killer->type) {
						case IRInstruction::INCPC:
						case IRInstruction::BARRIER:
							potential_killer++;
							break;
						default:
							notfound = false;
							break;
						}
					}


					// If these three instructions are a read-modify-write of the same register, then emit a modification of
					// the register, and then the normal read (eliminate the store)
					// e.g.
					// ldreg $0x0, v0
					// add $0x10, v0
					// streg v0, $0x0
					//  ||
					//  \/
					// add $0x10, $0x0(REGSTATE_REG) 
					// mov $0x0(REGSTATE_REG), [v0]
					//
					// We also determine if the final mov can be eliminted. It is only eliminated if the following instruction
					// kills the value read from the register, e.g.
					//
					// add $0x10, $0x0(REGSTATE_REG) 
					// mov $0x0(REGSTATE_REG), [v0]
					// mov $0x8(REGSTATE_REG), [v0]
					//  ||
					//  \/
					// add $0x10, $0x0(REGSTATE_REG) 
					// mov $0x8(REGSTATE_REG), [v0]
					unsigned reg_offset = insn->operands[0].value;
					if (mod_insn->ir_block == insn->ir_block && store_insn->ir_block == insn->ir_block && store_insn->type == IRInstruction::WRITE_REG && store_insn->operands[1].value == reg_offset) {

						IROperand *my_target = &insn->operands[1];
						IROperand *modify_source = &mod_insn->operands[0];
						IROperand *modify_target = &mod_insn->operands[1];
						IROperand *store_source = &store_insn->operands[0];

						if (my_target->is_alloc_reg() && !modify_source->is_alloc_stack() && !modify_target->is_alloc_stack() && store_source->is_alloc_reg()) {

							unsigned my_target_reg = my_target->alloc_data;
							unsigned modify_target_reg = modify_target->alloc_data;
							unsigned store_source_reg = store_source->alloc_data;

							// TODO: this is hideous
							bool fused = false;
							if (my_target_reg == modify_target_reg && modify_target_reg == store_source_reg) {
								switch (mod_insn->type) {
								case IRInstruction::ADD:
									if (modify_source->is_constant()) {
										encoder.add(modify_source->value, modify_source->size, X86Memory::get(REGSTATE_REG, reg_offset));
									} else if (modify_source->is_vreg()) {
										encoder.add(register_from_operand(modify_source), X86Memory::get(REGSTATE_REG, reg_offset));
									} else {
										assert(false);
									}
									fused = true;
									break;
								case IRInstruction::SUB:
									if (modify_source->is_constant()) {
										encoder.sub(modify_source->value, modify_source->size, X86Memory::get(REGSTATE_REG, reg_offset));
									} else if (modify_source->is_vreg()) {
										encoder.sub(register_from_operand(modify_source), X86Memory::get(REGSTATE_REG, reg_offset));
									} else {
										assert(false);
									}
									fused = true;
									break;
								case IRInstruction::OR:
									if (modify_source->is_constant()) {
										encoder.orr(modify_source->value, modify_source->size, X86Memory::get(REGSTATE_REG, reg_offset));
									} else if (modify_source->is_vreg()) {
										encoder.orr(register_from_operand(modify_source), X86Memory::get(REGSTATE_REG, reg_offset));
									} else {
										assert(false);
									}
									fused = true;
									break;
								case IRInstruction::AND:
									if (modify_source->is_constant()) {
										encoder.andd(modify_source->value, modify_source->size, X86Memory::get(REGSTATE_REG, reg_offset));
									} else if (modify_source->is_vreg()) {
										encoder.andd(register_from_operand(modify_source), X86Memory::get(REGSTATE_REG, reg_offset));
									} else {
										assert(false);
									}
									fused = true;
									break;
								case IRInstruction::XOR:
									if (modify_source->is_constant()) {
										encoder.xorr(modify_source->value, modify_source->size, X86Memory::get(REGSTATE_REG, reg_offset));
									} else if (modify_source->is_vreg()) {
										encoder.xorr(register_from_operand(modify_source), X86Memory::get(REGSTATE_REG, reg_offset));
									} else {
										assert(false);
									}
									fused = true;
									break;
								default:
									break;
								}

								if (fused) {
									ir_idx += 2;

									// If the IR node after the fused instruction kills the register we would load into, then
									// we don't need to load the register value
									auto &descr = insn_descriptors[potential_killer->type];
									//~ printf("Killer is %s %x\n", descr.mnemonic, pa);
									bool kills_value = false;
									for (int i = 0; i < 6; ++i) {
										IROperand *kop = &potential_killer->operands[i];
										// If the operand is an input of the register, we need the move
										if (descr.format[i] == 'I' || descr.format[i] == 'B') {
											if (kop->is_alloc_reg() && kop->alloc_data == store_source->alloc_data) {
												kills_value = false;
												//~ printf("doesn't kill\n");
												break;
											}
										}

										if (descr.format[i] == 'O') {
											if (kop->is_alloc_reg() && kop->alloc_data == store_source->alloc_data) {
												kills_value = true;
												//~ printf("kills\n");
												break;
											}
										}
									}

									// If the killer DOES kill the instruction, then we DO NOT need the move
									if (!kills_value) {
										encoder.mov(X86Memory::get(REGSTATE_REG, reg_offset), register_from_operand(store_source));
									}
									continue;
								}
							}
						}
					}

					if (offset->type == IROperand::CONSTANT) {
						// Load a constant offset guest register into the storage location
						if (target->is_alloc_reg()) {
							encoder.mov(X86Memory::get(REGSTATE_REG, offset->value), register_from_operand(target));
						} else if (target->is_alloc_stack()) {
							encoder.mov(X86Memory::get(REGSTATE_REG, offset->value), get_temp(0, 4));
							encoder.mov(get_temp(0, 4), stack_from_operand(target));
						} else {
							assert(false);
						}
					} else {
						assert(false);
					}
				}
			}

			break;
		}

		case IRInstruction::DISPATCH:
		{
			// If this dispatch hasn't got any compiled targets, do a return instead
			bool has_fallthrough = insn->operands[1].value != 0;

			// If this dispatch has a fallthrough PC, but no fallthrough block, don't try to dispatch
			if (insn->operands[1].value != 0 && insn->operands[3].value == 0) goto do_ret_instead;

			if (max_stack > 0x40) {
				encoder.add(max_stack - 0x40, REG_RSP);
			}

			encoder.ensure_extra_buffer(64);

			// Check to see if we need to stop chaining (e.g. an interrupt)
			encoder.cmp4(0, X86Memory::get(REG_FS, 48));

			uint32_t jump_offset1 = encoder.current_offset() + 1;
			encoder.jnz((int8_t) 0);

			size_t target_offset = insn->operands[2].value - ((size_t) encoder.get_buffer() + (size_t) encoder.current_offset());
			size_t fallthrough_offset = insn->operands[3].value - ((size_t) encoder.get_buffer() + (size_t) encoder.current_offset());

			bool jump_via_offsets = false;
			if (target_offset < 0x80000000 && (!has_fallthrough || fallthrough_offset < 0x80000000)) jump_via_offsets = true;

			if (jump_via_offsets) {
				if (has_fallthrough) {
					//load PC into EBX
					encoder.mov(PC_REG, REG_EBX);

					//mask page offset of PC (we already know that both targets land in this page)
					encoder.andd(0xfff, REG_EBX);

					encoder.mov(insn->operands[2].value, REG_RCX);
					encoder.mov(insn->operands[3].value, REG_RDX);

					encoder.cmp(insn->operands[0].value, REG_EBX);
					encoder.jne((int8_t) 2);
					encoder.jmp(REG_RCX);
					encoder.jmp(REG_RDX);

				} else {
					encoder.mov(insn->operands[2].value, REG_RCX);
					encoder.jmp(REG_RCX);
				}

			} else {
				if (has_fallthrough) {
					//load PC into EBX
					encoder.mov(PC_REG, REG_EBX);

					//mask page offset of PC (we already know that both targets land in this page)
					encoder.andd(0xfff, REG_EBX);
					encoder.cmp(insn->operands[0].value, REG_EBX);

					target_offset = insn->operands[2].value - ((size_t) encoder.get_buffer() + (size_t) encoder.current_offset());
					encoder.je((int32_t) target_offset - 6);

					fallthrough_offset = insn->operands[3].value - ((size_t) encoder.get_buffer() + (size_t) encoder.current_offset());
					encoder.jmp_offset((int32_t) fallthrough_offset - 5);
				} else {
					target_offset = insn->operands[2].value - ((size_t) encoder.get_buffer() + (size_t) encoder.current_offset());
					encoder.jmp_offset((int32_t) target_offset - 5);
				}
			}

			uint32_t current_offset = encoder.current_offset();
			*((uint8_t *) encoder.get_buffer() + jump_offset1) = (uint8_t) (current_offset - jump_offset1 - 1);

			// jnz above lands here
			encoder.mov1(0, X86Memory::get(REG_FS, 48));
			encoder.xorr(REG_EAX, REG_EAX);
			encoder.ret();

			break;
		}
		case IRInstruction::RET:
		{
do_ret_instead:
			if (max_stack > 0x40)
				encoder.add(max_stack - 0x40, REG_RSP);

			encoder.ensure_extra_buffer(64);

			// Check to see if we need to stop chaining (e.g. an interrupt)
			encoder.cmp1(0, X86Memory::get(REG_FS, 48));
			uint32_t jump_offset1 = encoder.current_offset() + 1;
			encoder.jnz((int8_t) 0);

#ifdef BLOCK_ENTRY_TRACKING
			encoder.cmp(PC_REG, X86Memory::get(REG_FS, 0x58));
			encoder.jne(9);
			encoder.incq(X86Memory::get(REG_FS, 0x60));
			//			encoder.intt(0x90);
#endif

			// Each chaining table entry is 16 bytes, arranged
			// 0	tag (4 bytes)
			// 8	pointer (8 bytes)

			encoder.mov(X86Memory::get(REG_FS, 32), REG_RBX); // Load the cache base address
			encoder.mov(PC_REG, REG_EAX); // Load the PC

			//encoder.andd(0x3fffc, REG_EAX);									// Mask the PC
			//encoder.andd(0x3ffc, REG_EAX);
			encoder.andd(0x7ffc, REG_EAX);

			encoder.cmp(PC_REG, X86Memory::get(REG_RBX, 0, REG_RAX, 4)); // Compare PC with cache entry tag

			uint32_t jump_offset2 = encoder.current_offset() + 1;
			encoder.jne((int8_t) 0); // Tags match?			
			encoder.jmp(X86Memory::get(REG_RBX, 8, REG_RAX, 4)); // Yep, tail call.

			*((uint8_t *) ((uint8_t*) encoder.get_buffer() + jump_offset1)) = (uint8_t) (uint64_t) (encoder.current_offset() - jump_offset1 - 1);

			encoder.mov4(0, X86Memory::get(REG_FS, 48));

			*((uint8_t *) ((uint8_t*) encoder.get_buffer() + jump_offset2)) = (uint8_t) (uint64_t) (encoder.current_offset() - jump_offset2 - 1);
			encoder.xorr(REG_EAX, REG_EAX);
			encoder.ret(); // RETURN

			break;
		}

		case IRInstruction::LOOP:
		{
			encoder.ensure_extra_buffer(64);

			// Check to see if we need to stop chaining (e.g. an interrupt)
			encoder.cmp1(0, X86Memory::get(REG_FS, 48));
			uint32_t jump_offset1 = encoder.current_offset() + 1;
			encoder.jnz((int8_t) 0);

			// Check to see if this is a self-loop
			encoder.mov(PC_REG, REG_EAX);
			encoder.andd(0xfff, REG_EAX);
			encoder.cmp((pa & 0xfff), REG_EAX);
			encoder.je((int32_t) (block_start_offset - encoder.current_offset() - 6));

			// Each chaining table entry is 16 bytes, arranged
			// 0	tag (4 bytes)
			// 8	pointer (8 bytes)

			encoder.mov(X86Memory::get(REG_FS, 32), REG_RBX); // Load the cache base address
			encoder.mov(PC_REG, REG_EAX); // Load the PC
			encoder.andd(0x7ffc, REG_EAX); // Mask the PC

			encoder.cmp(PC_REG, X86Memory::get(REG_RBX, 0, REG_RAX, 4)); // Compare PC with cache entry tag

			uint32_t jump_offset2 = encoder.current_offset() + 1;
			encoder.jne((int8_t) 0); // Tags match?

			if (max_stack > 0x40)
				encoder.add(max_stack - 0x40, REG_RSP);

			encoder.jmp(X86Memory::get(REG_RBX, 8, REG_RAX, 4)); // Yep, tail call.

			*((uint8_t *) ((uint8_t*) encoder.get_buffer() + jump_offset1)) = (uint8_t) (uint64_t) (encoder.current_offset() - jump_offset1 - 1);

			encoder.mov4(0, X86Memory::get(REG_FS, 48));

			*((uint8_t *) ((uint8_t*) encoder.get_buffer() + jump_offset2)) = (uint8_t) (uint64_t) (encoder.current_offset() - jump_offset2 - 1);

			if (max_stack > 0x40)
				encoder.add(max_stack - 0x40, REG_RSP);

			encoder.xorr(REG_EAX, REG_EAX);
			encoder.ret(); // RETURN

			break;
		}

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
					default:
						assert(false);
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
					default:
						assert(false);
						break;
					}
				} else if (source->is_alloc_stack() && dest->is_alloc_stack()) {
					assert(false);
				} else {
					assert(false);
				}
			} else if (source->type == IROperand::PC) {
				if (dest->is_alloc_reg()) {
					switch (insn->type) {
					case IRInstruction::ADD:
						//encoder.add(X86Memory::get(REGSTATE_REG, REG_OFFSET_OF(PC)), register_from_operand(dest));
						encoder.add(PC_REG, register_from_operand(dest));
						break;
					case IRInstruction::SUB:
						//encoder.sub(X86Memory::get(REGSTATE_REG, REG_OFFSET_OF(PC)), register_from_operand(dest));
						encoder.sub(PC_REG, register_from_operand(dest));
						break;
					default:
						assert(false);
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
						default:
							assert(false);
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
						default:
							assert(false);
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
				if (offset->value == REG_OFFSET_OF(PC)) {
					assert(value->size == 4);

					if (value->type == IROperand::CONSTANT) {
						encoder.mov(value->value, PC_REG);
					} else if (value->type == IROperand::VREG) {
						if (value->is_alloc_reg()) {
							encoder.mov(register_from_operand(value), PC_REG);
						} else if (value->is_alloc_stack()) {
							encoder.mov(stack_from_operand(value), PC_REG);
						} else {
							assert(false);
						}
					} else {
						assert(false);
					}
				} else {
					if (value->type == IROperand::CONSTANT) {
						IRInstruction *next = insn + 1;

						/*if (next->type == IRInstruction::WRITE_REG && 
								next->operands[0].is_constant() && 
								next->operands[1].is_constant() &&
								(next->operands[1].value == offset->value-1 || next->operands[1].value == offset->value+1)) {

							dump_this_shit = true;
						}*/


						switch (value->size) {
						case 8:
							encoder.mov(value->value, get_temp(0, 8));
							encoder.mov(get_temp(0, 8), X86Memory::get(REGSTATE_REG, offset->value));

							//encoder.mov8(value->value, X86Memory::get(REGSTATE_REG, offset->value));
							break;
						case 4:
							encoder.mov4(value->value, X86Memory::get(REGSTATE_REG, offset->value));
							break;
						case 2:
							encoder.mov2(value->value, X86Memory::get(REGSTATE_REG, offset->value));
							break;
						case 1:
							encoder.mov1(value->value, X86Memory::get(REGSTATE_REG, offset->value));
							break;
						default: assert(false);
						}
					} else if (value->type == IROperand::VREG) {
						if (value->is_alloc_reg()) {
							encoder.mov(register_from_operand(value), X86Memory::get(REGSTATE_REG, offset->value));
						} else if (value->is_alloc_stack()) {
							X86Register& tmp = get_temp(0, value->size);
							encoder.mov(stack_from_operand(value), tmp);
							encoder.mov(tmp, X86Memory::get(REGSTATE_REG, offset->value));
						} else {
							assert(false);
						}
					} else {
						assert(false);
					}
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

			IRInstruction *next_insn = (ir_idx + 1) < ctx.count() ? ctx.at(ir_idx + 1) : NULL;

			if (next_insn && next_insn->ir_block == (IRBlockId) target->value) {
				// The next instruction is in the block we're about to jump to, so we can
				// leave this as a fallthrough.
			} else {
				// Create a relocated jump instruction, and store the relocation offset and
				// target into the relocations list.
				uint32_t reloc_offset;
				encoder.jmp_reloc(reloc_offset);

				block_relocations.push_back({reloc_offset, (IRBlockId) target->value});

				encoder.align_up(8);
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
				if (cond->is_constant()) {
					IRBlockId target = cond->value ? tt->value : ft->value;
					uint32_t reloc_offset;
					encoder.jmp_reloc(reloc_offset);
					block_relocations.push_back({reloc_offset, (IRBlockId) target});

					encoder.align_up(8);
					break;
				}
				assert(false);
			}

			IRInstruction *next_insn = (ir_idx + 1) < ctx.count() ? ctx.at(ir_idx + 1) : NULL;

			if (next_insn && next_insn->ir_block == (IRBlockId) tt->value) {
				// Fallthrough is TRUE block
				{
					uint32_t reloc_offset;
					encoder.jz_reloc(reloc_offset);
					block_relocations.push_back({reloc_offset, (IRBlockId) ft->value});
				}
			} else if (next_insn && next_insn->ir_block == (IRBlockId) ft->value) {
				// Fallthrough is FALSE block
				{
					uint32_t reloc_offset;
					encoder.jnz_reloc(reloc_offset);
					block_relocations.push_back({reloc_offset, (IRBlockId) tt->value});
				}
			} else {
				// Fallthrough is NEITHER
				{
					uint32_t reloc_offset;
					encoder.jnz_reloc(reloc_offset);
					block_relocations.push_back({reloc_offset, (IRBlockId) tt->value});
				}

				{
					uint32_t reloc_offset;
					encoder.jmp_reloc(reloc_offset);
					block_relocations.push_back({reloc_offset, (IRBlockId) ft->value});
				}

				encoder.align_up(8);
			}

			break;
		}

		case IRInstruction::CALL:
		{
			IROperand *target = &insn->operands[0];

			IRInstruction *prev_insn = (ir_idx) > 0 ? ctx.at(ir_idx - 1) : NULL;
			IRInstruction *next_insn = (ir_idx + 1) < ctx.count() ? ctx.at(ir_idx + 1) : NULL;

			if (prev_insn) {
				if (prev_insn->type == IRInstruction::CALL && insn->count_operands() == prev_insn->count_operands()) {
					// Don't save the state, because the previous instruction was a call and it is already saved.
				} else {
					emit_save_reg_state(insn->count_operands(), stack_map);
				}
			} else {
				emit_save_reg_state(insn->count_operands(), stack_map);
			}

			//emit_save_reg_state();

			// DI, SI, DX, CX, R8, R9
			const X86Register * sysv_abi[] = {&REG_RDI, &REG_RSI, &REG_RDX, &REG_RCX, &REG_R8, &REG_R9};

			// CPU State
			load_state_field(0, *sysv_abi[0]);

			for (int i = 1; i < 6; i++) {
				if (insn->operands[i].type != IROperand::NONE) {
					encode_operand_function_argument(&insn->operands[i], *sysv_abi[i], stack_map);
				}
			}

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov(target->value, get_temp(1, 8));
			encoder.call(get_temp(1, 8));

			if (next_insn) {
				if (next_insn->type == IRInstruction::CALL && insn->count_operands() == next_insn->count_operands()) {
					// Don't restore the state, because the next instruction is a call and it will use it.
				} else {
					emit_restore_reg_state(insn->count_operands(), stack_map);
				}
			} else {
				emit_restore_reg_state(insn->count_operands(), stack_map);
			}

			break;
		}

		case IRInstruction::LDPC:
		{
			IROperand *target = &insn->operands[0];

			if (target->is_alloc_reg()) {
				if ((insn->unsafe_next()->type == IRInstruction::SUB || insn->unsafe_next()->type == IRInstruction::ADD) &&
						insn->unsafe_next()->operands[0].is_constant() &&
						insn->unsafe_next()->operands[0].value < 0x7fffffff &&
						insn->unsafe_next()->operands[1].is_alloc_reg() &&
						insn->unsafe_next()->operands[1].alloc_data == target->alloc_data) {

					int32_t amt;
					if (insn->unsafe_next()->type == IRInstruction::SUB) {
						amt = (int32_t) (~((uint32_t) insn->unsafe_next()->operands[0].value)) + 1;
					} else {
						amt = (int32_t) (uint32_t) insn->unsafe_next()->operands[0].value;
					}

					//printf("LDPC BY %d %d @ %08x\n", amt, insn->unsafe_next()->operands[0].value, pa);

					encoder.lea(X86Memory::get(PC_REG64, amt), register_from_operand(target));
					ir_idx++;
				} else {
					encoder.mov(PC_REG, register_from_operand(target));
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::INCPC:
		{
			IROperand *amount = &insn->operands[0];

			if (amount->is_constant()) {
				encoder.lea(X86Memory::get(PC_REG64, amount->value), PC_REG);
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::STPC:
		{
			IROperand *value = &insn->operands[0];

			if (value->is_constant()) {
				encoder.mov(value->value, PC_REG);
			} else if (value->is_alloc_reg()) {
				encoder.mov(register_from_operand(value), PC_REG);
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::READ_MEM:
		{
			IROperand *offset = &insn->operands[0];
			IROperand *disp = &insn->operands[1];
			IROperand *dest = &insn->operands[2];

			assert(disp->is_constant());

#ifdef TRACE_MEM_INSNS
			encoder.incq(X86Memory::get(REG_FS, 56));
#endif

			if (offset->is_vreg()) {
				if (offset->is_alloc_reg() && dest->is_alloc_reg()) {
					// mov const(reg), reg

					if (sim_events & SIM_EVENT_READ) {
						// Load memory address
						encoder.lea(X86Memory::get(register_from_operand(offset), disp->value), REG_ECX);

						// Do Load
						encoder.mov(X86Memory::get(REG_RCX), register_from_operand(dest));

						switch (dest->size) {
						case 1: encoder.out(REG_EAX, 0xe0); break;
						case 2: encoder.out(REG_EAX, 0xe1);	break;
						case 4: encoder.out(REG_EAX, 0xe2);	break;
						case 8: encoder.out(REG_EAX, 0xe3);	break;
						default: assert(false);
						}
					} else {
						encoder.mov(X86Memory::get(register_from_operand(offset), disp->value), register_from_operand(dest));
					}
				} else {
					// TODO: FIXME: ACCOUNT FOR THIS IN SIMULATION
					
					// Destination is sometimes not allocated, e.g. if an ldrd loads to a location, then only one
					// of the loaded registers is used but the other is killed
					encoder.mov(X86Memory::get(register_from_operand(offset), disp->value), get_temp(0, dest->size));
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::WRITE_MEM:
		{
			IROperand *value = &insn->operands[0];
			IROperand *disp = &insn->operands[1];
			IROperand *offset = &insn->operands[2];

			assert(disp->is_constant());

#ifdef TRACE_MEM_INSNS
			encoder.incq(X86Memory::get(REG_FS, 64));
#endif

			if (offset->is_vreg()) {
				if (value->is_vreg()) {
					if (offset->is_alloc_reg() && value->is_alloc_reg()) {
						// mov reg, const(reg)

						if (sim_events & SIM_EVENT_WRITE) {
							// Load memory address
							encoder.lea(X86Memory::get(register_from_operand(offset), disp->value), REG_ECX);

							// Perform store
							encoder.mov(register_from_operand(value), X86Memory::get(REG_RCX));

							switch (value->size) {
							case 1: encoder.out(REG_EAX, 0xe4); break;
							case 2: encoder.out(REG_EAX, 0xe5);	break;
							case 4: encoder.out(REG_EAX, 0xe6);	break;
							case 8: encoder.out(REG_EAX, 0xe7);	break;
							default: assert(false);
							}
						} else {
							// Perform memory access
							encoder.mov(register_from_operand(value), X86Memory::get(register_from_operand(offset), disp->value));
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

		case IRInstruction::READ_MEM_USER:
		{
			IROperand *offset = &insn->operands[0];
			IROperand *dest = &insn->operands[1];

			if (offset->is_vreg()) {
				if (offset->is_alloc_reg() && dest->is_alloc_reg()) {
					// mov %gs:(reg), reg
					encoder.mov(X86Memory::get(REG_GS, register_from_operand(offset)), register_from_operand(dest));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::WRITE_MEM_USER:
		{
			IROperand *value = &insn->operands[0];
			IROperand *offset = &insn->operands[1];

			if (offset->is_vreg()) {
				if (value->is_vreg()) {
					if (offset->is_alloc_reg() && value->is_alloc_reg()) {
						// mov reg, %gs:(reg)
						encoder.mov(register_from_operand(value), X86Memory::get(REG_GS, register_from_operand(offset)));
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

		case IRInstruction::ATOMIC_WRITE:
		{
			IROperand *addr = &insn->operands[0];
			IROperand *value = &insn->operands[1];

			if (addr->is_vreg()) {
				if (addr->is_alloc_reg() && value->is_alloc_reg()) {
					encoder.xchg(X86Memory::get(register_from_operand(addr)), register_from_operand(value));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			break;
		}

		case IRInstruction::WRITE_DEVICE:
		{
			IROperand *dev = &insn->operands[0];
			IROperand *reg = &insn->operands[1];
			IROperand *val = &insn->operands[2];

			IRInstruction *prev_insn = (ir_idx) > 0 ? ctx.at(ir_idx - 1) : NULL;
			IRInstruction *next_insn = (ir_idx + 1) < ctx.count() ? ctx.at(ir_idx + 1) : NULL;

			if (prev_insn && prev_insn->type == IRInstruction::WRITE_DEVICE) {
				//
			} else {
				emit_save_reg_state(4, stack_map);
			}

			load_state_field(0, REG_RDI);

			encode_operand_function_argument(dev, REG_RSI, stack_map);
			encode_operand_function_argument(reg, REG_RDX, stack_map);
			encode_operand_function_argument(val, REG_RCX, stack_map);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t) & cpu_write_device, get_temp(1, 8));
			encoder.call(get_temp(1, 8));

			if (next_insn && next_insn->type == IRInstruction::WRITE_DEVICE) {
				//
			} else {
				emit_restore_reg_state(4, stack_map);
			}

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

			emit_save_reg_state(4, stack_map);

			load_state_field(0, REG_RDI);

			encode_operand_function_argument(dev, REG_RSI, stack_map);
			encode_operand_function_argument(reg, REG_RDX, stack_map);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t) & cpu_read_device, get_temp(1, 8));
			encoder.call(get_temp(1, 8));

			emit_restore_reg_state(4, stack_map);

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
						encoder.xorr(register_from_operand(dest), register_from_operand(dest));
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

		case IRInstruction::NOT:
		{
			IROperand *dest = &insn->operands[0];

			if (dest->is_alloc_reg()) {
				encoder.nott(register_from_operand(dest));
			} else if (dest->is_alloc_stack()) {
				encoder.nott(stack_from_operand(dest));
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
					default:
						assert(false);
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
					default:
						assert(false);
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
					default:
						assert(false);
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
						default:
							assert(false);
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
						default:
							assert(false);
							break;
						}
					}
				} else if (dest->is_alloc_stack()) {
					// OPER const -> stack

					if (source->size == 8) assert(false);

					switch (insn->type) {
					case IRInstruction::AND:
						encoder.andd(source->value, dest->size, stack_from_operand(dest));
						break;
					case IRInstruction::OR:
						encoder.orr(source->value, dest->size, stack_from_operand(dest));
						break;
					case IRInstruction::XOR:
						encoder.xorr(source->value, dest->size, stack_from_operand(dest));
						break;
					default:
						assert(false);
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
							if (register_from_operand(source) == REG_EAX && register_from_operand(dest) == REG_RAX) {
								encoder.cltq();
							} else if (register_from_operand(source) == REG_AX && register_from_operand(dest) == REG_EAX) {
								encoder.cwtl();
							} else if (register_from_operand(source) == REG_AL && register_from_operand(dest) == REG_AX) {
								encoder.cbtw();
							} else {
								encoder.movsx(register_from_operand(source), register_from_operand(dest));
							}

							break;
						default:
							assert(false);
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
						default:
							assert(false);
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
						// Apparently we can't yet encode cmp (stack), (reg) so encode cmp (reg), (stack) instead
						// and invert the result
						invert = true;

						encoder.cmp(register_from_operand(lhs), stack_from_operand(rhs));
					} else if (lhs->is_alloc_stack() && rhs->is_alloc_stack()) {
						// Apparently we can't yet encode cmp (stack), (reg) so encode cmp (reg), (stack) instead
						// and invert the result
						invert = true;

						X86Register& tmp = unspill_temp(lhs, 0);
						encoder.cmp(tmp, stack_from_operand(rhs));
					} else {
						assert(false);
					}

					break;
				}

				case IROperand::CONSTANT:
				{
					if (lhs->is_alloc_reg()) {
						encoder.cmp(rhs->value, register_from_operand(lhs));
					} else if (lhs->is_alloc_stack()) {
						switch (rhs->size) {
						case 1: encoder.cmp1(rhs->value, stack_from_operand(lhs));
							break;
						case 2: encoder.cmp2(rhs->value, stack_from_operand(lhs));
							break;
						case 4: encoder.cmp4(rhs->value, stack_from_operand(lhs));
							break;
						case 8: encoder.cmp8(rhs->value, stack_from_operand(lhs));
							break;
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
						case 1: encoder.cmp1(lhs->value, stack_from_operand(rhs));
							break;
						case 2: encoder.cmp2(lhs->value, stack_from_operand(rhs));
							break;
						case 4: encoder.cmp4(lhs->value, stack_from_operand(rhs));
							break;
						case 8: encoder.cmp8(lhs->value, stack_from_operand(rhs));
							break;
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

			bool should_branch = false;

			IRInstruction *next_insn = (ir_idx + 1) < ctx.count() ? ctx.at(ir_idx + 1) : NULL;

			// TODO: Look at this optimisation
			if (next_insn && next_insn->type == IRInstruction::BRANCH) {
				// If the next instruction is a branch, we need to check to see if it's branching on
				// this condition, before we go ahead an emit an optimised form.

				if (next_insn->operands[0].is_vreg() && next_insn->operands[0].value == dest->value) {
					// Right here we go, we've got a compare-and-branch situation.

					// Set the do-a-branch-instead flag
					should_branch = true;
				}
			}

			switch (insn->type) {
			case IRInstruction::CMPEQ:
				encoder.sete(register_from_operand(dest));

				if (should_branch) {
					assert(dest->is_alloc_reg());
					IRBlockId next_block = (next_insn + 1)->ir_block;

					// Skip the next instruction (which is the branch)
					ir_idx++;

					IROperand *tt = &next_insn->operands[1];
					IROperand *ft = &next_insn->operands[2];

					{
						uint32_t reloc_offset;
						encoder.je_reloc(reloc_offset);
						block_relocations.push_back({reloc_offset, (IRBlockId) tt->value});
					}

					{
						if (ft->value != next_block) {
							uint32_t reloc_offset;
							encoder.jmp_reloc(reloc_offset);
							block_relocations.push_back({reloc_offset, (IRBlockId) ft->value});
						}
					}
				}
				break;

			case IRInstruction::CMPNE:
				encoder.setne(register_from_operand(dest));
				if (should_branch) {
					assert(dest->is_alloc_reg());
					IRBlockId next_block = (next_insn + 1)->ir_block;

					// Skip the next instruction (which is the branch)
					ir_idx++;

					IROperand *tt = &next_insn->operands[1];
					IROperand *ft = &next_insn->operands[2];

					{
						uint32_t reloc_offset;
						encoder.jne_reloc(reloc_offset);
						block_relocations.push_back({reloc_offset, (IRBlockId) tt->value});
					}

					{
						if (ft->value != next_block) {
							uint32_t reloc_offset;
							encoder.jmp_reloc(reloc_offset);
							block_relocations.push_back({reloc_offset, (IRBlockId) ft->value});
						}
					}
				}

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
			/*IROperand *mode = &insn->operands[0];

			emit_save_reg_state();

			load_state_field(0, REG_RDI);

			encode_operand_function_argument(mode, REG_RSI);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&cpu_set_mode, get_temp(0, 8));
			encoder.call(get_temp(0, 8));

			emit_restore_reg_state();*/

			break;
		}

		case IRInstruction::SHL:
		case IRInstruction::SHR:
		case IRInstruction::SAR:
		case IRInstruction::ROR:
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
					case IRInstruction::ROR:
						encoder.ror(amount->value, operand);
						break;
					default:
						assert(false);
						break;
					}
				} else {
					X86Memory operand = stack_from_operand(dest);

					switch (insn->type) {
					case IRInstruction::SHL:
						encoder.shl(amount->value, dest->size, operand);
						break;
					case IRInstruction::SHR:
						encoder.shr(amount->value, dest->size, operand);
						break;
					case IRInstruction::SAR:
						encoder.sar(amount->value, dest->size, operand);
						break;
					case IRInstruction::ROR:
						encoder.ror(amount->value, dest->size, operand);
						break;
					default:
						assert(false);
						break;
					}
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
						case IRInstruction::ROR:
							encoder.ror(REG_CL, operand);
							break;
						default:
							assert(false);
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

		case IRInstruction::COUNT:
		{
			X86Register& tmp = get_temp(0, 8);

			load_state_field(40, tmp);
			encoder.add8(1, X86Memory::get(tmp));

			break;
		}

		case IRInstruction::TRIGGER_IRQ:
		{
			encoder.mov(1, REG_ECX);
			encoder.mov(REG_RCX, X86Memory::get(REG_FS, 48));

			break;
		}

		case IRInstruction::FLUSH:
		case IRInstruction::FLUSH_ITLB:
		case IRInstruction::FLUSH_DTLB:
		{
			encoder.mov(1, REG_ECX);

#ifdef SYSCALL_CALL_GATE
			encoder.lcall(X86Memory(REG_RIP));
			encoder.emit64(0xdeadbeefbabecafe);
			encoder.emit16(0x38);
#else
			encoder.intt(0x85);
#endif
			break;
		}

		case IRInstruction::FLUSH_ITLB_ENTRY:
		case IRInstruction::FLUSH_DTLB_ENTRY:
		{
			if (insn->operands[0].is_constant()) {
				encoder.mov(insn->operands[0].value, REG_R14);
			} else if (insn->operands[0].is_vreg()) {
				if (insn->operands[0].is_alloc_reg()) {
					const X86Register& addr = register_from_operand(&insn->operands[0], 4);
					encoder.mov(addr, REG_R14D);
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			encoder.mov(4, REG_ECX);

#ifdef SYSCALL_CALL_GATE
			encoder.lcall(X86Memory(REG_RIP));
			encoder.emit64(0xdeadbeefbabecafe);
			encoder.emit16(0x38);
#else
			encoder.intt(0x85);
#endif
			break;
		}

		case IRInstruction::FLUSH_CONTEXT_ID:
		{
			if (insn->operands[0].is_constant()) {
				encoder.mov(insn->operands[0].value, REG_R14);
			} else if (insn->operands[0].is_vreg()) {
				if (insn->operands[0].is_alloc_reg()) {
					const X86Register& addr = register_from_operand(&insn->operands[0], 4);
					encoder.mov(addr, REG_R14D);
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			encoder.mov(8, REG_ECX);

#ifdef SYSCALL_CALL_GATE
			encoder.lcall(X86Memory(REG_RIP));
			encoder.emit64(0xdeadbeefbabecafe);
			encoder.emit16(0x38);
#else
			encoder.intt(0x85);
#endif
			break;
		}

		case IRInstruction::INVALIDATE_ICACHE:
		{
			/*encoder.mov(6, REG_ECX);
#ifdef SYSCALL_CALL_GATE
			encoder.lcall(X86Memory(REG_RIP));
			encoder.emit64(0xdeadbeefbabecafe);
			encoder.emit16(0x38);
#else
			encoder.intt(0x85);
#endif*/
			break;
		}

		case IRInstruction::INVALIDATE_ICACHE_ENTRY:
		{
			/*if (insn->operands[0].is_constant()) {
				encoder.mov(insn->operands[0].value, REG_R14);
			} else if (insn->operands[0].is_vreg()) {
				if (insn->operands[0].is_alloc_reg()) {
					const X86Register& addr = register_from_operand(&insn->operands[0], 4);
					encoder.mov(addr, REG_R14D);
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}
			
			encoder.mov(7, REG_ECX);
			
#ifdef SYSCALL_CALL_GATE
			encoder.lcall(X86Memory(REG_RIP));
			encoder.emit64(0xdeadbeefbabecafe);
			encoder.emit16(0x38);
#else
			encoder.intt(0x85);
#endif*/
			break;
		}

		case IRInstruction::SET_CONTEXT_ID:
		{
			if (insn->operands[0].is_constant()) {
				encoder.mov(insn->operands[0].value, REG_R14);
			} else if (insn->operands[0].is_vreg()) {
				if (insn->operands[0].is_alloc_reg()) {
					const X86Register& addr = register_from_operand(&insn->operands[0], 4);
					encoder.mov(addr, REG_R14D);
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			encoder.mov(9, REG_ECX);

#ifdef SYSCALL_CALL_GATE
			encoder.lcall(X86Memory(REG_RIP));
			encoder.emit64(0xdeadbeefbabecafe);
			encoder.emit16(0x38);
#else
			encoder.intt(0x85);
#endif
			break;
		}

		case IRInstruction::PGT_CHANGE:
		{
			encoder.mov(10, REG_ECX);

#ifdef SYSCALL_CALL_GATE
			encoder.lcall(X86Memory(REG_RIP));
			encoder.emit64(0xdeadbeefbabecafe);
			encoder.emit16(0x38);
#else
			encoder.intt(0x85);
#endif
			break;
		}

		case IRInstruction::SWITCH_TO_KERNEL_MODE:
		{
			if (used_phys_regs.get(8))
				encoder.push(REG_R11);

			encoder.mov(X86Memory::get(REG_FS, 0x50), REG_R14);
			encoder.mov1(1, X86Memory::get(REG_R14));
			encoder.syscall();

			if (used_phys_regs.get(8))
				encoder.pop(REG_R11);
			break;
		}

		case IRInstruction::SWITCH_TO_USER_MODE:
		{
			if (used_phys_regs.get(8))
				encoder.push(REG_R11);

			encoder.pushf();

			encoder.mov(X86Memory::get(REG_FS, 0x50), REG_R14);
			encoder.mov1(0, X86Memory::get(REG_R14));

			encoder.pop(REG_R11);

			encoder.lea(X86Memory::get(REG_RIP, 2), REG_RCX);
			encoder.sysret();

			if (used_phys_regs.get(8))
				encoder.pop(REG_R11);
			break;
		}

		case IRInstruction::SET_ZN_FLAGS:
		{
			IROperand *val = &insn->operands[0];

			if (val->is_alloc_reg()) {
				encoder.test(register_from_operand(val), register_from_operand(val));
			} else {
				assert(false);
			}

			encoder.setz(X86Memory::get(REGSTATE_REG, REG_OFFSET_OF(Z))); // Zero
			encoder.sets(X86Memory::get(REGSTATE_REG, REG_OFFSET_OF(N))); // Negative

			break;
		}


		case IRInstruction::ADC:
		case IRInstruction::SBC:
		case IRInstruction::ADC_WITH_FLAGS:
		case IRInstruction::SBC_WITH_FLAGS:
		{
			IROperand *src = &insn->operands[0];
			IROperand *dst = &insn->operands[1];
			IROperand *carry = &insn->operands[2];

			bool just_do_a_normal_operation = false;
			bool is_an_add = insn->type == IRInstruction::ADC || insn->type == IRInstruction::ADC_WITH_FLAGS;

			// Set up carry bit for adc/sbc
			if (carry->is_constant()) {
				if (is_an_add) {
					if (carry->value == 0) {
						just_do_a_normal_operation = true;
					} else {
						encoder.stc();
					}
				} else {
					if (carry->value == 1) {
						just_do_a_normal_operation = true;
					} else {
						encoder.stc();
					}
				}
			} else if (carry->is_vreg()) {
				if (is_an_add) {
					encoder.mov(0xff, get_temp(0, 1));
					if (carry->is_alloc_reg()) {
						encoder.add(register_from_operand(carry, 1), get_temp(0, 1));
					} else if (carry->is_alloc_stack()) {
						encoder.add(stack_from_operand(carry), get_temp(0, 1));
					} else {
						assert(false);
					}
				} else {
					encoder.xorr(get_temp(0, 1), get_temp(0, 1));
					if (carry->is_alloc_reg()) {
						encoder.sub(register_from_operand(carry, 1), get_temp(0, 1));
					} else if (carry->is_alloc_stack()) {
						encoder.sub(stack_from_operand(carry), get_temp(0, 1));
					} else {
						assert(false);
					}

					encoder.cmc();
				}
			} else {
				assert(false);
			}

			if (just_do_a_normal_operation) {
				if (src->is_constant() && dst->is_alloc_reg()) {
					if (is_an_add)
						encoder.add(src->value, register_from_operand(dst));
					else
						encoder.sub(src->value, register_from_operand(dst));
				} else if (src->is_constant() && dst->is_alloc_stack()) {
					assert(false);
				} else if (src->is_alloc_reg() && dst->is_alloc_reg()) {
					if (is_an_add)
						encoder.add(register_from_operand(src), register_from_operand(dst));
					else
						encoder.sub(register_from_operand(src), register_from_operand(dst));
				} else if (src->is_alloc_reg() && dst->is_alloc_stack()) {
					if (is_an_add)
						encoder.add(register_from_operand(src), stack_from_operand(dst));
					else
						encoder.sub(register_from_operand(src), stack_from_operand(dst));
				} else if (src->is_alloc_stack() && dst->is_alloc_reg()) {
					if (is_an_add)
						encoder.add(stack_from_operand(src), register_from_operand(dst));
					else
						encoder.sub(stack_from_operand(src), register_from_operand(dst));
				} else if (src->is_alloc_stack() && dst->is_alloc_stack()) {
					X86Register& tmp = get_temp(0, src->size);
					encoder.mov(stack_from_operand(src), tmp);

					if (is_an_add)
						encoder.add(tmp, stack_from_operand(dst));
					else
						encoder.sub(tmp, stack_from_operand(dst));
				} else {
					printf("src:%d, dst:%d\n", src->alloc_mode, dst->alloc_mode);
					assert(false);
				}
			} else {
				if (src->is_constant() && dst->is_alloc_reg()) {
					if (is_an_add)
						encoder.adc(src->value, register_from_operand(dst));
					else
						encoder.sbb(src->value, register_from_operand(dst));
				} else if (src->is_alloc_reg() && dst->is_alloc_reg()) {
					if (is_an_add)
						encoder.adc(register_from_operand(src), register_from_operand(dst));
					else
						encoder.sbb(register_from_operand(src), register_from_operand(dst));
				} else if (src->is_alloc_reg() && dst->is_alloc_stack()) {
					if (is_an_add)
						encoder.adc(register_from_operand(src), stack_from_operand(dst));
					else
						encoder.sbb(register_from_operand(src), stack_from_operand(dst));
				} else if (src->is_alloc_stack() && dst->is_alloc_reg()) {
					if (is_an_add)
						encoder.adc(stack_from_operand(src), register_from_operand(dst));
					else
						encoder.sbb(stack_from_operand(src), register_from_operand(dst));
				} else if (src->is_alloc_stack() && dst->is_alloc_stack()) {
					X86Register& tmp = get_temp(0, src->size);
					encoder.mov(stack_from_operand(src), tmp);

					if (is_an_add)
						encoder.adc(tmp, stack_from_operand(dst));
					else
						encoder.sbb(tmp, stack_from_operand(dst));
				} else {
					assert(false);
				}
			}

			// If it's the flag setting instruction, set the flags.
			if (insn->type == IRInstruction::ADC_WITH_FLAGS || insn->type == IRInstruction::SBC_WITH_FLAGS) {
				if (insn->type == IRInstruction::SBC_WITH_FLAGS)
					encoder.setnc(X86Memory::get(REGSTATE_REG, REG_OFFSET_OF(C))); // Carry
				else
					encoder.setc(X86Memory::get(REGSTATE_REG, REG_OFFSET_OF(C))); // Carry
				encoder.seto(X86Memory::get(REGSTATE_REG, REG_OFFSET_OF(V))); // Overflow
				encoder.setz(X86Memory::get(REGSTATE_REG, REG_OFFSET_OF(Z))); // Zero
				encoder.sets(X86Memory::get(REGSTATE_REG, REG_OFFSET_OF(N))); // Negative
			}

			break;
		}

		case IRInstruction::TRAP:
		{
			encoder.int3();
			break;
		}

		case IRInstruction::TRACE:
		{
			const IROperand& opcode = insn->operands[0];
			assert(opcode.is_constant());

			emit_save_reg_state(6, stack_map);

			load_state_field(0, REG_RDI);
			encoder.mov((uint8_t) opcode.value, REG_RSI);

			if (insn->operands[1].is_valid()) {
				encode_operand_function_argument(&insn->operands[1], REG_RDX, stack_map);
			}

			if (insn->operands[2].is_valid()) {
				encode_operand_function_argument(&insn->operands[2], REG_RCX, stack_map);
			}

			if (insn->operands[3].is_valid()) {
				encode_operand_function_argument(&insn->operands[3], REG_R8, stack_map);
			}

			if (insn->operands[4].is_valid()) {
				encode_operand_function_argument(&insn->operands[4], REG_R9, stack_map);
			}

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t) & jit_trace, get_temp(1, 8));
			encoder.call(get_temp(1, 8));

			emit_restore_reg_state(6, stack_map);

			break;
		}

		default:
			printf("unsupported instruction: %s\n", insn_descriptors[insn->type].mnemonic);
			success = false;
			break;
		}
	}

	for (auto& reloc : block_relocations) {
		int32_t value = native_block_offsets[reloc.second];
		value -= reloc.first;
		value -= 4;

		uint32_t *slot = (uint32_t *) & encoder.get_buffer()[reloc.first];
		*slot = value;
	}

	if (dump_this_shit) {
		asm volatile("out %0, $0xff\n" ::"a"(15), "D"(encoder.get_buffer()), "S"(encoder.get_buffer_size()), "d"(pa));
	}

	return success;
}

void BlockCompiler::emit_save_reg_state(int num_operands, stack_map_t &stack_map) {
	uint32_t stack_offset = 0;
	stack_map.clear();

	//First, count required stack slots
	for (int i = register_assignments_8.size() - 1; i >= 0; i--) {
		if (used_phys_regs.get(i)) {
			stack_offset += 8;
		}
	}

#ifdef CALLER_SAVE_REGSTATE
	encoder.push(REGSTATE_REG);
#endif

	for (int i = register_assignments_8.size() - 1; i >= 0; i--) {
		if (used_phys_regs.get(i)) {
			const X86Register &reg = get_allocable_register(i, 8);

			stack_offset -= 8;
			stack_map[&reg] = stack_offset;
			encoder.push(reg);

		}
	}
}

void BlockCompiler::emit_restore_reg_state(int num_operands, stack_map_t &stack_map) {
	for (unsigned int i = 0; i < register_assignments_8.size(); ++i) {
		if (used_phys_regs.get(i)) {
			encoder.pop(get_allocable_register(i, 8));
		}
	}

#ifdef CALLER_SAVE_REGSTATE
	encoder.pop(REGSTATE_REG);
#endif
}

void BlockCompiler::encode_operand_function_argument(IROperand *oper, const X86Register& target_reg, stack_map_t &stack_map) {
	if (oper->is_constant() == IROperand::CONSTANT) {
		if (oper->value == 0) encoder.xorr(target_reg, target_reg);
		else encoder.mov(oper->value, target_reg);
	} else if (oper->is_vreg()) {
		if (get_allocable_register(oper->alloc_data, 8) == REG_RBX) {
			encoder.mov(REG_RBX, target_reg);
		} else if (oper->is_alloc_reg()) {
			const X86Register &reg = get_allocable_register(oper->alloc_data, 8);
			assert(stack_map.count(&reg));
			encoder.mov(X86Memory::get(REG_RSP, stack_map[&reg]), target_reg);
		} else {
			encoder.mov(X86Memory::get(FRAMEPOINTER_REG, (oper->alloc_data * -1) - 8), target_reg);
		}
	} else {
		assert(false);
	}
}

static void dump_insn(IRInstruction *insn) {
	assert(insn->type < ARRAY_SIZE(insn_descriptors));
	const struct insn_descriptor *descr = &insn_descriptors[insn->type];

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
				printf("i%d $0x%lx", oper->size, oper->value);
			} else if (oper->is_pc()) {
				printf("i4 pc (%08x)", oper->value);
			} else if (oper->is_block()) {
				printf("b%d", oper->value);
			} else if (oper->is_func()) {
				printf("%%%p", (const char *) oper->value);
			} else {
				printf("<invalid>");
			}
		}
	}
}

void BlockCompiler::dump_ir() {
	IRBlockId current_block_id = INVALID_BLOCK_ID;

	for (uint32_t ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);

		if (current_block_id != insn->ir_block) {
			if (insn->ir_block == NOP_BLOCK) continue;
			current_block_id = insn->ir_block;
			printf("block %d:\n", current_block_id);
		}

		dump_insn(insn);

		printf("\n");
	}
}

void BlockCompiler::encode_operand_to_reg(shared::IROperand *operand, const x86::X86Register& reg) {
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

bool BlockCompiler::lower_stack_to_reg() {
	std::map<uint32_t, uint32_t> lowered_entries;
	PopulatedSet<9> avail_phys_regs = used_phys_regs;
	avail_phys_regs.invert();

	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);

		// Don't need to do any clever allocation here since the stack entries should already be re-used

		for (unsigned int op_idx = 0; op_idx < 6; ++op_idx) {
			IROperand &op = insn->operands[op_idx];
			if (!op.is_valid()) break;

			if (op.is_alloc_stack()) {
				int32_t selected_reg;
				if (!lowered_entries.count(op.alloc_data)) {
					if (avail_phys_regs.empty()) continue;

					selected_reg = avail_phys_regs.next_avail();
					avail_phys_regs.clear(selected_reg);
					assert(selected_reg != -1);
					lowered_entries[op.alloc_data] = selected_reg;
					used_phys_regs.set(selected_reg);

				} else {
					selected_reg = lowered_entries[op.alloc_data];
				}
				op.allocate(IROperand::ALLOCATED_REG, selected_reg);
			}
		}
	}

	return true;
}

bool BlockCompiler::constant_prop() {
	std::map<IRRegId, IROperand> constant_operands;

	IRBlockId prev_block = 0;
	//	printf("********* %x\n", pa);
	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);
		if (insn->ir_block == NOP_BLOCK) continue;
		if (prev_block != insn->ir_block) {
			constant_operands.clear();
			//			printf("ZAP!\n");
		}
		prev_block = insn->ir_block;

		//		dump_insn(insn);
		//		printf("\n");

		if (insn->type == IRInstruction::MOV) {
			assert(insn->operands[1].is_vreg());
			if (insn->operands[0].type == IROperand::CONSTANT) {
				constant_operands[insn->operands[1].value] = insn->operands[0];
				//				printf("v%u <= %u\n", insn->operands[1].value, insn->operands[0].value);
				continue;
			}
		}

		if (insn->type == IRInstruction::ADD || insn->type == IRInstruction::SUB) {
			assert(insn->operands[1].is_vreg());
			if (insn->operands[0].type == IROperand::CONSTANT) {
				if (constant_operands.count(insn->operands[1].value)) {
					if (insn->type == IRInstruction::ADD) {
						//						printf("v%u += %u\n", insn->operands[1].value, insn->operands[0].value);
						constant_operands[insn->operands[1].value].value += insn->operands[0].value;
					} else {
						//						printf("v%u -= %u\n", insn->operands[1].value, insn->operands[0].value);
						constant_operands[insn->operands[1].value].value -= insn->operands[0].value;
					}
				}
			}
		}

		struct insn_descriptor &descr = insn_descriptors[insn->type];

		for (unsigned int op_idx = 0; op_idx < 6; ++op_idx) {
			IROperand *operand = &(insn->operands[op_idx]);
			if (descr.format[op_idx] == 'I') {
				if (operand->is_vreg() && constant_operands.count(operand->value)) {
					//					printf("v%u := %u\n", operand->value, constant_operands[operand->value].value);
					*operand = constant_operands[operand->value];
				}
			} else {
				if (operand->is_vreg()) constant_operands.erase(operand->value);
			}
		}
	}

	return true;
}

bool BlockCompiler::reorder_blocks() {
	tick_timer<false> timer;
	timer.reset();

	std::vector<IRBlockId> reordering(ctx.block_count(), NOP_BLOCK);

	std::vector<std::pair<IRBlockId, IRBlockId> > block_targets(ctx.block_count(), {
		NOP_BLOCK, NOP_BLOCK});
	std::set<IRBlockId> blocks;

	timer.tick("Init");

	blocks.insert(0);
	// build a simple cfg
	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);

		switch (insn->type) {
		case IRInstruction::JMP:
			block_targets[insn->ir_block] = {(IRBlockId) insn->operands[0].value, NOP_BLOCK};
			blocks.insert(insn->operands[0].value);
			break;
		case IRInstruction::BRANCH:
			block_targets[insn->ir_block] = {(IRBlockId) insn->operands[1].value, (IRBlockId) insn->operands[2].value};
			blocks.insert(insn->operands[1].value);
			blocks.insert(insn->operands[2].value);
			break;
		case IRInstruction::RET:
		case IRInstruction::DISPATCH:
			block_targets[insn->ir_block] = {NOP_BLOCK, NOP_BLOCK};
			break;
		default:
			break;
		}
	}

	timer.tick("CFG");

	// For each block, figure out the maximum number of predecessors which it has
	std::vector<uint32_t> max_depth(ctx.block_count(), 0);
	std::vector<IRBlockId> work_list;

	work_list.reserve(10);
	work_list.push_back(0);
	while (work_list.size()) {
		IRBlockId block = work_list.back();
		work_list.pop_back();

		if (block == NOP_BLOCK) continue;

		uint32_t my_max_depth = max_depth[block];
		IRBlockId first_target = block_targets[block].first;
		IRBlockId second_target = block_targets[block].second;

		if (first_target != NOP_BLOCK) {
			uint32_t prev_max_depth = max_depth[block_targets[block].first];
			if (prev_max_depth < my_max_depth + 1) {
				max_depth[block_targets[block].first] = my_max_depth + 1;
				work_list.push_back(block_targets[block].first);
			}
		}

		if (second_target != NOP_BLOCK) {
			uint32_t prev_max_depth = max_depth[block_targets[block].second];
			if (prev_max_depth < my_max_depth + 1) {
				max_depth[block_targets[block].second] = my_max_depth + 1;
				work_list.push_back(block_targets[block].second);
			}
		}
	}

	timer.tick("Analyse");

	// Oh god what have I done
	auto comp = [&max_depth] (IRBlockId a, IRBlockId b) -> bool {
		return max_depth[a] < max_depth[b];
	};

	std::vector<IRBlockId> queue;
	queue.insert(queue.begin(), blocks.begin(), blocks.end());
	std::sort(queue.begin(), queue.end(), comp);


	for (unsigned int i = 0; i < queue.size(); ++i) {
		reordering[queue[i]] = i;
	}

	timer.tick("Sort");

	// FINALLY actually assign the new block ordering
	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);
		if (insn->ir_block == NOP_BLOCK) continue;
		insn->ir_block = reordering[insn->ir_block];

		if (insn->type == IRInstruction::JMP) {
			insn->operands[0].value = reordering[insn->operands[0].value];
		}
		if (insn->type == IRInstruction::BRANCH) {
			insn->operands[1].value = reordering[insn->operands[1].value];
			insn->operands[2].value = reordering[insn->operands[2].value];
		}

	}

	ctx.recount_blocks(queue.size());

	timer.tick("Assign");
	timer.dump("Reorder ");

	return true;

#if 0
uh_oh:

	for (int i = 0; i < queue.size(); ++i) {
		printf("%u = %u\n", i, max_depth[i]);
	}

	printf("digraph {\n");
	for (int i = 0; i < queue.size(); ++i) {
		bool has_node = false;
		if (block_targets[i].first != NOP_BLOCK) {
			printf("%u -> %u\n", i, block_targets[i].first);
			has_node = true;
		}
		if (block_targets[i].second != NOP_BLOCK) {
			printf("%u -> %u\n", i, block_targets[i].second);
			has_node = true;
		}

		if (has_node) {
			printf("%u [label=\"%u (%u, %u)\"]\n", i, i, reordering[i], max_depth[i]);
		}
	}
	printf("}");

	assert(false);
#endif
}

bool BlockCompiler::reg_value_reuse() {
	// Reuse values read from registers (rather than reloading from memory)
	// A value becomes live when it is
	// - read from a register into a vreg
	// - written from a vreg to a register
	// A value is killed when the vreg is written to (the value is overwritten)
	//

	// Maps of register offsets to vregs
	std::map<uint32_t, IROperand> offset_to_vreg;
	std::map<IROperand, uint32_t> vreg_to_offset;

	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);
		auto &descr = insn_descriptors[insn->type];

		// First, check to see if the vregs containing any live values have been killed
		for (unsigned int op_idx = 0; op_idx < 6; ++op_idx) {
			const IROperand &op = insn->operands[op_idx];

			// If this operand is written to
			if (descr.format[op_idx] == 'O' || descr.format[op_idx] == 'B') {
				if (vreg_to_offset.count(op)) {
					offset_to_vreg.erase(vreg_to_offset[op]);
					vreg_to_offset.erase(op);
				}
			}
		}

		switch (insn->type) {
		case IRInstruction::JMP:
		case IRInstruction::BRANCH:
		case IRInstruction::READ_MEM:
		case IRInstruction::READ_MEM_USER:
		case IRInstruction::WRITE_MEM:
		case IRInstruction::WRITE_MEM_USER:
		case IRInstruction::LDPC:
		case IRInstruction::INCPC:
		case IRInstruction::CALL:
		case IRInstruction::DISPATCH:
		case IRInstruction::RET:
		case IRInstruction::READ_DEVICE:
		case IRInstruction::WRITE_DEVICE:

			offset_to_vreg.clear();
			vreg_to_offset.clear();
			break;


		case IRInstruction::READ_REG:
		{
			// If this is a register read, make a value live
			IROperand &offset = insn->operands[0];
			IROperand &vreg = insn->operands[1];

			assert(vreg.is_vreg());
			assert(offset.is_constant());

			// XXX ARM HAX don't try and cache the PC or flags
			if (offset.value >= REG_OFFSET_OF(PC)) break;

			if (offset_to_vreg.count(offset.value)) {
				// reuse the already-live value for this instruction
				insn->type = IRInstruction::MOV;
				insn->operands[0] = offset_to_vreg[offset.value];
			} else {
				// mark the value as live
				offset_to_vreg[offset.value] = vreg;
				vreg_to_offset[vreg] = offset.value;
			}

			break;
		}
		case IRInstruction::WRITE_REG:
		{
			// if this is a write to any live regs, kill them
			IROperand &offset = insn->operands[1];
			IROperand &value = insn->operands[0];

			// XXX ARM HAX don't try and cache the PC
			if (offset.value >= REG_OFFSET_OF(PC)) break;

			offset_to_vreg[offset.value] = value;
			if (value.is_vreg()) vreg_to_offset[value] = offset.value;

			break;
		}
		default:
			break;
		}
	}

	return true;
}

bool BlockCompiler::value_merging() {
	// Merge vregs. If a vreg is defined by a mov and never modified, replace all uses of the vreg with the source of
	// the mov. E.g.
	// mov v0, v1
	// mov v1, v2
	// ||
	// \/
	// mov v0, v0
	// mov v0, v2

	std::vector<IRRegId> merged_vregs(ctx.reg_count(), -1);

	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);

		// If this is a move of one vreg to another, record this as a candidate for merging
		if (insn->type == IRInstruction::MOV && insn->operands[0].is_vreg()) {
			if (merged_vregs[insn->operands[0].value] != (IRRegId) - 1)
				merged_vregs[insn->operands[1].value] = merged_vregs[insn->operands[0].value];
			else
				merged_vregs[insn->operands[1].value] = insn->operands[0].value;
		}			// If this is not a move, remove any outputs of the instruction from the merge list
		else {
			auto &descr = insn_descriptors[insn->type];
			for (unsigned int op_idx = 0; op_idx < 6; ++op_idx) {
				if (descr.format[op_idx] == 'O' || descr.format[op_idx] == 'B') {
					if (insn->operands[op_idx].is_vreg()) {
						merged_vregs[insn->operands[op_idx].value] = -1;
					}
				}
			}
		}
	}

	// Apply current merges
	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);
		for (unsigned int op_idx = 0; op_idx < 6; ++op_idx) {
			IROperand &op = insn->operands[op_idx];
			if (op.is_vreg() && merged_vregs[op.value] != (IRRegId) - 1) op.value = merged_vregs[op.value];
		}

		if (insn->type == IRInstruction::MOV) {
			IROperand &op0 = insn->operands[0], &op1 = insn->operands[1];
			if (op0.is_vreg() && op1.is_vreg() && op0.value == op1.value) make_instruction_nop(insn, true);
		}
		//~ dump_insn(insn);
		//~ printf("\n");
	}

	//printf("*** after merge:\n");
	//dump_ir();
	return true;
}
