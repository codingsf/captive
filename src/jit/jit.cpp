#include <jit/jit.h>
#include <util/thread-pool.h>
#include <shared-jit.h>
#include <captive.h>

#include <sstream>
#include <unistd.h>

#include <unordered_map>
#include <set>

DECLARE_CONTEXT(JIT);

using namespace captive::jit;
using namespace captive::shared;

std::string InstructionPrinter::render_instruction(const IRInstruction& insn)
{
	std::stringstream str;

	switch (insn.type) {
	case IRInstruction::INVALID:
		str << "<invalid>";
		break;

	case IRInstruction::VERIFY:
		str << "verify";
		break;

	case IRInstruction::NOP:
		str << "nop";
		break;

	case IRInstruction::TRAP:
		str << "trap";
		break;

	case IRInstruction::MOV:
		str << "mov " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;

	case IRInstruction::CMOV:
		str << "cmov " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]) << ", " << render_operand(insn.operands[2]);
		break;

	case IRInstruction::LDPC:
		str << "ldpc " << render_operand(insn.operands[0]);
		break;

	case IRInstruction::ADD:
		str << "add " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::SUB:
		str << "sub " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::MUL:
		str << "mul " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::DIV:
		str << "div " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::MOD:
		str << "mod " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;

	case IRInstruction::SHL:
		str << "shl " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::SHR:
		str << "shr " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::SAR:
		str << "sar " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::CLZ:
		str << "clz " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;

	case IRInstruction::AND:
		str << "and " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::OR:
		str << "or " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::XOR:
		str << "xor " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;

	case IRInstruction::CMPEQ:
		str << "cmpeq " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]) << ", " << render_operand(insn.operands[2]);
		break;
	case IRInstruction::CMPNE:
		str << "cmpne " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]) << ", " << render_operand(insn.operands[2]);
		break;
	case IRInstruction::CMPGT:
		str << "cmpgt " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]) << ", " << render_operand(insn.operands[2]);
		break;
	case IRInstruction::CMPGTE:
		str << "cmpgte " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]) << ", " << render_operand(insn.operands[2]);
		break;
	case IRInstruction::CMPLT:
		str << "cmplt " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]) << ", " << render_operand(insn.operands[2]);
		break;
	case IRInstruction::CMPLTE:
		str << "cmplte " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]) << ", " << render_operand(insn.operands[2]);
		break;

	case IRInstruction::SX:
		str << "sx " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::ZX:
		str << "zx " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::TRUNC:
		str << "trunc " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;

	case IRInstruction::READ_REG:
		str << "ldreg " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::WRITE_REG:
		str << "streg " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::READ_MEM:
		str << "ldmem " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;
	case IRInstruction::WRITE_MEM:
		str << "stmem " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]);
		break;

	case IRInstruction::CALL:
		str << "call";
		break;

	case IRInstruction::JMP:
		str << "jump " << render_operand(insn.operands[0]);
		break;
	case IRInstruction::BRANCH:
		str << "branch " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]) << ", " << render_operand(insn.operands[2]);
		break;
	case IRInstruction::RET:
		str << "ret";
		break;

	case IRInstruction::SET_CPU_MODE:
		str << "set-cpu-mode " << render_operand(insn.operands[0]);
		break;

	case IRInstruction::WRITE_DEVICE:
		str << "stdev " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]) << ", " << render_operand(insn.operands[2]);
		break;
	case IRInstruction::READ_DEVICE:
		str << "lddev " << render_operand(insn.operands[0]) << ", " << render_operand(insn.operands[1]) << ", " << render_operand(insn.operands[2]);
		break;

	default:
		str << "???";
		break;
	}

	return str.str();
}

std::string InstructionPrinter::render_operand(const IROperand& oper)
{
	switch (oper.type) {
	case IROperand::CONSTANT:
		return "i" + std::to_string(oper.size) + " #" + std::to_string(oper.value);

	case IROperand::VREG:
		return "i" + std::to_string(oper.size) + " r" + std::to_string(oper.value);

	case IROperand::BLOCK:
		return "b" + std::to_string(oper.value);

	case IROperand::PC:
		return "pc";

	case IROperand::FUNC:
	{
		std::stringstream x;
		x << "f" << std::hex << oper.value;
		return x.str();
	}

	default:
		return "i" + std::to_string(oper.size) + " ?" + std::to_string(oper.value);
	}
}

/*

std::string RawInstruction::mnemonic() const
{
	switch(type) {
	case RawInstruction::VERIFY: return "verify";
	case RawInstruction::CALL: return "call";
	case RawInstruction::JMP: return "jump";
	case RawInstruction::MOV: return "mov";
	case RawInstruction::LDPC: return "ldpc";
	case RawInstruction::ADD: return "add";
	case RawInstruction::SUB: return "sub";
	case RawInstruction::MUL: return "mul";
	case RawInstruction::DIV: return "div";
	case RawInstruction::MOD: return "mod";
	case RawInstruction::SHL: return "shl";
	case RawInstruction::SHR: return "shr";
	case RawInstruction::SAR: return "sar";
	case RawInstruction::CLZ: return "clz";
	case RawInstruction::AND: return "and";
	case RawInstruction::OR: return "or";
	case RawInstruction::XOR: return "xor";
	case RawInstruction::CMPEQ: return "cmp-eq";
	case RawInstruction::CMPNE: return "cmp-ne";
	case RawInstruction::CMPLT: return "cmp-lt";
	case RawInstruction::CMPLTE: return "cmp-lte";
	case RawInstruction::CMPGT: return "cmp-gt";
	case RawInstruction::CMPGTE: return "cmp-gte";
	case RawInstruction::SX: return "sx";
	case RawInstruction::ZX: return "zx";
	case RawInstruction::TRUNC: return "trunc";
	case RawInstruction::READ_REG: return "ldreg";
	case RawInstruction::WRITE_REG: return "streg";
	case RawInstruction::READ_MEM: return "ldmem";
	case RawInstruction::WRITE_MEM: return "stmem";
	case RawInstruction::CMOV: return "cmov";
	case RawInstruction::BRANCH: return "branch";
	case RawInstruction::RET: return "return";
	case RawInstruction::NOP: return "nop";
	case RawInstruction::TRAP: return "trap";
	case RawInstruction::SET_CPU_MODE: return "set-cpu-mode";
	case RawInstruction::WRITE_DEVICE: return "write-device";
	case RawInstruction::READ_DEVICE: return "read-device";
	case RawInstruction::INVALID: return "(invalid)";
	default:
		return "???";
	}
}

std::string RawBytecode::render() const
{
	std::stringstream str;

	str << "Block=" << block_id << ": " << insn.mnemonic() << " ";

	switch (insn.type) {
	case RawInstruction::TRAP:
	case RawInstruction::NOP:
	case RawInstruction::RET:
	case RawInstruction::INVALID:
	case RawInstruction::VERIFY:
		break;

	case RawInstruction::JMP:
	case RawInstruction::LDPC:
	case RawInstruction::SET_CPU_MODE:
		str << insn.operands[0].render();
		break;

	case RawInstruction::MOV:
	case RawInstruction::ADD:
	case RawInstruction::SUB:
	case RawInstruction::MUL:
	case RawInstruction::DIV:
	case RawInstruction::MOD:
	case RawInstruction::SHL:
	case RawInstruction::SHR:
	case RawInstruction::SAR:
	case RawInstruction::CLZ:
	case RawInstruction::AND:
	case RawInstruction::OR:
	case RawInstruction::XOR:
	case RawInstruction::SX:
	case RawInstruction::ZX:
	case RawInstruction::TRUNC:
	case RawInstruction::READ_REG:
	case RawInstruction::WRITE_REG:
	case RawInstruction::READ_MEM:
	case RawInstruction::WRITE_MEM:
		str << insn.operands[0].render() << ", " << insn.operands[1].render();
		break;

	case RawInstruction::CMPEQ:
	case RawInstruction::CMPNE:
	case RawInstruction::CMPLT:
	case RawInstruction::CMPLTE:
	case RawInstruction::CMPGT:
	case RawInstruction::CMPGTE:
	case RawInstruction::CMOV:
	case RawInstruction::BRANCH:
	case RawInstruction::WRITE_DEVICE:
	case RawInstruction::READ_DEVICE:
		str << insn.operands[0].render() << ", " << insn.operands[1].render() << ", " << insn.operands[2].render();
		break;

	case RawInstruction::CALL:
		for (int i = 0; i < 4; i++) {
			if (insn.operands[i].type == RawOperand::NONE)
				continue;

			if (i > 0)
				str << ", ";

			str << insn.operands[i].render();
		}
		break;
	}

	return str.str();
}*/

JIT::JIT(util::ThreadPool& worker_threads) : _worker_threads(worker_threads), _shared_memory(NULL)
{

}

JIT::~JIT()
{

}

JITStrategy::JITStrategy(JIT& owner) : _owner(owner)
{

}

JITStrategy::~JITStrategy()
{

}

BlockJIT::BlockJIT(JIT& owner) : JITStrategy(owner)
{

}


BlockJIT::~BlockJIT()
{

}

void BlockJIT::compile_block_async(BlockWorkUnit* work_unit, block_completion_t completion, void* completion_data)
{
	assert(false);
}

RegionJIT::RegionJIT(JIT& owner) : JITStrategy(owner)
{

}

RegionJIT::~RegionJIT()
{

}

class RegionAsyncCompilation {
public:
	RegionAsyncCompilation(RegionJIT *jit, RegionWorkUnit *rwu, RegionJIT::region_completion_t completion, void *completion_data)
		: _jit(jit), _rwu(rwu), _completion(completion), _completion_data(completion_data) { }

	inline RegionJIT *jit() const { return _jit; }
	inline RegionWorkUnit *region_work_unit() const { return _rwu; }
	inline RegionJIT::region_completion_t completion() const { return _completion; }
	inline void *completion_data() const { return _completion_data; }

private:
	RegionJIT *_jit;
	RegionWorkUnit *_rwu;
	RegionJIT::region_completion_t _completion;
	void *_completion_data;
};

static uint64_t do_compile_region(void *odata)
{
	RegionAsyncCompilation *data = (RegionAsyncCompilation *)odata;

	bool result = data->jit()->compile_region(data->region_work_unit());
	data->completion()(data->region_work_unit(), result, data->completion_data());

	delete data;
	return 0;
}

void RegionJIT::compile_region_async(RegionWorkUnit *rwu, region_completion_t completion, void *completion_data)
{
	RegionAsyncCompilation *data = new RegionAsyncCompilation(this, rwu, completion, completion_data);
	owner().worker_threads().queue_work(do_compile_region, NULL, data);
}

bool JIT::quick_opt(TranslationBlock* tb)
{
	std::set<IRBlockId> blocks;
	std::unordered_multimap<IRBlockId, IRBlockId> block_succs;
	std::unordered_multimap<IRBlockId, IRBlockId> block_preds;
	std::unordered_map<IRBlockId, IRInstruction *> block_terminators;

	// Remove useless instructions, and build a block predecessor/successor map while we're at it.
	for (uint32_t i = 0; i < tb->ir_insn_count; i++) {
		IRInstruction *insn = (IRInstruction *)&tb->ir_insn[i];

		blocks.insert(insn->ir_block);

		switch (insn->type) {
		case IRInstruction::ADD:
		case IRInstruction::SUB:
		case IRInstruction::SAR:
		case IRInstruction::SHR:
		case IRInstruction::SHL:
		case IRInstruction::OR:
		case IRInstruction::XOR:
			if (insn->operands[0].type == IROperand::CONSTANT && insn->operands[0].value == 0) {
				insn->type = IRInstruction::NOP;
			}
			break;

		case IRInstruction::MUL:
		case IRInstruction::DIV:
			if (insn->operands[0].type == IROperand::CONSTANT && insn->operands[0].value == 1) {
				insn->type = IRInstruction::NOP;
			}
			break;

		case IRInstruction::JMP:
			assert(insn->operands[0].type == IROperand::BLOCK);

			block_succs.emplace((IRBlockId)insn->ir_block, (IRBlockId)insn->operands[0].value);
			block_preds.emplace((IRBlockId)insn->operands[0].value, (IRBlockId)insn->ir_block);
			block_terminators[insn->ir_block] = insn;

			break;

		case IRInstruction::BRANCH:
			assert(insn->operands[1].type == IROperand::BLOCK);
			assert(insn->operands[2].type == IROperand::BLOCK);

			block_succs.emplace((IRBlockId)insn->ir_block, (IRBlockId)insn->operands[1].value);
			block_succs.emplace((IRBlockId)insn->ir_block, (IRBlockId)insn->operands[2].value);

			block_preds.emplace((IRBlockId)insn->operands[1].value, (IRBlockId)insn->ir_block);
			block_preds.emplace((IRBlockId)insn->operands[2].value, (IRBlockId)insn->ir_block);

			break;

		default:
			break;
		}
	}

	// Now, merge blocks
/*retry:
	for (auto merger : blocks) {
		// If the merger has multiple successors, then we can't merge
		if (block_succs.count(merger) != 1) continue;

		// Find the candidate mergee
		auto mergee = block_succs.find(merger)->second;

		// If the mergee has multiple predecessors, then we can't merge.
		if (block_preds.count(mergee) != 1) continue;

		// Okay, so we can merge the mergee into the merger.
		fprintf(stderr, "merge %d into %d\n", mergee, merger);

		// Delete the terminator instruction in the merger.
		block_terminators[merger]->insn.type = RawInstruction::NOP;

		// Rewrite the instructions in the mergee block, to belong to
		// the merger block.
		for (uint32_t i = 0; i < count; i++) {
			if (bcarr[i].block_id == mergee) {
				bcarr[i].block_id = merger;
			}
		}

		// Erase the successors of the merger (should only be the mergee).
		block_succs.erase(merger);

		// Insert the successors of the mergee as the successors of the
		// merger, and change the predecessors of the mergee successors
		// to the merger.
		for (auto S = block_succs.find(mergee); S != block_succs.end(); ++S) {
			auto original_successor = S->second;
			// Insert the successor
			block_succs.emplace((uint32_t)merger, (uint32_t)original_successor);

			for (auto P = block_preds.find(original_successor); P != block_preds.end(); ++P) {
				auto original_predecessor = P->second;

				if (original_predecessor == mergee) {
					block_preds.erase(P);
					block_preds.emplace((uint32_t)original_successor, (uint32_t)merger);
				}
			}
		}

		// Erase the successors of the mergee.
		block_succs.erase(mergee);

		// Erase the predecessors of the mergee.
		block_preds.erase(mergee);

		// Erase the mergee
		blocks.erase(mergee);

		goto retry;
	}*/

	return true;
}
