#include <jit/block-compiler.h>
#include <jit/ir-sorter.h>

#include <set>
#include <list>

using namespace captive::arch::jit;
using namespace captive::arch::jit::algo;
using namespace captive::arch::x86;
using namespace captive::shared;

typedef std::set<IRRegId> reg_set_t;
typedef std::set<IRBlockId> block_set_t;
typedef std::map<IRBlockId, block_set_t> block_relation_map_t;
typedef std::map<IRBlockId, reg_set_t> block_regs_t;
typedef enum _use_def_type {
	USE,
	DEF
} use_def_type_t;
typedef std::pair<use_def_type_t, IRRegId> use_def_t;
typedef std::list<use_def_t> use_def_list_t;
typedef std::map<IRBlockId, use_def_list_t> block_use_def_t;

typedef struct _verify_context {
	block_set_t blocks;
	block_relation_map_t block_succesors;
	block_relation_map_t block_predecessors;

	block_use_def_t usedefs;

	inline void add_use(IRBlockId block, const IROperand& op) { if (op.is_vreg()) usedefs[block].push_back(use_def_t(USE, op.value)); }
	inline void add_def(IRBlockId block, const IROperand& op) { assert(op.is_vreg()); usedefs[block].push_back(use_def_t(DEF, op.value)); }
} verify_context_t;

static bool compute_path_walk(verify_context_t& vctx, int& counter, IRBlockId b)
{
	auto& succs = vctx.block_succesors[b];
	if (succs.size() == 0) {
		counter++;
		if (counter > 100) {
			return false;
		}
	} else {
		for (auto succ : succs) {
			if (!compute_path_walk(vctx, counter, succ))
				return false;
		}
	}
	
	return true;
}

static bool too_many_paths(verify_context_t& vctx)
{
	int counter = 0;
	return !compute_path_walk(vctx, counter, 0);
}

static bool has_operands(const IRInstruction& insn, int count)
{
	int i, counted = 0;
	bool is_now_invalid = false;
	for (i = 0; i < 6; i++) {
		if (insn.operands[i].is_valid()) {
			assert(!is_now_invalid);
			counted++;
		} else {
			is_now_invalid = true;
		}
	}

	if (count != counted) {
		printf("verifier: incorrect number of operands to IR node (type=%d), expected=%d, got=%d\n", insn.type, count, counted);
		return false;
	}
	
	return true;
}

static bool walk_usedef(verify_context_t& ctx, IRBlockId block, std::set<IRBlockId>& path, std::set<IRRegId> seen_defs)
{
	// Detect cyclic control flow (which, incidentally, is illegal)
	if (path.count(block)) {
		printf("verification failed: cyclic control flow\n");
		return false;
	}
	
	// Update the current path set
	path.insert(block);
	
	// Enumerate over the use-defs in order.
	for (auto usedef : ctx.usedefs[block]) {
		// If this is a use, then we MUST already have a valid def of the register, either by a def in a parent
		// block, or by a def in this block.
		if (usedef.first == USE) {
			// No previous defs?  Verification failure.
			if (seen_defs.count(usedef.second) == 0) {
				printf("verification failed: use of register %d before def in block %d\n", usedef.second, block);
				return false;
			}
		} else {
			// If this is a def, then pop it into the seen defs set.
			seen_defs.insert(usedef.second);
		}
	}
	
	// Recursively traverse over successors, passing in a reference to the current path set,
	// and COPYING in the current list of seen defs.
	for (auto succ : ctx.block_succesors[block]) {
		if (!walk_usedef(ctx, succ, path, seen_defs)) return false;
	}
	
	// We're done in this block now.  Remove it from the current path set.
	path.erase(block);
	return true;
}

#define ABORT_IF(__cond, __msg, ...) do { if (__cond) { printf("verification failure: " __msg, ## __VA_ARGS__); return false; } } while(0)
#define CHECK_OPERANDS(__nr) ABORT_IF(!has_operands(*insn, __nr), "invalid operand count\n")
#define ADD_USE(__oper) vctx.add_use(insn->ir_block, __oper)
#define ADD_DEF(__oper) vctx.add_def(insn->ir_block, __oper)

bool BlockCompiler::verify()
{
	verify_context_t vctx;

	sort_ir();

	IRBlockId current_block = INVALID_BLOCK_ID;
	
	// Build a control-flow graph.
	for (uint32_t ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		const IRInstruction& insn = *ctx.at(ir_idx);
		
		if (insn.ir_block != current_block) {
			if (ir_idx > 0) {
				const IRInstruction& term = *ctx.at(ir_idx - 1);
				switch (term.type) {
				case IRInstruction::JMP:
					vctx.block_succesors[term.ir_block].insert((IRBlockId)term.operands[0].value);
					vctx.block_predecessors[(IRBlockId)term.operands[0].value].insert(term.ir_block);
					break;
					
				case IRInstruction::BRANCH:
					vctx.block_succesors[term.ir_block].insert((IRBlockId)term.operands[1].value);
					vctx.block_succesors[term.ir_block].insert((IRBlockId)term.operands[2].value);
					vctx.block_predecessors[(IRBlockId)term.operands[1].value].insert(term.ir_block);
					vctx.block_predecessors[(IRBlockId)term.operands[2].value].insert(term.ir_block);
					break;
					
				case IRInstruction::RET:
				case IRInstruction::DISPATCH:
					break;
					
				default:
					printf("verification failed: block %d does not end in a terminator\n", term.ir_block);
					return false;
				}
			}
			
			current_block = insn.ir_block;
			vctx.blocks.insert(current_block);
		}
	}
	
	std::set<IRBlockId> check_blocks;
	for (uint32_t ir_idx = 0; ir_idx < ctx.count(); ir_idx++) {
		const IRInstruction *insn = ctx.at(ir_idx);
		const IROperand& op0 = insn->operands[0];
		const IROperand& op1 = insn->operands[1];
		const IROperand& op2 = insn->operands[2];
		/*const IROperand& op3 = insn->operands[3];
		const IROperand& op4 = insn->operands[4];
		const IROperand& op5 = insn->operands[5];*/
		
		// Let's see if we've changed block.
		if (insn->ir_block != current_block) {			
			// Change our current block ID.
			current_block = insn->ir_block;			
		}
		
		switch (insn->type) {
		case IRInstruction::NOP:
			break;
			
		case IRInstruction::BARRIER:
			CHECK_OPERANDS(0);
			break;

		case IRInstruction::MOV:
			CHECK_OPERANDS(2);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_vreg());
			assert(op0.size == op1.size);

			ADD_USE(op0);
			ADD_DEF(op1);
			break;

		case IRInstruction::TRUNC:
			CHECK_OPERANDS(2);
			assert(op0.is_vreg());
			assert(op1.is_vreg());
			assert(op0.size > op1.size);

			ADD_USE(op0);
			ADD_DEF(op1);
			break;

		case IRInstruction::SX:
		case IRInstruction::ZX:
			CHECK_OPERANDS(2);
			assert(op0.is_vreg());
			assert(op1.is_vreg());
			assert(op0.size < op1.size);

			ADD_USE(op0);
			ADD_DEF(op1);
			break;

		case IRInstruction::ADD:
		case IRInstruction::SUB:
		case IRInstruction::MUL:
		case IRInstruction::DIV:
		case IRInstruction::MOD:
		case IRInstruction::AND:
		case IRInstruction::OR:
		case IRInstruction::XOR:
			CHECK_OPERANDS(2);
			assert(op0.is_constant() || op0.is_vreg() || op0.is_pc());
			assert(op1.is_vreg());

			ADD_USE(op0);
			ADD_USE(op1);
			ADD_DEF(op1);

			break;
			
		case IRInstruction::ADC:
		case IRInstruction::SBC:
		case IRInstruction::ADC_WITH_FLAGS:
		case IRInstruction::SBC_WITH_FLAGS:
			CHECK_OPERANDS(3);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_vreg());
			assert(op2.is_constant() || op2.is_vreg());
			
			ADD_USE(op0);
			ADD_DEF(op1);
			ADD_USE(op2);
			break;
			
		case IRInstruction::SET_ZN_FLAGS:
			CHECK_OPERANDS(1);
			assert(op0.is_constant() || op0.is_vreg());
			
			ADD_USE(op0);
			break;

		case IRInstruction::SHR:
		case IRInstruction::SHL:
		case IRInstruction::SAR:
			CHECK_OPERANDS(2);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_vreg());

			ADD_USE(op0);
			ADD_USE(op1);
			ADD_DEF(op1);
			break;
			
		case IRInstruction::CLZ:
			CHECK_OPERANDS(2);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_vreg());

			ADD_USE(op0);
			ADD_DEF(op1);
			
			break;			

		case IRInstruction::CMPEQ:
		case IRInstruction::CMPNE:
		case IRInstruction::CMPLT:
		case IRInstruction::CMPLTE:
		case IRInstruction::CMPGT:
		case IRInstruction::CMPGTE:
			CHECK_OPERANDS(3);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_constant() || op1.is_vreg());
			assert(op2.is_vreg());

			assert(op0.size == op1.size);

			ADD_USE(op0);
			ADD_USE(op1);
			ADD_DEF(op2);
			break;
			
		case IRInstruction::READ_REG:
			//  off, tgt
			CHECK_OPERANDS(2);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_vreg());

			ADD_USE(op0);
			ADD_DEF(op1);
			break;

		case IRInstruction::WRITE_REG:
			//  val, off
			CHECK_OPERANDS(2);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_constant() || op1.is_vreg());

			ADD_USE(op0);
			ADD_USE(op1);
			break;
			
		case IRInstruction::READ_MEM:
			// off, disp, tgt
			CHECK_OPERANDS(3);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_constant() || op1.is_vreg());
			assert(op2.is_vreg());
			
			ADD_USE(op0);
			ADD_USE(op1);
			ADD_DEF(op2);
			
			break;
			
		case IRInstruction::READ_MEM_USER:
			// off, tgt
			CHECK_OPERANDS(2);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_vreg());
			
			ADD_USE(op0);
			ADD_DEF(op1);
			
			break;
			
		case IRInstruction::WRITE_MEM:
			// val, disp, off
			CHECK_OPERANDS(3);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_constant() || op1.is_vreg());
			assert(op2.is_constant() || op2.is_vreg());
			
			ADD_USE(op0);
			ADD_USE(op1);
			ADD_USE(op2);
			
			break;
			
		case IRInstruction::WRITE_MEM_USER:
			// val, off
			CHECK_OPERANDS(2);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_constant() || op1.is_vreg());
			
			ADD_USE(op0);
			ADD_USE(op1);
			
			break;
			
		case IRInstruction::READ_DEVICE:
			// dev, off, tgt
			CHECK_OPERANDS(3);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_constant() || op1.is_vreg());
			assert(op2.is_vreg());
			
			ADD_USE(op0);
			ADD_USE(op1);
			ADD_DEF(op2);
			
			break;
			
		case IRInstruction::WRITE_DEVICE:
			// dev, off, val
			CHECK_OPERANDS(3);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_constant() || op1.is_vreg());
			assert(op2.is_constant() || op2.is_vreg());
			
			ADD_USE(op0);
			ADD_USE(op1);
			ADD_USE(op2);
			
			break;

		case IRInstruction::INCPC:
			CHECK_OPERANDS(1);
			assert(op0.is_constant());
			
			break;
			
		case IRInstruction::LDPC:
			CHECK_OPERANDS(1);
			assert(op0.is_vreg());
			
			ADD_DEF(op0);
			break;
			
		case IRInstruction::JMP:
			CHECK_OPERANDS(1);
			assert(op0.is_block());

			check_blocks.insert((IRBlockId)op0.value);
			break;

		case IRInstruction::BRANCH:
			CHECK_OPERANDS(3);
			assert(op0.is_constant() || op0.is_vreg());
			assert(op1.is_block());
			assert(op2.is_block());

			ADD_USE(op0);

			check_blocks.insert((IRBlockId)op1.value);
			check_blocks.insert((IRBlockId)op2.value);
			break;

		case IRInstruction::RET:
			CHECK_OPERANDS(0);
			break;
			
		case IRInstruction::DISPATCH:
			CHECK_OPERANDS(2);
			break;

		case IRInstruction::CALL:
			assert(op0.is_func());
			assert(!op1.is_valid() || op1.is_constant() || op1.is_vreg());
			// TODO
			break;

		case IRInstruction::SET_CPU_MODE:
			CHECK_OPERANDS(1);
			assert(op0.is_constant() || op0.is_vreg());
			ADD_USE(op0);
			break;
			
		case IRInstruction::FLUSH:
		case IRInstruction::FLUSH_DTLB:
		case IRInstruction::FLUSH_ITLB:
			CHECK_OPERANDS(0);
			break;
			
		case IRInstruction::FLUSH_DTLB_ENTRY:
		case IRInstruction::FLUSH_ITLB_ENTRY:
			CHECK_OPERANDS(1);
			
			assert(op0.is_constant() || op0.is_vreg());
			ADD_USE(op0);
			
			break;
			
		default:
			fatal("verifier: unknown instruction type %d\n", insn->type);
		}
	}

	assert(vctx.blocks.count((IRBlockId)0));
	
	// Check jump targets are valid
	for (auto block : check_blocks) {
		if (!vctx.blocks.count(block)) {
			printf("verifier: block %d, which is a jump target, has not been seen\n", block);
			return false;
		}
	}
	
	if (too_many_paths(vctx)) return true;
	
	// Check use-def chains are valid
	std::set<IRBlockId> path;
	if (!walk_usedef(vctx, (IRBlockId)0, path, std::set<IRRegId>()))
		return false;
	return true;
}

