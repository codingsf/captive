#include <jit/wsj-x86.h>
#include <captive.h>

using namespace captive::jit::x86;

static bool emit_ret(X86OutputBuffer& buffer, X86Instruction& insn)
{
	buffer.emit(0xc3);
	return true;
}

static bool emit_mov(X86OutputBuffer& buffer, X86Instruction& insn)
{
	return false;
}

X86Builder::emitter_fn_t X86Builder::emitter_functions[] = {
	emit_ret,
	emit_mov,
};

X86Builder::X86Builder()
{

}

bool X86Builder::generate(void* raw_buffer, uint64_t& size)
{
	X86OutputBuffer buffer(raw_buffer, size);
	for (auto insn : instructions) {
		if (insn.opcode < sizeof(emitter_functions) / sizeof(emitter_functions[0])) {
			if (!emitter_functions[insn.opcode](buffer, insn)) {
				return false;
			}
		} else {
			assert(false);
			return false;
		}
	}

	size = buffer.used_size();
	return true;
}
