#include <interp.h>

using namespace captive::arch;

template<class DecodeType>
Interpreter<DecodeType>::Interpreter()
{

}

template<class DecodeType>
Interpreter<DecodeType>::~Interpreter()
{

}

template<class DecodeType>
bool Interpreter<DecodeType>::step_single_trace(DecodeType& insn)
{
	bool step_ok;

	//printf("%d [%08x] %08x %30s ", get_insns_executed(), insn.pc, insn.ir, disasm.disassemble(pc, *insn));
	step_ok = step_single(insn);
	//printf("\n");

	return step_ok;
}


// Terribly ugly hack
namespace captive {
	namespace arch {
		namespace arm {
			class ArmDecode;
		}
	}
}

template class Interpreter<captive::arch::arm::ArmDecode>;