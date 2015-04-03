#include <jit/wsj.h>
#include <jit/wsj-x86.h>
#include <jit/ir.h>
#include <hypervisor/shared-memory.h>

#include <shared-jit.h>

#include <captive.h>

using namespace captive::jit;
using namespace captive::jit::x86;
using namespace captive::shared;

WSJ::WSJ(engine::Engine& engine, util::ThreadPool& worker_threads) : JIT(worker_threads), BlockJIT((JIT&)*this), RegionJIT((JIT&)*this), _engine(engine)
{
}

WSJ::~WSJ()
{
}

bool WSJ::init()
{
	return true;
}

bool WSJ::compile_block(BlockWorkUnit *bwu)
{
	X86Builder builder;

	if (!build(builder, NULL))
		return false;

	uint64_t buffer_size = 4096;
	void *buffer = _shared_memory->allocate(buffer_size);

	builder.generate(buffer, buffer_size);

	//bwu->fnptr =  buffer;
	return false;
}

bool WSJ::compile_region(RegionWorkUnit *rwu)
{
	return false;
}

bool WSJ::build(x86::X86Builder& builder, const TranslationBlock* tb)
{
	IRContext *ir = IRContext::build_context(tb);
	DEBUG << "IR:\n" << ir->to_string();

	ir::IRBlock *block = &ir->entry_block();
	return lower_block(builder, *ir, *block);
}

bool WSJ::lower_block(x86::X86Builder& builder, IRContext& ctx, ir::IRBlock& block)
{
	for (auto insn : block) {
		DEBUG << "Lowering " << insn->to_string();
		switch (insn->type()) {
		case ir::IRInstruction::STREG:
			builder.mov(X86Operand(X86Operand::RSI, 0), X86Operand(X86Operand::RAX));
			break;

		default: assert(false && "Unknown Instruction"); return false;
		}
	}

	builder.ret();
	return true;
}
