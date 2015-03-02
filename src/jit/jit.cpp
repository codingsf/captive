#include <jit/jit.h>
#include <captive.h>

DECLARE_CONTEXT(JIT);

using namespace captive::jit;

bool JIT::init()
{
	return true;
}

struct ir_opcode {
	uint32_t type;
	uint32_t val;
};

struct ir_insn {
	uint32_t block_id;
	uint32_t type;
	ir_opcode opcodes[4];
};

uint64_t JIT::compile_block(void* ir, uint32_t count)
{
	DEBUG << CONTEXT(JIT) << "Compiling block with " << count << " instructions @ " << std::hex << (uint64_t)ir;

	ir_insn *insns = (ir_insn *)ir;
	for (int i = 0; i < count; i++) {
		DEBUG << CONTEXT(JIT) << "Instruction " << i << ": block=" << insns[i].block_id << ", type=" << insns[i].type;
	}

	return 0;
}
