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
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/Verifier.h>
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
	Module *block_module = new Module("block", ctx);

	std::vector<Type *> params;
	params.push_back(IntegerType::getInt8PtrTy(ctx));
	params.push_back(IntegerType::getInt8PtrTy(ctx));

	FunctionType *fn = FunctionType::get(IntegerType::getInt32Ty(ctx), params, false);
	Function *block_fn = Function::Create(fn, GlobalValue::ExternalLinkage, "block", block_module);
	BasicBlock *entry_block = BasicBlock::Create(ctx, "entry", block_fn);

	IRBuilder<> builder(ctx);
	builder.SetInsertPoint(entry_block);

	LoweringContext lc(builder);
	lc.vtype = Type::getVoidTy(ctx);
	lc.i8 = IntegerType::getInt8Ty(ctx); lc.pi8 = PointerType::get(lc.i8, 0);
	lc.i16 = IntegerType::getInt16Ty(ctx); lc.pi16 = PointerType::get(lc.i16, 0);
	lc.i32 = IntegerType::getInt32Ty(ctx); lc.pi32 = PointerType::get(lc.i32, 0);
	lc.i64 = IntegerType::getInt64Ty(ctx); lc.pi64 = PointerType::get(lc.i64, 0);

	Function::ArgumentListType& args = block_fn->getArgumentList();

	lc.cpu_obj = &args.front();
	lc.reg_state = builder.CreatePtrToInt(args.getNext(&args.front()), lc.i64);

	// Populate the basic-block map
	for (uint32_t block_id = 0; block_id < bcd->block_count; block_id++) {
		lc.basic_blocks[block_id] = BasicBlock::Create(ctx, "bb", block_fn);
	}

	// Populate the vreg map
	for (uint32_t reg_id = 0; reg_id < bcd->vreg_count; reg_id++) {
		Type *vreg_type;
		switch (bcd->vregs[reg_id].size) {
		case 1: vreg_type = lc.i8; break;
		case 2: vreg_type = lc.i16; break;
		case 4: vreg_type = lc.i32; break;
		case 8: vreg_type = lc.i64; break;
		default: assert(false);
		}

		lc.vregs[reg_id] = builder.CreateAlloca(vreg_type, NULL, "r" + std::to_string(reg_id));
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

	// Delete empty blocks
	for (auto block : lc.basic_blocks) {
		if (block.second->size() == 0) {
			block.second->eraseFromParent();
		}
	}

	// Verify
	{
		PassManager verifyManager;
		verifyManager.add(createVerifierPass(true));
		verifyManager.run(*block_module);

	}

	// Optimise
	{
		PassManager optManager;

		PassManagerBuilder optManagerBuilder;
		optManagerBuilder.BBVectorize = false;
		optManagerBuilder.DisableTailCalls = true;
		optManagerBuilder.OptLevel = 3;
		optManagerBuilder.populateModulePassManager(optManager);

		optManager.run(*block_module);
	}

	// Print out the module
	{
		raw_os_ostream str(std::cerr);

		PassManager printManager;
		printManager.add(createPrintModulePass(str, ""));
		printManager.run(*block_module);
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

	ExecutionEngine *engine = EngineBuilder(block_module)
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
		switch(oper->size) {
		case 1: return ctx.const8(oper->val);
		case 2: return ctx.const16(oper->val);
		case 4: return ctx.const32(oper->val);
		case 8: return ctx.const64(oper->val);
		default: return NULL;
		}
	} else {
		return NULL;
	}
}

Type *LLVMJIT::type_for_operand(LoweringContext& ctx, const RawOperand* oper, bool ptr)
{
	switch (oper->size) {
	case 1: return ptr ? ctx.pi8 : ctx.i8;
	case 2: return ptr ? ctx.pi16 : ctx.i16;
	case 4: return ptr ? ctx.pi32 : ctx.i32;
	case 8: return ptr ? ctx.pi64 : ctx.i64;
	default: return NULL;
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

	const RawOperand *op0 = &bc->insn.operands[0];
	const RawOperand *op1 = &bc->insn.operands[1];
	const RawOperand *op2 = &bc->insn.operands[2];
	const RawOperand *op3 = &bc->insn.operands[3];

	switch (bc->insn.type) {
	case RawInstruction::JMP:
	{
		BasicBlock *bb = block_for_operand(ctx, op0);

		assert(bb);
		ctx.builder.CreateBr(bb);
		break;
	}

	case RawInstruction::RET:
	{
		ctx.builder.CreateRet(ctx.const32(1));
		break;
	}

	case RawInstruction::MOV:
	{
		Value *src = value_for_operand(ctx, op0), *dst = vreg_for_operand(ctx, op1);

		assert(src && dst);
		ctx.builder.CreateStore(src, dst);
		break;
	}

	case RawInstruction::ADD:
	case RawInstruction::SUB:
	case RawInstruction::MUL:
	case RawInstruction::DIV:
	case RawInstruction::MOD:
	case RawInstruction::SHL:
	case RawInstruction::SHR:
	case RawInstruction::SAR:
	case RawInstruction::AND:
	case RawInstruction::OR:
	case RawInstruction::XOR:
	{
		Value *src = value_for_operand(ctx, op0), *dst = vreg_for_operand(ctx, op1);

		assert(src && dst);

		Value *lhs = ctx.builder.CreateLoad(dst);
		Value *rhs = src;

		Value *result;
		switch (bc->insn.type) {
		case RawInstruction::ADD: result = ctx.builder.CreateAdd(lhs, rhs); break;
		case RawInstruction::SUB: result = ctx.builder.CreateSub(lhs, rhs); break;
		case RawInstruction::MUL: result = ctx.builder.CreateMul(lhs, rhs); break;
		case RawInstruction::DIV: assert(false); break;
		case RawInstruction::MOD: assert(false); break;
		case RawInstruction::SHL: result = ctx.builder.CreateShl(lhs, rhs); break;
		case RawInstruction::SHR: result = ctx.builder.CreateLShr(lhs, rhs); break;
		case RawInstruction::SAR: result = ctx.builder.CreateAShr(lhs, rhs); break;
		case RawInstruction::AND: result = ctx.builder.CreateAnd(lhs, rhs); break;
		case RawInstruction::OR:  result = ctx.builder.CreateOr(lhs, rhs); break;
		case RawInstruction::XOR: result = ctx.builder.CreateXor(lhs, rhs); break;
		default: assert(false);
		}

		ctx.builder.CreateStore(result, dst);
		break;
	}

	case RawInstruction::CLZ: break;

	case RawInstruction::CMPEQ: break;
	case RawInstruction::CMPNE: break;
	case RawInstruction::CMPLT: break;
	case RawInstruction::CMPLTE: break;
	case RawInstruction::CMPGT: break;
	case RawInstruction::CMPGTE: break;

	case RawInstruction::SX:
	case RawInstruction::ZX:
	case RawInstruction::TRUNC:
	{
		Value *src = value_for_operand(ctx, op0), *dst = vreg_for_operand(ctx, op1);

		Instruction::CastOps op = Instruction::Trunc;
		if (bc->insn.type == RawInstruction::SX) {
			op = Instruction::SExt;
		} else if (bc->insn.type == RawInstruction::ZX) {
			op = Instruction::ZExt;
		}

		ctx.builder.CreateStore(ctx.builder.CreateCast(op, src, type_for_operand(ctx, op1, false)), dst);
	}

	case RawInstruction::READ_REG:
	{
		Value *offset = value_for_operand(ctx, op0), *dst = vreg_for_operand(ctx, op1);

		assert(offset && dst);

		offset = ctx.builder.CreateCast(Instruction::ZExt, offset, ctx.i64);

		Value *regptr = ctx.builder.CreateIntToPtr(ctx.builder.CreateAdd(ctx.reg_state, offset), type_for_operand(ctx, op1, true));
		ctx.builder.CreateStore(ctx.builder.CreateLoad(regptr), dst);

		break;
	}

	case RawInstruction::WRITE_REG:
	{
		Value *offset = value_for_operand(ctx, op1), *val = value_for_operand(ctx, op0);

		assert(offset && val);

		offset = ctx.builder.CreateCast(Instruction::ZExt, offset, ctx.i64);

		Value *regptr = ctx.builder.CreateIntToPtr(ctx.builder.CreateAdd(ctx.reg_state, offset), type_for_operand(ctx, op0, true));
		ctx.builder.CreateStore(val, regptr);

		break;
	}

	case RawInstruction::READ_MEM:
	{
		Value *addr = value_for_operand(ctx, op0), *dst = vreg_for_operand(ctx, op1);

		assert(offset && dst);

		Value *memptr = ctx.builder.CreateIntToPtr(addr, type_for_operand(ctx, op1, true));
		ctx.builder.CreateStore(ctx.builder.CreateLoad(memptr), dst);

		break;
	}

	case RawInstruction::WRITE_MEM:
	{
		Value *addr = value_for_operand(ctx, op1), *src = value_for_operand(ctx, op0);

		assert(offset && dst);

		Value *memptr = ctx.builder.CreateIntToPtr(addr, type_for_operand(ctx, op0, true));
		ctx.builder.CreateStore(src, memptr);

		break;
	}

	case RawInstruction::CMOV: break;
	case RawInstruction::BRANCH: break;
	case RawInstruction::NOP: break;
	case RawInstruction::TRAP: break;
	case RawInstruction::TAKE_EXCEPTION: {
		std::vector<Type *> params;
		params.push_back(ctx.pi8);
		params.push_back(ctx.i32);
		params.push_back(ctx.i32);

		FunctionType *fntype = FunctionType::get(ctx.vtype, params, false);
		Constant *fn = ctx.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("jit_take_exception", fntype);

		assert(fn);

		Value *v1 = value_for_operand(ctx, op0), *v2 = value_for_operand(ctx, op1);

		assert(v1 && v2);

		ctx.builder.CreateCall3(fn, ctx.cpu_obj, v1, v2);
		break;
	}

	case RawInstruction::SET_CPU_MODE: break;
	}

	return true;
}
