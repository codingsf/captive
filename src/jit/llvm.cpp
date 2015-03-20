#include <jit/llvm.h>
#include <jit/llvm-mm.h>
#include <jit/allocator.h>
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

LLVMJIT::LLVMJIT(engine::Engine& engine) : BlockJIT((JIT&)*this), RegionJIT((JIT&)*this), _engine(engine), _allocator(NULL)
{
}

LLVMJIT::~LLVMJIT()
{
	if (_allocator) {
		delete _allocator;
	}
}

bool LLVMJIT::init()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetAsmPrinter();

	return true;
}

void *LLVMJIT::internal_compile_block(const RawBytecodeDescriptor* bcd)
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

	block_fn->addFnAttr(Attribute::NoRedZone);

	BasicBlock *entry_block = BasicBlock::Create(ctx, "entry", block_fn);

	IRBuilder<> builder(ctx);
	builder.SetInsertPoint(entry_block);

	LoweringContext lc(builder);
	lc.vtype = Type::getVoidTy(ctx);
	lc.i1 = IntegerType::getInt1Ty(ctx);
	lc.i8 = IntegerType::getInt8Ty(ctx); lc.pi8 = PointerType::get(lc.i8, 0);
	lc.i16 = IntegerType::getInt16Ty(ctx); lc.pi16 = PointerType::get(lc.i16, 0);
	lc.i32 = IntegerType::getInt32Ty(ctx); lc.pi32 = PointerType::get(lc.i32, 0);
	lc.i64 = IntegerType::getInt64Ty(ctx); lc.pi64 = PointerType::get(lc.i64, 0);

	Function::ArgumentListType& args = block_fn->getArgumentList();

	lc.cpu_obj = &args.front();
	lc.reg_state = builder.CreatePtrToInt(args.getNext(&args.front()), lc.i64);

	// Populate the basic-block map
	for (uint32_t idx = 0; idx < bcd->bytecode_count; idx++) {
		if (lc.basic_blocks[bcd->bc[idx].block_id] == NULL) {
			lc.basic_blocks[bcd->bc[idx].block_id] = BasicBlock::Create(ctx, "bb", block_fn);
		}
	}

	lc.alloca_block = entry_block;

	// Lower all instructions
	for (uint32_t idx = 0; idx < bcd->bytecode_count; idx++) {
		const RawBytecode *bc = &bcd->bc[idx];
		BasicBlock *bb = lc.basic_blocks[bc->block_id];

		builder.SetInsertPoint(bb);
		if (!lower_bytecode(lc, bc)) {
			ERROR << CONTEXT(LLVMBlockJIT) << "Failed to lower byte-code: " << bc->render();
			return NULL;
		}
	}

	// Create a branch to the first block, from the entry block.
	builder.SetInsertPoint(entry_block);
	assert(blocks[0]);
	builder.CreateBr(lc.basic_blocks[0]);

	// Verify
	{
		PassManager verifyManager;
		verifyManager.add(createVerifierPass(true));
		verifyManager.run(*block_module);

	}

	// Print out the module
	/*{
		raw_os_ostream str(std::cerr);

		PassManager printManager;
		printManager.add(createPrintModulePass(str, ""));
		printManager.run(*block_module);
	}*/

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
	/*{
		raw_os_ostream str(std::cerr);

		PassManager printManager;
		printManager.add(createPrintModulePass(str, ""));
		printManager.run(*block_module);
	}*/

	if (_allocator == NULL) {
		_allocator = new Allocator(_code_arena, _code_arena_size);
	}

	// Initialise a new MCJIT engine
	TargetOptions target_opts;
	target_opts.DisableTailCalls = false;
	target_opts.PositionIndependentExecutable = true;
	target_opts.EnableFastISel = false;
	target_opts.NoFramePointerElim = true;
	target_opts.PrintMachineCode = false;

	ExecutionEngine *engine = EngineBuilder(block_module)
		.setEngineKind(EngineKind::JIT)
		/*.setOptLevel(CodeGenOpt::Aggressive)
		.setRelocationModel(Reloc::PIC_)*/
		.setUseMCJIT(true)
		.setMCJITMemoryManager(new LLVMJITMemoryManager(_engine, *_allocator))
		.setTargetOptions(target_opts)
		.setAllocateGVsWithCode(true)
		.create();

	if (!engine) {
		ERROR << CONTEXT(LLVMBlockJIT) << "Unable to create LLVM Engine";
		return NULL;
	}

	// We actually want compilation to happen.
	engine->DisableLazyCompilation();
	engine->finalizeObject();

	// Compile the function!
	void *ptr = (void *)engine->getFunctionAddress(block_fn->getName());
	if (!ptr) {
		ERROR << CONTEXT(LLVMBlockJIT) << "Unable to compile function";
		return NULL;
	}

	block_fn->deleteBody();
	delete engine;

	return ptr;
}

Value *LLVMJIT::value_for_operand(LoweringContext& ctx, const RawOperand* oper)
{
	if (oper->type == RawOperand::VREG) {
		return ctx.builder.CreateLoad(vreg_for_operand(ctx, oper));
	} else if (oper->type == RawOperand::CONSTANT) {
		switch(oper->size) {
		case 1: return ctx.const8(oper->val);
		case 2: return ctx.const16(oper->val);
		case 4: return ctx.const32(oper->val);
		case 8: return ctx.const64(oper->val);
		default: return NULL;
		}
	} else if (oper->type == RawOperand::FUNC) {
		return ctx.const64(oper->val);
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
	default: assert(false); return NULL;
	}
}

Value *LLVMJIT::insert_vreg(LoweringContext& ctx, uint32_t idx, uint8_t size)
{
	IRBuilder<> alloca_block_builder(ctx.alloca_block);

	Type *vreg_type = NULL;
	switch (size) {
	case 1: vreg_type = ctx.i8; break;
	case 2: vreg_type = ctx.i16; break;
	case 4: vreg_type = ctx.i32; break;
	case 8: vreg_type = ctx.i64; break;
	default: assert(false); return NULL;
	}

	Value *vreg_alloc = alloca_block_builder.CreateAlloca(vreg_type, NULL, "r" + std::to_string(idx));

	ctx.vregs[idx] = vreg_alloc;
	return vreg_alloc;
}

Value *LLVMJIT::vreg_for_operand(LoweringContext& ctx, const RawOperand* oper)
{
	if (oper->type == RawOperand::VREG) {
		if (ctx.vregs.find(oper->val) == ctx.vregs.end()) {
			return insert_vreg(ctx, oper->val, oper->size);
		} else {
			return ctx.vregs[oper->val];
		}
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
	// DEBUG << CONTEXT(LLVMBlockJIT) << "Lowering: " << bc->render();

	const RawOperand *op0 = &bc->insn.operands[0];
	const RawOperand *op1 = &bc->insn.operands[1];
	const RawOperand *op2 = &bc->insn.operands[2];
	/*const RawOperand *op3 = &bc->insn.operands[3];
	const RawOperand *op4 = &bc->insn.operands[4];
	const RawOperand *op5 = &bc->insn.operands[5];*/

	switch (bc->insn.type) {
	case RawInstruction::JMP:
	{
		BasicBlock *bb = block_for_operand(ctx, op0);

		assert(bb);
		ctx.builder.CreateBr(bb);
		return true;
	}

	case RawInstruction::CALL:
	{
		std::vector<Type *> param_types;
		param_types.push_back(ctx.pi8);

		std::vector<Value *> param_vals;
		param_vals.push_back(ctx.cpu_obj);

		for (int i = 1; i < 6; i++) {
			const RawOperand *oper = &bc->insn.operands[i];
			if (oper->type == RawOperand::NONE) continue;

			switch (oper->size) {
			case 1: param_types.push_back(ctx.i8); param_vals.push_back(value_for_operand(ctx, oper)); break;
			case 2: param_types.push_back(ctx.i16); param_vals.push_back(value_for_operand(ctx, oper)); break;
			case 4: param_types.push_back(ctx.i32); param_vals.push_back(value_for_operand(ctx, oper)); break;
			case 8: param_types.push_back(ctx.i64); param_vals.push_back(value_for_operand(ctx, oper)); break;
			default: assert(false);
			}
		}

		PointerType *fntype = FunctionType::get(ctx.vtype, param_types, false)->getPointerTo(0);
		assert(fntype);

		Value *fn = value_for_operand(ctx, op0);
		assert(fn);

		fn = ctx.builder.CreateIntToPtr(fn, fntype);
		assert(fn);

		ctx.builder.CreateCall(fn, param_vals);
		return true;
	}

	case RawInstruction::RET:
	{
		ctx.builder.CreateRet(ctx.const32(1));
		return true;
	}

	case RawInstruction::MOV:
	{
		Value *src = value_for_operand(ctx, op0), *dst = vreg_for_operand(ctx, op1);

		assert(src && dst);
		ctx.builder.CreateStore(src, dst);
		return true;
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

		Value *result = NULL;
		switch (bc->insn.type) {
		case RawInstruction::ADD: result = ctx.builder.CreateAdd(lhs, rhs); break;
		case RawInstruction::SUB: result = ctx.builder.CreateSub(lhs, rhs); break;
		case RawInstruction::MUL: result = ctx.builder.CreateMul(lhs, rhs); break;
		case RawInstruction::DIV: result = ctx.builder.CreateUDiv(lhs, rhs); break;
		case RawInstruction::MOD: result = ctx.builder.CreateURem(lhs, rhs); break;

		case RawInstruction::SHL: result = ctx.builder.CreateShl(lhs, rhs); break;
		case RawInstruction::SHR: result = ctx.builder.CreateLShr(lhs, rhs); break;
		case RawInstruction::SAR: result = ctx.builder.CreateAShr(lhs, rhs); break;

		case RawInstruction::AND: result = ctx.builder.CreateAnd(lhs, rhs); break;
		case RawInstruction::OR:  result = ctx.builder.CreateOr(lhs, rhs); break;
		case RawInstruction::XOR: result = ctx.builder.CreateXor(lhs, rhs); break;

		default: assert(false);
		}

		assert(result);
		ctx.builder.CreateStore(result, dst);
		return true;
	}

	case RawInstruction::CMPEQ:
	case RawInstruction::CMPNE:
	case RawInstruction::CMPLT:
	case RawInstruction::CMPLTE:
	case RawInstruction::CMPGT:
	case RawInstruction::CMPGTE:
	{
		Value *lh = value_for_operand(ctx, op0), *rh = value_for_operand(ctx, op1), *dst = vreg_for_operand(ctx, op2);

		assert(lh && rh && dst);

		Value *result = NULL;
		switch(bc->insn.type) {
		case RawInstruction::CMPEQ: result = ctx.builder.CreateICmpEQ(lh, rh); break;
		case RawInstruction::CMPNE: result = ctx.builder.CreateICmpNE(lh, rh); break;

		case RawInstruction::CMPLT: result = ctx.builder.CreateICmpULT(lh, rh); break;
		case RawInstruction::CMPLTE: result = ctx.builder.CreateICmpULE(lh, rh); break;

		case RawInstruction::CMPGT: result = ctx.builder.CreateICmpUGT(lh, rh); break;
		case RawInstruction::CMPGTE: result = ctx.builder.CreateICmpUGE(lh, rh); break;

		default: assert(false);
		}

		result = ctx.builder.CreateCast(Instruction::ZExt, result, ctx.i8);
		ctx.builder.CreateStore(result, dst);

		return true;
	}

	case RawInstruction::CLZ:
	{
		std::vector<Type *> params;
		params.push_back(ctx.i32);
		params.push_back(ctx.i1);

		FunctionType *fntype = FunctionType::get(ctx.i32, params, false);
		Constant *fn = ctx.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("llvm.ctlz.i32", fntype);
		assert(fn);

		Value *src = value_for_operand(ctx, op0), *dst = vreg_for_operand(ctx, op1);
		assert(src && dst);

		Value *result = ctx.builder.CreateCall2(fn, src, ctx.const1(0));
		ctx.builder.CreateStore(result, dst);

		return true;
	}

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
		return true;
	}

	case RawInstruction::READ_REG:
	{
		Value *offset = value_for_operand(ctx, op0), *dst = vreg_for_operand(ctx, op1);

		assert(offset && dst);

		offset = ctx.builder.CreateCast(Instruction::ZExt, offset, ctx.i64);

		Value *regptr = ctx.builder.CreateIntToPtr(ctx.builder.CreateAdd(ctx.reg_state, offset), type_for_operand(ctx, op1, true));
		ctx.builder.CreateStore(ctx.builder.CreateLoad(regptr), dst);

		return true;
	}

	case RawInstruction::WRITE_REG:
	{
		Value *offset = value_for_operand(ctx, op1), *val = value_for_operand(ctx, op0);

		assert(offset && val);

		offset = ctx.builder.CreateCast(Instruction::ZExt, offset, ctx.i64);

		Value *regptr = ctx.builder.CreateAdd(ctx.reg_state, offset);
		regptr = ctx.builder.CreateIntToPtr(regptr, type_for_operand(ctx, op0, true));
		ctx.builder.CreateStore(val, regptr);

		return true;
	}

	case RawInstruction::READ_MEM:
	{
		Value *addr = value_for_operand(ctx, op0), *dst = vreg_for_operand(ctx, op1);

		assert(addr && dst);

		Type *memptrtype = type_for_operand(ctx, op1, true);
		assert(memptrtype);

		Value *memptr = ctx.builder.CreateIntToPtr(addr, memptrtype);
		ctx.builder.CreateStore(ctx.builder.CreateLoad(memptr, true), dst);

		return true;
	}

	case RawInstruction::WRITE_MEM:
	{
		Value *addr = value_for_operand(ctx, op1), *src = value_for_operand(ctx, op0);

		assert(addr && dst);

		Value *memptr = ctx.builder.CreateIntToPtr(addr, type_for_operand(ctx, op0, true));
		ctx.builder.CreateStore(src, memptr, true);

		return true;
	}

	case RawInstruction::CMOV: assert(false && "Unsupported CMOV"); return false;

	case RawInstruction::LDPC: assert(false && "Unsupported LDPC"); return false;

	case RawInstruction::BRANCH:
	{
		Value *cond = value_for_operand(ctx, op0);
		BasicBlock *bbt = block_for_operand(ctx, op1);
		BasicBlock *bbf = block_for_operand(ctx, op2);

		assert(cond && bbt && bbf);

		cond = ctx.builder.CreateCast(Instruction::Trunc, cond, ctx.i1);
		ctx.builder.CreateCondBr(cond, bbt, bbf);

		return true;
	}

	case RawInstruction::NOP: return true;

	case RawInstruction::TRAP: {
		std::vector<Type *> params;
		params.push_back(ctx.pi8);

		FunctionType *fntype = FunctionType::get(ctx.vtype, params, false);
		Constant *fn = ctx.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("cpu_trap", fntype);

		assert(fn);

		ctx.builder.CreateCall(fn, ctx.cpu_obj);
		return true;
	}

	case RawInstruction::VERIFY: {
		std::vector<Type *> params;
		params.push_back(ctx.pi8);

		FunctionType *fntype = FunctionType::get(ctx.vtype, params, false);
		Constant *fn = ctx.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("jit_verify", fntype);

		assert(fn);

		ctx.builder.CreateCall(fn, ctx.cpu_obj);
		return true;
	}

	case RawInstruction::SET_CPU_MODE: {
		std::vector<Type *> params;
		params.push_back(ctx.pi8);
		params.push_back(ctx.i8);

		FunctionType *fntype = FunctionType::get(ctx.vtype, params, false);
		Constant *fn = ctx.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("cpu_set_mode", fntype);

		assert(fn);

		Value *v1 = value_for_operand(ctx, op0);
		assert(v1);

		ctx.builder.CreateCall2(fn, ctx.cpu_obj, v1);
		return true;
	}

	case RawInstruction::WRITE_DEVICE: {
		std::vector<Type *> params;
		params.push_back(ctx.pi8);
		params.push_back(ctx.i32);
		params.push_back(ctx.i32);
		params.push_back(ctx.i32);

		FunctionType *fntype = FunctionType::get(ctx.vtype, params, false);
		Constant *fn = ctx.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("cpu_write_device", fntype);

		assert(fn);

		Value *v1 = value_for_operand(ctx, op0);
		assert(v1);
		Value *v2 = value_for_operand(ctx, op1);
		assert(v2);
		Value *v3 = value_for_operand(ctx, op2);
		assert(v3);

		ctx.builder.CreateCall4(fn, ctx.cpu_obj, v1, v2, v3);
		return true;
	}

	case RawInstruction::READ_DEVICE: {
		std::vector<Type *> params;
		params.push_back(ctx.pi8);
		params.push_back(ctx.i32);
		params.push_back(ctx.i32);
		params.push_back(ctx.pi32);

		FunctionType *fntype = FunctionType::get(ctx.vtype, params, false);
		Constant *fn = ctx.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("cpu_read_device", fntype);

		assert(fn);

		Value *v1 = value_for_operand(ctx, op0);
		assert(v1);
		Value *v2 = value_for_operand(ctx, op1);
		assert(v2);
		Value *v3 = vreg_for_operand(ctx, op2);
		assert(v3);

		ctx.builder.CreateCall4(fn, ctx.cpu_obj, v1, v2, v3);
		return true;
	}

	case RawInstruction::INVALID: assert(false && "Invalid Instruction"); return false;
	default: assert(false && "Unhandled Instruction"); return false;
	}
}
