#include <jit/wsj.h>
#include <jit/wsj-x86.h>
#include <jit/ir.h>
#include <hypervisor/shared-memory.h>

#include <captive.h>

using namespace captive::jit;
using namespace captive::jit::x86;

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

BlockCompilationResult WSJ::compile_block(BlockWorkUnit *bwu)
{
	BlockCompilationResult result;
	result.fn_ptr = NULL;
	result.work_unit_id = bwu->work_unit_id;

	X86Builder builder;

	if (!build(builder, bwu->bds))
		return result;

	uint64_t buffer_size = 4096;
	void *buffer = _shared_memory->allocate(buffer_size);

	builder.generate(buffer, buffer_size);

	result.fn_ptr = buffer;
	return result;
}

RegionCompilationResult WSJ::compile_region(RegionWorkUnit *rwu)
{
	RegionCompilationResult result;
	result.fn_ptr = NULL;
	result.work_unit_id = rwu->work_unit_id;

	return result;
}

bool WSJ::build(x86::X86Builder& builder, const RawBytecodeDescriptor* bcd)
{
	IRContext *ir = IRContext::build_context(bcd);
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
