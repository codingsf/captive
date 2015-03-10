#include <jit/wsj-x86.h>

using namespace captive::jit::x86;

X86Builder::X86Builder()
{

}

bool X86Builder::generate(void* buffer, uint64_t& size)
{
	uint8_t *p = (uint8_t *)buffer;
	for (auto insn : instructions) {
		if (insn.opcode > 0x100) {
			*p++ = 0xf0
		}

		*p++ = (uint8_t)insn.opcode;
	}

	return false;
}
