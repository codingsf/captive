#include <jit/block-compiler.h>
#include <jit/ir-sorter.h>
#include <set>
#include <list>
#include <map>

#include <small-set.h>

extern "C" void cpu_set_mode(void *cpu, uint8_t mode);
extern "C" void cpu_write_device(void *cpu, uint32_t devid, uint32_t reg, uint32_t val);
extern "C" void cpu_read_device(void *cpu, uint32_t devid, uint32_t reg, uint32_t& val);
extern "C" void jit_verify(void *cpu);
extern "C" void jit_rum(void *cpu);

using namespace captive::arch::jit;
using namespace captive::arch::jit::algo;
using namespace captive::arch::x86;
using namespace captive::shared;

/* Register Mapping
 *
 * RAX  Allocatable			0
 * RBX  Temporary
 * RCX  Temporary
 * RDX  Allocatable			1
 * RSI  Allocatable			2
 * RDI  Allocatable			3
 * R8   Allocatable			4
 * R9   Allocatable			5
 * R10  Allocatable			6
 * R11  Not used
 * R12  Not used
 * R13  Not used
 * R14  State variable
 * R15  Register File
 */

BlockCompiler::BlockCompiler(TranslationContext& ctx, gpa_t pa) : ctx(ctx), pa(pa)
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

	//printf("*** %x\n", pa);

	//dump_ir();

	if (!thread_jumps()) return false;
	if (!dbe()) return false;
	if (!merge_blocks()) return false;
	if (!sort_ir()) return false;
	if (!peephole()) return false;
	if (!sort_ir()) return false;
	if (!analyse(max_stack)) return false;
	if (!sort_ir()) return false;
	if (!allocate()) return false;

	if( !post_allocate_peephole()) return false;
	
	if (!lower(max_stack)) {
		encoder.destroy_buffer();
		return false;
	}

	fn = (block_txln_fn)encoder.get_buffer();
	return true;
}

bool BlockCompiler::sort_ir()
{
	MergeSort sorter(ctx);
	return sorter.sort();
}

// If this is an add of a negative value, replace it with a subtract of a positive value
static bool peephole_add(IRInstruction &insn)
{
	IROperand &op1 = insn.operands[0];
	if(op1.is_constant()) {
		uint64_t unsigned_negative = 0;
		switch(op1.size) {
			case 1: unsigned_negative = 0x80; break;
			case 2: unsigned_negative = 0x8000; break;
			case 4: unsigned_negative = 0x80000000; break;
			case 8: unsigned_negative = 0x8000000000000000; break;
			default: assert(false);
		}

		if(op1.value >= unsigned_negative) {
			insn.type = IRInstruction::SUB;
			switch(op1.size) {
				case 1: op1.value = -(int8_t)op1.value; break;
				case 2: op1.value = -(int16_t)op1.value; break;
				case 4: op1.value = -(int32_t)op1.value; break;
				case 8: op1.value = -(int64_t)op1.value; break;
				default: assert(false);
			}
		}
	}
	return true;
}

// If we shift an n-bit value by n bits, then replace this instruction with a move of 0.
// We explicitly move in 0, rather than XORing out, in order to avoid creating spurious use-defs
bool peephole_shift(IRInstruction &insn)
{
	IROperand &op1 = insn.operands[0];
	IROperand &op2 = insn.operands[1];
	if(op1.is_constant()) {
		bool zeros_out = false;
		switch(op2.size) {
			case 1: zeros_out = op1.value == 8; break;
			case 2: zeros_out = op1.value == 16; break;
			case 4: zeros_out = op1.value == 32; break;
			case 8: zeros_out = op1.value == 64; break;
		}

		if(zeros_out) {
			insn.type = IRInstruction::MOV;
			op1.type = IROperand::CONSTANT;
			op1.value = 0;
		}
	}

	return true;
}

static void make_instruction_nop(IRInstruction *insn, bool set_block)
{
	insn->type = IRInstruction::NOP;
	insn->operands[0].type = IROperand::NONE;
	insn->operands[1].type = IROperand::NONE;
	insn->operands[2].type = IROperand::NONE;
	insn->operands[3].type = IROperand::NONE;
	insn->operands[4].type = IROperand::NONE;
	insn->operands[5].type = IROperand::NONE;
	if(set_block) insn->ir_block = 0x7fffffff;
}

// Perform some basic optimisations
bool BlockCompiler::peephole()
{
	IRInstruction *prev_pc_inc = NULL;
	IRBlockId prev_block = INVALID_BLOCK_ID;
	
	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);

		if(insn->ir_block != prev_block) {
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
		case IRInstruction::OR:
		case IRInstruction::XOR:
			if (insn->operands[0].is_constant() && insn->operands[0].value == 0) {
				make_instruction_nop(insn, true);
			}
			break;
			
			
		case IRInstruction::INCPC:
			if(prev_pc_inc) {
				insn->operands[0].value += prev_pc_inc->operands[0].value;
				prev_pc_inc->type = IRInstruction::NOP;
			}
			prev_pc_inc = insn;
						
			break;
			
		case IRInstruction::READ_REG:
			// HACK HACK HACK (pc offset)
			if(insn->operands[0].value == 0x3c) {
				prev_pc_inc = NULL;
			}
			break;
			
		case IRInstruction::JMP:
		case IRInstruction::BRANCH:
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
			
		default:
			break;
		}

	}

	return true;
}

static bool is_mov_nop(IRInstruction *insn)
{
	bool is_nop = (insn->type == IRInstruction::MOV) && (insn->operands[0].alloc_mode == insn->operands[1].alloc_mode) && (insn->operands[0].alloc_data == insn->operands[1].alloc_data);
	if(is_nop)make_instruction_nop(insn, false);
	return is_nop;
}

bool BlockCompiler::post_allocate_peephole()
{
	uint32_t this_insn_idx = 0;
	uint32_t next_insn_idx = 0;

	while(true) {
		IRInstruction *insn;
		IRInstruction *next_insn;
		
		do {
			insn = ctx.at(this_insn_idx);
			this_insn_idx++;
			if(this_insn_idx >= ctx.count()) goto exit;
			
		} while(is_mov_nop(insn));
		
		if(next_insn_idx <= this_insn_idx) next_insn_idx = this_insn_idx+1;
		
		do {
			next_insn = ctx.at(next_insn_idx);
			next_insn_idx++;
			if(next_insn_idx >= ctx.count()) goto exit;
		} while(is_mov_nop(next_insn));
		
		// If we have an add, then a memory operation
		// If the add is of a constant to reg A, and the memory op is from and to reg A
		// Then nop out the add and add the add's source to the mem op's displacement
		// e.g.
		// add $0x10, %eax
		// mov (%eax), %eax
		// becomes
		// mov $0x10(%eax), %eax
		
		if(insn->ir_block != next_insn->ir_block) continue;
		
		// if we have an add...
		if(insn->type == IRInstruction::ADD || insn->type == IRInstruction::SUB)
		{
			// then a memory op...
			if(next_insn->type == IRInstruction::READ_MEM || next_insn->type == IRInstruction::WRITE_MEM)
			{
				IRInstruction *add = insn;
				IRInstruction *mem = next_insn;
				// if the add is of a constant to reg A...
				if(add->operands[0].is_constant() && add->operands[1].is_alloc_reg())
				{
					// and the memory op is from and to reg A
					uint16_t reg_a_data = add->operands[1].alloc_data; 
					if(mem->operands[0].is_alloc_reg() && mem->operands[2].is_alloc_reg()) {
						if((mem->operands[0].alloc_data == reg_a_data) && mem->operands[2].alloc_data == reg_a_data) {
							// then nop out the add and add the add's source to the mem op's displacement
							assert(mem->operands[1].is_constant());
							
							if(insn->type == IRInstruction::ADD)
								mem->operands[1].value += add->operands[0].value;
							else 
								mem->operands[1].value -= add->operands[0].value;
							make_instruction_nop(add, false);
							
							this_insn_idx = next_insn_idx;
							next_insn_idx++;
							
						}
					}
				}
			}
		} else {
			// If this instruction isn't an add or subtract, just skip these instructions
			// TODO: do this optimisation more intelligently since we miss quite a few opportunities caused by this
						
			this_insn_idx++;
			next_insn_idx = this_insn_idx;
		}
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
	{ .mnemonic = "invalid",	.format = "XXXXXX", .has_side_effects = false },
	{ .mnemonic = "verify",		.format = "NXXXXX", .has_side_effects = true },
	{ .mnemonic = "count",		.format = "NNXXXX", .has_side_effects = true },

	{ .mnemonic = "nop",		.format = "XXXXXX", .has_side_effects = false },
	{ .mnemonic = "trap",		.format = "XXXXXX", .has_side_effects = true },

	{ .mnemonic = "mov",		.format = "IOXXXX", .has_side_effects = false },
	{ .mnemonic = "cmov",		.format = "XXXXXX", .has_side_effects = false },
	{ .mnemonic = "ldpc",		.format = "OXXXXX", .has_side_effects = false },
	{ .mnemonic = "inc-pc",		.format = "IXXXXX", .has_side_effects = true },

	{ .mnemonic = "add",		.format = "IBXXXX", .has_side_effects = false },
	{ .mnemonic = "sub",		.format = "IBXXXX", .has_side_effects = false },
	{ .mnemonic = "mul",		.format = "IBXXXX", .has_side_effects = false },
	{ .mnemonic = "div",		.format = "IBXXXX", .has_side_effects = false },
	{ .mnemonic = "mod",		.format = "IBXXXX", .has_side_effects = false },

	{ .mnemonic = "shl",		.format = "IBXXXX", .has_side_effects = false },
	{ .mnemonic = "shr",		.format = "IBXXXX", .has_side_effects = false },
	{ .mnemonic = "sar",		.format = "IBXXXX", .has_side_effects = false },
	{ .mnemonic = "clz",		.format = "IOXXXX", .has_side_effects = false },

	{ .mnemonic = "and",		.format = "IBXXXX", .has_side_effects = false },
	{ .mnemonic = "or",		.format = "IBXXXX", .has_side_effects = false },
	{ .mnemonic = "xor",		.format = "IBXXXX", .has_side_effects = false },

	{ .mnemonic = "cmp eq",		.format = "IIOXXX", .has_side_effects = false },
	{ .mnemonic = "cmp ne",		.format = "IIOXXX", .has_side_effects = false },
	{ .mnemonic = "cmp gt",		.format = "IIOXXX", .has_side_effects = false },
	{ .mnemonic = "cmp gte",	.format = "IIOXXX", .has_side_effects = false },
	{ .mnemonic = "cmp lt",		.format = "IIOXXX", .has_side_effects = false },
	{ .mnemonic = "cmp lte",	.format = "IIOXXX", .has_side_effects = false },

	{ .mnemonic = "mov sx",		.format = "IOXXXX", .has_side_effects = false },
	{ .mnemonic = "mov zx",		.format = "IOXXXX", .has_side_effects = false },
	{ .mnemonic = "mov trunc",	.format = "IOXXXX", .has_side_effects = false },

	{ .mnemonic = "ldreg",		.format = "IOXXXX", .has_side_effects = false },
	{ .mnemonic = "streg",		.format = "IIXXXX", .has_side_effects = true },
	{ .mnemonic = "ldmem",		.format = "IIOXXX", .has_side_effects = true },
	{ .mnemonic = "stmem",		.format = "IIIXXX", .has_side_effects = true },
	{ .mnemonic = "ldumem",		.format = "IOXXXX", .has_side_effects = true },
	{ .mnemonic = "stumem",		.format = "IIXXXX", .has_side_effects = true },

	{ .mnemonic = "call",		.format = "NIIIII", .has_side_effects = true },
	{ .mnemonic = "jmp",		.format = "NXXXXX", .has_side_effects = true },
	{ .mnemonic = "branch",		.format = "INNXXX", .has_side_effects = true },
	{ .mnemonic = "ret",		.format = "XXXXXX", .has_side_effects = true },
	{ .mnemonic = "dispatch",	.format = "NNXXXX", .has_side_effects = true },

	{ .mnemonic = "scm",		.format = "IXXXXX", .has_side_effects = true },
	{ .mnemonic = "stdev",		.format = "IIIXXX", .has_side_effects = true },
	{ .mnemonic = "lddev",		.format = "IIOXXX", .has_side_effects = false },

	{ .mnemonic = "flush",		.format = "XXXXXX", .has_side_effects = true },
	{ .mnemonic = "flush itlb",	.format = "XXXXXX", .has_side_effects = true },
	{ .mnemonic = "flush dtlb",	.format = "XXXXXX", .has_side_effects = true },
	{ .mnemonic = "flush itlb",	.format = "IXXXXX", .has_side_effects = true },
	{ .mnemonic = "flush dtlb",	.format = "IXXXXX", .has_side_effects = true },

	{ .mnemonic = "adc flags",	.format = "IIIOXX", .has_side_effects = false },
	
	{ .mnemonic = "barrier",	.format = "XXXXXX", .has_side_effects = true },
};

bool BlockCompiler::analyse(uint32_t& max_stack)
{
	IRBlockId latest_block_id = INVALID_BLOCK_ID;

	used_phys_regs.clear();

	std::map<IRRegId, uint32_t> allocation;			// local register allocation
	std::map<IRRegId, uint32_t> global_allocation;	// global register allocation
	std::set<IRRegId> live_ins, live_outs;
	
	PopulatedSet<8> avail_regs; // Register indicies that are available for allocation.

	uint32_t next_global = 0;	// Next stack location for globally allocated register.

	std::map<IRRegId, IRBlockId> vreg_seen_block;

	//printf("allocating for 0x%08x over %u vregs\n", pa, ctx.reg_count());

	// Build up a map of which vregs have been seen in which blocks, to detect spanning vregs.
	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);
		
		if(insn->type == IRInstruction::BARRIER) next_global = 0;
		
		for (int op_idx = 0; op_idx < 6; op_idx++) {
			IROperand *oper = &insn->operands[op_idx];

			// If the operand is a vreg, and is not already a global...
			if (oper->is_vreg() && (global_allocation.count(oper->value) == 0)) {
				auto seen_in_block = vreg_seen_block.find(oper->value);
				
				// If we have already seen this operand, and not in the same block, then we
				// must globally allocate it.
				if (seen_in_block != vreg_seen_block.end() && seen_in_block->second != insn->ir_block) {
					global_allocation[oper->value] = next_global;
					next_global += 8;
					if(next_global > max_stack) max_stack = next_global;
				}
				
				vreg_seen_block[oper->value] = insn->ir_block;
			}
		}
	}

	for (int ir_idx = ctx.count() - 1; ir_idx >= 0; ir_idx--) {
		// Grab a pointer to the instruction we're looking at.
		IRInstruction *insn = ctx.at(ir_idx);

		// Make sure it has a descriptor.
		assert(insn->type < ARRAY_SIZE(insn_descriptors));
		const struct insn_descriptor *descr = &insn_descriptors[insn->type];

		// Detect a change in block
		if (latest_block_id != insn->ir_block) {
			// Clear the live-in working set and current allocations.
			live_ins.clear();
			allocation.clear();

			// Reset the available register bitfield
			avail_regs.fill(0x7f);
			
			// Update the latest block id.
			latest_block_id = insn->ir_block;
			//printf("block %d:\n", latest_block_id);
		}

		if(insn->type == IRInstruction::BARRIER) {
			next_global = 0;			
		}

		// Clear the live-out set, and make every current live-in a live-out.
		live_outs.clear();
		for (auto in : live_ins) {
			live_outs.insert(in);
		}
		
		// Loop over the VREG operands and update the live-in set accordingly.
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

		// Release LIVE-OUTs that are not LIVE-INs
		for (auto out : live_outs) {
			if(global_allocation.count(out)) continue;
			if (!live_ins.count(out)) {
				auto alloc = allocation.find(out);
				assert(alloc != allocation.end());

				// Make the released register available again.
				avail_regs.set(alloc->second);
			}
		}

		// Allocate LIVE-INs
		for (auto in : live_ins) {
			// If the live-in is not already allocated, allocate it.
			if (allocation.count(in) == 0 && global_allocation.count(in) == 0) {
				int32_t next_reg = avail_regs.next_avail();
				
				if (avail_regs.next_avail() == -1) {
					global_allocation[in] = next_global;
					next_global += 8;
					if(max_stack < next_global) max_stack = next_global;
					
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

//		printf("About to fuck up %10s\n", insn_descriptors[insn->type].mnemonic);
		// Loop over operands to update the allocation information on VREG operands.
		for (int op_idx = 0; op_idx < 6; op_idx++) {
			IROperand *oper = &insn->operands[op_idx];

			if (oper->is_vreg()) {
				if (descr->format[op_idx] != 'I' && live_outs.count(oper->value)) not_dead = true;

				// If this vreg has been allocated to the stack, then fill in the stack entry location here
				auto global_alloc = global_allocation.find(oper->value);
				if(global_alloc != global_allocation.end()) {
					oper->allocate(IROperand::ALLOCATED_STACK, global_alloc->second);
					if(descr->format[op_idx] == 'O' || descr->format[op_idx] == 'B') not_dead = true;
				} else {

					// Otherwise, if the value has been locally allocated, fill in the local allocation
					auto alloc_reg = allocation.find((IRRegId)oper->value);

	//				printf("Operand %u (%u) is %c and is live out:%u\n", op_idx, oper->value, descr->format[op_idx], live_outs.count(oper->value));

					if (alloc_reg != allocation.end()) {
						oper->allocate(IROperand::ALLOCATED_REG, alloc_reg->second);
					}

				}
			}
		}

		// Remove allocations that are not LIVE-INs
		for (auto out : live_outs) {
			if(global_allocation.count(out)) continue;
			if (!live_ins.count(out)) {
				auto alloc = allocation.find(out);
				assert(alloc != allocation.end());

				allocation.erase(alloc);
			}
		}

		// If this instruction is dead, remove any live ins which are not live outs
		// in order to propagate dead instruction elimination information
		if(!not_dead && can_be_dead) {
			make_instruction_nop(insn, true);

			auto old_live_ins = live_ins;
			for(auto in : old_live_ins) {
				if(global_allocation.count(in)) continue;
				if(live_outs.count(in) == 0) {
					//printf("Found a live in that's not a live out in a dead instruction r%d \n", in);
					auto alloc = allocation.find(in);
					assert(alloc != allocation.end());

					avail_regs.set(alloc->second);

					allocation.erase(alloc);

					live_ins.erase(in);
				}
			}
		}

//		printf("  [%03d] %10s\n", ir_idx, insn_descriptors[insn->type].mnemonic);

#if 0
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
		printf(" }\n");
#endif
	}

	//printf("block %08x\n", tb.block_addr);
//	dump_ir();

	return true;
}

bool BlockCompiler::thread_jumps()
{
	std::map<IRBlockId, IRInstruction *> first_instructions;
	std::map<IRBlockId, IRInstruction *> last_instructions;

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

	for (auto block : last_instructions) {
		IRInstruction *source_instruction = block.second;

		switch(source_instruction->type) {
		case IRInstruction::JMP: {
			IRInstruction *target_instruction = first_instructions[block.second->operands[0].value];

			while (target_instruction->type == IRInstruction::JMP) {
				target_instruction = first_instructions[target_instruction->operands[0].value];
			}

			if (target_instruction->type == IRInstruction::RET) {
				*source_instruction = *target_instruction;
				source_instruction->ir_block = block.first;
			} else if (target_instruction->type == IRInstruction::BRANCH) {
				*source_instruction = *target_instruction;
				source_instruction->ir_block = block.first;
				goto do_branch_thread;
			} else if (target_instruction->type == IRInstruction::DISPATCH) {
				*source_instruction = *target_instruction;
				source_instruction->ir_block = block.first;
			} else {
				source_instruction->operands[0].value = target_instruction->ir_block;
			}

			break;
		}

		case IRInstruction::BRANCH: {
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
			assert(false);
		}
	}

	return true;
}

bool BlockCompiler::dbe()
{
	block_list_t blocks, exits;
	cfg_t succs, preds;
	std::set<IRBlockId> to_die;

	if (!build_cfg(blocks, succs, preds, exits))
		return false;

	for (auto block : blocks) {
		if (block == 0) continue;

		if (preds[block].size() == 0) {
			to_die.insert(block);
		}
	}

	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);

		if (to_die.count(insn->ir_block) != 0) {
			make_instruction_nop(insn, true);
			insn->ir_block = 0;
		}
	}

	return true;
}

bool BlockCompiler::merge_blocks()
{
	block_list_t blocks, exits;
	cfg_t succs, preds;

	if (!build_cfg(blocks, succs, preds, exits))
		return false;

start:
	for(auto block : blocks) {
		if(succs[block].size() == 1) {
			IRBlockId successor = succs[block][0];
			
			if(preds[successor].size() == 1) {
				if(merge_block(successor, block)){
					succs[block] = succs[successor];
					preds[successor].clear();
					succs[successor].clear();
					goto start;
				}
			}
		}
		
	}
	
	return true;
}

bool BlockCompiler::merge_block(IRBlockId merge_from, IRBlockId merge_into)
{
	// Don't try to merge 'backwards' yet since it's a bit more complicated
	if(merge_from < merge_into) return false;
	
	// Nop out the terminator instruction from the merge_into block
	for(unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);
		
		// We can only merge if the terminator is a jmp
		if(insn->ir_block == merge_into && insn->type == IRInstruction::JMP) {
			make_instruction_nop(insn, true);
			break;
		}
	}
	
	for(unsigned int ir_idx = 0; ir_idx < ctx.count(); ++ir_idx) {
		IRInstruction *insn = ctx.at(ir_idx);
		
		// Move instructions from the 'from' block to the 'to' block
		if(insn->ir_block == merge_from) insn->ir_block = merge_into;
	}
	return true;
}

bool BlockCompiler::build_cfg(block_list_t& blocks, cfg_t& succs, cfg_t& preds, block_list_t& exits)
{
	IRBlockId current_block_id = INVALID_BLOCK_ID;

	for (unsigned int ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);

		if (insn->ir_block != current_block_id) {
			current_block_id = insn->ir_block;
			blocks.push_back(current_block_id);
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

	encoder.mov(REG_RDI, REG_R14);	// JIT state ptr
	load_state_field(8, REG_R15);	// Register state ptr

	IRBlockId current_block_id = INVALID_BLOCK_ID;
	for (uint32_t ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);

		// Record the native offset of an IR block
		if (current_block_id != insn->ir_block) {
			current_block_id = insn->ir_block;
			native_block_offsets[current_block_id] = encoder.current_offset();
		}

		switch (insn->type) {
		case IRInstruction::BARRIER:
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

		case IRInstruction::DISPATCH:
		case IRInstruction::RET:
		{
			// Function Epilogue
			encoder.pop(REG_RBX);
			encoder.pop(REG_R14);
			encoder.pop(REG_R15);
			encoder.xorr(REG_EAX, REG_EAX);
			encoder.leave();
			encoder.ret();
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
					// TODO: FIXME: HACK HACK HACK XXX
					switch (insn->type) {
					case IRInstruction::ADD:
						encoder.add(X86Memory::get(REG_R15, 60), register_from_operand(dest));
						break;
					case IRInstruction::SUB:
						encoder.sub(X86Memory::get(REG_R15, 60), register_from_operand(dest));
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

			IRInstruction *next_insn = (ir_idx + 1) < ctx.count() ? ctx.at(ir_idx + 1) : NULL;

			if (next_insn && next_insn->ir_block == (IRBlockId)target->value) {
				// The next instruction is in the block we're about to jump to, so we can
				// leave this as a fallthrough.
			} else {
				// Create a relocated jump instruction, and store the relocation offset and
				// target into the relocations list.
				uint32_t reloc_offset;
				encoder.jmp_reloc(reloc_offset);

				block_relocations[reloc_offset] = target->value;

				encoder.align_up(16);
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

			IRInstruction *next_insn = (ir_idx + 1) < ctx.count() ? ctx.at(ir_idx + 1) : NULL;

			if (next_insn && next_insn->ir_block == (IRBlockId)tt->value) {
				// Fallthrough is TRUE block
				{
					uint32_t reloc_offset;
					encoder.jz_reloc(reloc_offset);
					block_relocations[reloc_offset] = ft->value;
				}
			} else if (next_insn && next_insn->ir_block == (IRBlockId)ft->value) {
				// Fallthrough is FALSE block
				{
					uint32_t reloc_offset;
					encoder.jnz_reloc(reloc_offset);
					block_relocations[reloc_offset] = tt->value;
				}
			} else {
				// Fallthrough is NEITHER
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
				
				encoder.align_up(16);
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
					emit_save_reg_state(insn->count_operands());
				}
			} else {
				emit_save_reg_state(insn->count_operands());
			}

			//emit_save_reg_state();

			// DI, SI, DX, CX, R8, R9
			const X86Register *sysv_abi[] = { &REG_RDI, &REG_RSI, &REG_RDX, &REG_RCX, &REG_R8, &REG_R9 };

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

			if (next_insn) {
				if (next_insn->type == IRInstruction::CALL && insn->count_operands() == next_insn->count_operands()) {
					// Don't restore the state, because the next instruction is a call and it will use it.
				} else {
					emit_restore_reg_state(insn->count_operands());
				}
			} else {
				emit_restore_reg_state(insn->count_operands());
			}

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
			IROperand *disp = &insn->operands[1];
			IROperand *dest = &insn->operands[2];

			assert(disp->is_constant());

			if (offset->is_vreg()) {
				if (offset->is_alloc_reg() && dest->is_alloc_reg()) {
					// mov (reg), reg
					encoder.mov(X86Memory::get(register_from_operand(offset), disp->value), register_from_operand(dest));
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
			IROperand *disp = &insn->operands[1];
			IROperand *offset = &insn->operands[2];

			assert(disp->is_constant());

			if (offset->is_vreg()) {
				if (value->is_vreg()) {
					if (offset->is_alloc_reg() && value->is_alloc_reg()) {
						// mov reg, (reg)

						encoder.mov(register_from_operand(value), X86Memory::get(register_from_operand(offset), disp->value));
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
			encoder.nop();				// JIC

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
			encoder.nop();				// JIC

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
				emit_save_reg_state(4);
			}

			load_state_field(0, REG_RDI);

			encode_operand_function_argument(dev, REG_RSI);
			encode_operand_function_argument(reg, REG_RDX);
			encode_operand_function_argument(val, REG_RCX);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&cpu_write_device, get_temp(0, 8));
			encoder.call(get_temp(0, 8));

			if (next_insn && next_insn->type == IRInstruction::WRITE_DEVICE) {
				//
			} else {
				emit_restore_reg_state(4);
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

			emit_save_reg_state(4);

			load_state_field(0, REG_RDI);

			encode_operand_function_argument(dev, REG_RSI);
			encode_operand_function_argument(reg, REG_RDX);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&cpu_read_device, get_temp(0, 8));
			encoder.call(get_temp(0, 8));

			emit_restore_reg_state(4);

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
						encoder.andd(source->value, stack_from_operand(dest));
						break;
					case IRInstruction::OR:
						encoder.orr(source->value, stack_from_operand(dest));
						break;
					case IRInstruction::XOR:
						encoder.xorr(source->value, stack_from_operand(dest));
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
							encoder.movsx(register_from_operand(source), register_from_operand(dest));
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

			bool should_branch_instead = false;

			IRInstruction *next_insn = (ir_idx + 1) < ctx.count() ? ctx.at(ir_idx + 1) : NULL;

			// TODO: Look at this optimisation
			/*if (next_insn && next_insn->type == IRInstruction::BRANCH) {
				// If the next instruction is a branch, we need to check to see if it's branching on
				// this condition, before we go ahead an emit an optimised form.

				if (next_insn->operands[0].is_vreg() && next_insn->operands[0].value == dest->value) {
					// Right here we go, we've got a compare-and-branch situation.

					// Skip the next instruction (which is the branch)
					i++;

					// Set the do-a-branch-instead flag
					should_branch_instead = true;
				}
			}*/

			switch (insn->type) {
			case IRInstruction::CMPEQ:
				if (should_branch_instead) {
					assert(dest->is_alloc_reg());

					IROperand *tt = &next_insn->operands[1];
					IROperand *ft = &next_insn->operands[2];

					{
						uint32_t reloc_offset;
						encoder.je_reloc(reloc_offset);
						block_relocations[reloc_offset] = tt->value;
					}

					{
						uint32_t reloc_offset;
						encoder.jmp_reloc(reloc_offset);
						block_relocations[reloc_offset] = ft->value;
					}
				} else {
					encoder.sete(register_from_operand(dest));
				}
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
					default:
						assert(false);
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

		case IRInstruction::VERIFY:
		{
			encoder.nop(X86Memory::get(REG_RAX, insn->operands[0].value));

			emit_save_reg_state(1);

			load_state_field(0, REG_RDI);

			// Load the address of the target function into a temporary, and perform an indirect call.
			encoder.mov((uint64_t)&jit_verify, get_temp(0, 8));
			encoder.call(get_temp(0, 8));

			emit_restore_reg_state(1);

			break;
		}

		case IRInstruction::COUNT:
		{
			X86Register& tmp = get_temp(0, 8);

			load_state_field(32, tmp);
			encoder.add8(1, tmp);

			break;
		}

		case IRInstruction::FLUSH:
		case IRInstruction::FLUSH_ITLB:
		case IRInstruction::FLUSH_DTLB:
		{
			encoder.mov(1, REG_EBX);
			encoder.intt(0x85);
			break;
		}

		case IRInstruction::FLUSH_ITLB_ENTRY:
		case IRInstruction::FLUSH_DTLB_ENTRY:
		{
			if (insn->operands[0].is_constant()) {
				encoder.mov(insn->operands[0].value, REG_RCX);
			} else if (insn->operands[0].is_vreg()) {
				if (insn->operands[0].is_alloc_reg()) {
					const X86Register& addr = register_from_operand(&insn->operands[0], 4);
					encoder.mov(addr, REG_ECX);
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			encoder.mov(4, REG_EBX);
			encoder.intt(0x85);
			break;
		}

		case IRInstruction::ADC_WITH_FLAGS:
		{
			bool dest_is_rax = register_from_operand(&insn->operands[3], 8) == REG_RAX;
			if (!dest_is_rax) {
				encoder.push(REG_RAX);
			}

			IROperand *lhs = &insn->operands[0];
			IROperand *rhs = &insn->operands[1];
			IROperand *carry_in = &insn->operands[2];
			IROperand *flags_out = &insn->operands[3];

			assert(flags_out->is_vreg());
			
			X86Register& tmp = get_temp(0, 4);

			// Load up lhs
			if (lhs->is_constant()) {
				encoder.mov(lhs->value, tmp);
			} else if (lhs->is_vreg()) {
				if (lhs->is_alloc_reg()) {
					encoder.mov(register_from_operand(lhs, 4), tmp);
				} else if (lhs->is_alloc_stack()) {
					encoder.mov(stack_from_operand(lhs), tmp);
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			// Set up carry bit for adc
			if (carry_in->is_constant()) {
				if (carry_in->value == 0) {
					encoder.clc();
				} else {
					encoder.stc();
				}
			} else if (carry_in->is_vreg()) {
				encoder.mov(0xff, get_temp(1, 1));
				if (carry_in->is_alloc_reg()) {
					encoder.add(register_from_operand(carry_in, 1), get_temp(1, 1));
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			// Perform add with carry to set the flags
			if (rhs->is_constant()) {
				encoder.adc((uint32_t)rhs->value, tmp);
			} else if (rhs->is_vreg()) {
				if (rhs->is_alloc_reg()) {
					encoder.adc(register_from_operand(rhs, 4), tmp);
				} else if (rhs->is_alloc_stack()) {
					encoder.adc(stack_from_operand(rhs), tmp);
				} else {
					assert(false);
				}
			} else {
				assert(false);
			}

			// Read flags out into AX
			// What are you lahfing at?
			encoder.lahf();
			encoder.seto(REG_AL);

			//Move AX into correct destination register
			if (!dest_is_rax) {
				if (flags_out->is_alloc_reg()) {
					encoder.mov(REG_AX, register_from_operand(flags_out, 2));
				} else {
					assert(false);
				}
				
				encoder.pop(REG_RAX);
			}

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

	//asm volatile("out %0, $0xff\n" :: "a"(15), "D"(encoder.get_buffer()), "S"(encoder.get_buffer_size()), "d"(pa));
	return success;
}

void BlockCompiler::emit_save_reg_state(int num_operands)
{
	encoder.push(REG_RSI);
	encoder.push(REG_RDI);
	encoder.push(REG_RAX);
	encoder.push(REG_RDX);
	encoder.push(REG_R8);
	encoder.push(REG_R9);
	encoder.push(REG_R10);
}

void BlockCompiler::emit_restore_reg_state(int num_operands)
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
		if(oper->value == 0) encoder.xorr(target_reg, target_reg);
		else encoder.mov(oper->value, target_reg);
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
			encoder.mov(X86Memory::get(REG_RBP, (oper->alloc_data * -1) - 8), target_reg);
		}
	} else {
		assert(false);
	}
}

void BlockCompiler::dump_ir()
{
	IRBlockId current_block_id = INVALID_BLOCK_ID;

	for (uint32_t ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		IRInstruction *insn = ctx.at(ir_idx);

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
					printf("i%d $0x%lx", oper->size, oper->value);
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
