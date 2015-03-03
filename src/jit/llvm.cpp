#include <jit/llvm.h>
#include <jit/llvm-mm.h>
#include <captive.h>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>

#include <llvm/Support/TargetSelect.h>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>

#include <llvm/PassManager.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>

#include <map>
#include <vector>

USE_CONTEXT(JIT)
DECLARE_CHILD_CONTEXT(LLVM, JIT);
DECLARE_CHILD_CONTEXT(LLVMBlockJIT, LLVM);

using namespace captive::jit;
using namespace llvm;

LLVMJIT::LLVMJIT() : mm(NULL)
{
}

LLVMJIT::~LLVMJIT()
{
	if (mm) {
		delete mm;
	}
}

bool LLVMJIT::init()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetAsmPrinter();

	return true;
}

void *LLVMJIT::compile_block(const RawBytecodeDescriptor* bcd)
{
	if (bcd->bytecode_count == 0)
		return NULL;

	LLVMContext ctx;
	Module block_module("block", ctx);

	std::vector<Type *> params;
	params.push_back(IntegerType::getInt8PtrTy(ctx));

	FunctionType *fn = FunctionType::get(IntegerType::getInt32Ty(ctx), params, false);
	Function *block_fn = Function::Create(fn, GlobalValue::ExternalLinkage, "block", &block_module);
	BasicBlock *entry_block = BasicBlock::Create(ctx, "entry", block_fn);

	IRBuilder<> builder(ctx);
	builder.SetInsertPoint(entry_block);

	LoweringContext lc(builder);
	lc.i8 = IntegerType::getInt8Ty(ctx);
	lc.i16 = IntegerType::getInt16Ty(ctx);
	lc.i32 = IntegerType::getInt32Ty(ctx);
	lc.i64 = IntegerType::getInt64Ty(ctx);

	lc.regs = builder.CreatePtrToInt(&block_fn->getArgumentList().front(), lc.i64);

	// Populate the basic-block map
	for (uint32_t block_id = 0; block_id < bcd->block_count; block_id++) {
		lc.basic_blocks[block_id] = BasicBlock::Create(ctx, "bb", block_fn);
	}

	// Populate the vreg map
	for (uint32_t reg_id = 0; reg_id < bcd->vreg_count; reg_id++) {
		lc.vregs[reg_id] = builder.CreateAlloca(IntegerType::getInt32Ty(ctx), NULL, "r" + std::to_string(reg_id));
	}

	// Create a branch to the first block, from the entry block.
	assert(blocks[0]);
	builder.CreateBr(lc.basic_blocks[0]);

	// Lower all instructions
	for (uint32_t idx = 0; idx < bcd->bytecode_count; idx++) {
		const RawBytecode *bc = &bcd->bc[idx];
		BasicBlock *bb = lc.basic_blocks[bc->block_id];

		builder.SetInsertPoint(bb);
		lower_bytecode(lc, bc);
	}

	// Print out the module
	{
		raw_os_ostream str(std::cerr);

		PassManager printManager;
		printManager.add(createPrintModulePass(str, ""));
		printManager.run(block_module);
	}

	// If we haven't got a memory manager, create one now.
	if (!mm) {
		mm = new LLVMJITMemoryManager(_code_arena, _code_arena_size);
	}

	// Initialise a new MCJIT engine
	TargetOptions target_opts;
	target_opts.DisableTailCalls = false;
	target_opts.PositionIndependentExecutable = true;
	target_opts.EnableFastISel = false;
	target_opts.NoFramePointerElim = true;
	target_opts.PrintMachineCode = false;

	ExecutionEngine *engine = EngineBuilder(&block_module)
		.setUseMCJIT(true)
		.setMCJITMemoryManager(mm)
		.setTargetOptions(target_opts)
		.setAllocateGVsWithCode(true)
		.create();

	if (!engine) {
		ERROR << CONTEXT(LLVMBlockJIT) << "Unable to create LLVM Engine";
		return NULL;
	}

	// We actually want compilation to happen.
	engine->DisableLazyCompilation();

	// Compile the function!
	void *ptr = (void *)engine->getFunctionAddress("block");
	if (!ptr) {
		ERROR << CONTEXT(LLVMBlockJIT) << "Unable to compile function";
		return NULL;
	}

	DEBUG << CONTEXT(LLVMBlockJIT) << "Compiled function to " << std::hex << (uint64_t)ptr << ", X=" << (uint32_t)*(uint8_t *)ptr;

	return ptr;
}

void *LLVMJIT::compile_region(const RawBytecodeDescriptor* bcd)
{
	return 0;
}

Value *LLVMJIT::value_for_operand(LoweringContext& ctx, const RawOperand* oper)
{
	if (oper->type == RawOperand::VREG) {
		return ctx.builder.CreateLoad(ctx.vregs[oper->val]);
	} else if (oper->type == RawOperand::CONSTANT) {
		return ctx.const32(oper->val);
	} else {
		return NULL;
	}
}

Value *LLVMJIT::vreg_for_operand(LoweringContext& ctx, const RawOperand* oper)
{
	if (oper->type == RawOperand::VREG) {
		return ctx.vregs[oper->val];
	} else {
		return NULL;
	}
}

BasicBlock *LLVMJIT::block_for_operand(LoweringContext& ctx, const RawOperand* oper)
{
	if (oper->type == RawOperand::BLOCK) {
		return ctx.basic_blocks[oper->val];
	} else {
		return NULL;
	}
}

bool LLVMJIT::lower_bytecode(LoweringContext& ctx, const RawBytecode* bc)
{
	DEBUG << CONTEXT(LLVMBlockJIT) << "Lowering: " << bc->render();

	switch (bc->insn.type) {
	case RawInstruction::JMP:
	{
		BasicBlock *bb = block_for_operand(ctx, &bc->insn.operands[0]);

		assert(bb);
		ctx.builder.CreateBr(bb);
		break;
	}

	case RawInstruction::RET:
	{
		ctx.builder.CreateRet(ctx.const32(0));
		break;
	}

	case RawInstruction::MOV:
	{
		Value *src = value_for_operand(ctx, &bc->insn.operands[0]), *dst = vreg_for_operand(ctx, &bc->insn.operands[1]);

		assert(src && dst);
		ctx.builder.CreateStore(src, dst);
		break;
	}

	case RawInstruction::ADD: break;
	case RawInstruction::SUB: break;
	case RawInstruction::MUL: break;
	case RawInstruction::DIV: break;
	case RawInstruction::MOD: break;

	case RawInstruction::SHL:
	{
		Value *src = value_for_operand(ctx, &bc->insn.operands[0]), *dst = vreg_for_operand(ctx, &bc->insn.operands[1]);

		assert(src && dst);
		ctx.builder.CreateStore(ctx.builder.CreateShl(ctx.builder.CreateLoad(dst), src), dst);
		break;
	}

	case RawInstruction::SHR:
	{
		Value *src = value_for_operand(ctx, &bc->insn.operands[0]), *dst = vreg_for_operand(ctx, &bc->insn.operands[1]);

		assert(src && dst);
		ctx.builder.CreateStore(ctx.builder.CreateLShr(ctx.builder.CreateLoad(dst), src), dst);
		break;
	}

	case RawInstruction::SAR:
	{
		Value *src = value_for_operand(ctx, &bc->insn.operands[0]), *dst = vreg_for_operand(ctx, &bc->insn.operands[1]);

		assert(src && dst);
		ctx.builder.CreateStore(ctx.builder.CreateAShr(ctx.builder.CreateLoad(dst), src), dst);
		break;
	}

	case RawInstruction::CLZ: break;
	case RawInstruction::AND:
	{
		Value *src = value_for_operand(ctx, &bc->insn.operands[0]), *dst = vreg_for_operand(ctx, &bc->insn.operands[1]);

		assert(src && dst);
		ctx.builder.CreateStore(ctx.builder.CreateAnd(ctx.builder.CreateLoad(dst), src), dst);
		break;
	}

	case RawInstruction::OR: break;
	case RawInstruction::XOR: break;
	case RawInstruction::CMPEQ: break;
	case RawInstruction::CMPNE: break;
	case RawInstruction::CMPLT: break;
	case RawInstruction::CMPLTE: break;
	case RawInstruction::CMPGT: break;
	case RawInstruction::CMPGTE: break;
	case RawInstruction::SX: break;
	case RawInstruction::ZX: break;
	case RawInstruction::TRUNC: break;
	case RawInstruction::READ_REG:
	{
		Value *offset = value_for_operand(ctx, &bc->insn.operands[0]), *dst = vreg_for_operand(ctx, &bc->insn.operands[1]);

		assert(offset && dst);

		Value *regptr = ctx.builder.CreateIntToPtr(ctx.builder.CreateAdd(ctx.regs, offset), PointerType::get(ctx.i32, 0));
		ctx.builder.CreateStore(ctx.builder.CreateLoad(regptr), dst);

		break;
	}

	case RawInstruction::WRITE_REG:
	{
		Value *offset = value_for_operand(ctx, &bc->insn.operands[1]), *val = value_for_operand(ctx, &bc->insn.operands[0]);

		assert(offset && val);

		Value *regptr = ctx.builder.CreateIntToPtr(ctx.builder.CreateAdd(ctx.regs, offset), PointerType::get(ctx.i32, 0));
		ctx.builder.CreateStore(val, regptr);

		break;
	}

	case RawInstruction::READ_MEM: break;
	case RawInstruction::WRITE_MEM: break;
	case RawInstruction::CMOV: break;
	case RawInstruction::BRANCH: break;
	case RawInstruction::NOP: break;
	case RawInstruction::TRAP: break;
	case RawInstruction::TAKE_EXCEPTION: break;
	case RawInstruction::SET_CPU_MODE: break;
	}

	return true;
}
