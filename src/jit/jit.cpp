#include <jit/jit.h>
#include <captive.h>

#include <sstream>

DECLARE_CONTEXT(JIT);

using namespace captive::jit;

std::string RawOperand::render() const
{
	switch(type) {
	case CONSTANT:
		return "i" + std::to_string(size) + " #" + std::to_string(val);
	case VREG:
		return "i" + std::to_string(size) + " r" + std::to_string(val);
	case BLOCK:
		return "b" + std::to_string(val);
	case FUNC:
	{
		std::stringstream x;
		x << "f" << std::hex << val;
		return x.str();
	}
	default:
		return "i" + std::to_string(size) + " ?" + std::to_string(val);
	}
}

std::string RawInstruction::mnemonic() const
{
	switch(type) {
	case RawInstruction::CALL: return "call";
	case RawInstruction::JMP: return "jump";
	case RawInstruction::MOV: return "mov";
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
		break;

	case RawInstruction::JMP:
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
	case RawInstruction::CMPEQ:
	case RawInstruction::CMPNE:
	case RawInstruction::CMPLT:
	case RawInstruction::CMPLTE:
	case RawInstruction::CMPGT:
	case RawInstruction::CMPGTE:
	case RawInstruction::SX:
	case RawInstruction::ZX:
	case RawInstruction::TRUNC:
	case RawInstruction::READ_REG:
	case RawInstruction::WRITE_REG:
	case RawInstruction::READ_MEM:
	case RawInstruction::WRITE_MEM:
		str << insn.operands[0].render() << ", " << insn.operands[1].render();
		break;

	case RawInstruction::CMOV:
	case RawInstruction::BRANCH:
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
}

JIT::JIT() : _code_arena(NULL), _code_arena_size(0)
{

}

JIT::~JIT()
{

}

BlockJIT::~BlockJIT()
{

}

RegionJIT::~RegionJIT()
{

}
