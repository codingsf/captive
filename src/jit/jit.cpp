#include <jit/jit.h>
#include <util/thread-pool.h>
#include <hypervisor/cpu.h>
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

JIT::JIT(util::ThreadPool& worker_threads) : _worker_threads(worker_threads), _shared_memory(NULL)
{

}

JIT::~JIT()
{

}

struct block_txln_cache_entry {
	uint32_t tag;
	uint32_t count;
	void *fn;
};

struct analysis_work {
	JIT *jit;
	std::unordered_map<uint32_t, std::vector<uint32_t>> regions;
	captive::hypervisor::CPU *cpu;
};

static uint64_t analyse_async(const struct analysis_work *work)
{
	for (const auto& region : work->regions) {
		work->jit->compile_region_async(region.first, region.second);
	}
	
	work->cpu->interrupt(2);
	
	delete work;
	return 0;
}

void JIT::analyse(captive::hypervisor::CPU& cpu, void* cache_ptr)
{
	struct analysis_work *work = new struct analysis_work();
	work->jit = this;
	work->cpu = &cpu;
	
	for (uint32_t cache_idx = 0; cache_idx < 20000003; cache_idx++) {
		struct block_txln_cache_entry *entry = &((struct block_txln_cache_entry *)cache_ptr)[cache_idx];
		if (entry->tag == 1) continue;
		
		std::vector<uint32_t>& blocks = work->regions[entry->tag & ~0xfff];
		blocks.push_back(entry->tag);
	}
	
	_worker_threads.queue_work((captive::util::action_t)analyse_async, NULL, work);
}
