#ifdef NDEBUG
#undef NDEBUG
#endif

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
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/Verifier.h>

#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/PassManager.h>

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/DependenceAnalysis.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Analysis/ScalarEvolution.h>

#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Vectorize.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include <map>
#include <vector>

USE_CONTEXT(JIT)
DECLARE_CHILD_CONTEXT(LLVM, JIT);

using namespace captive::jit;
using namespace llvm;

LLVMJIT::LLVMJIT(engine::Engine& engine, util::ThreadPool& worker_threads) : JIT(worker_threads), BlockJIT((JIT&)*this), RegionJIT((JIT&)*this), _engine(engine)
{
}

LLVMJIT::~LLVMJIT()
{
}

bool LLVMJIT::init()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetAsmPrinter();

	return true;
}

Value *LLVMJIT::value_for_operand(LoweringContext& ctx, const RawOperand* oper)
{
	if (oper->type == RawOperand::VREG) {
		LoadInst *load = ctx.builder.CreateLoad(vreg_for_operand(ctx, oper));

		return load;
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
	} else if (oper->type == RawOperand::PC) {
		return ctx.builder.CreateLoad(ctx.pc_ptr);
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
	// DEBUG << "Lowering: " << bc->render();

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
		//ctx.builder.CreateRet(ctx.const32(1));
		ctx.builder.CreateBr(ctx.dispatch_block);
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
		set_aa_metadata(regptr, TAG_CLASS_REGISTER);

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
		set_aa_metadata(regptr, TAG_CLASS_REGISTER);

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
		set_aa_metadata(memptr, TAG_CLASS_MEMORY);

		ctx.builder.CreateStore(ctx.builder.CreateLoad(memptr, true), dst);

		return true;
	}

	case RawInstruction::WRITE_MEM:
	{
		Value *addr = value_for_operand(ctx, op1), *src = value_for_operand(ctx, op0);

		assert(addr && src);

		Value *memptr = ctx.builder.CreateIntToPtr(addr, type_for_operand(ctx, op0, true));
		set_aa_metadata(memptr, TAG_CLASS_MEMORY);

		ctx.builder.CreateStore(src, memptr, true);

		return true;
	}

	case RawInstruction::CMOV: assert(false && "Unsupported CMOV"); return false;

	case RawInstruction::LDPC:
	{
		// TODO: Optimise this
		Value *dst = vreg_for_operand(ctx, op0);

		assert(dst);

		ctx.builder.CreateStore(ctx.builder.CreateLoad(ctx.pc_ptr), dst);

		return true;
	}

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

	default:
		ERROR << "Unhandled Instruction: " << bc->insn.type;
		assert(false && "Unhandled Instruction"); return false;
	}
}

void LLVMJIT::set_aa_metadata(Value *v, metadata_tags tag)
{
	assert(v->getValueID() >= Value::InstructionVal);

	Instruction *inst = (Instruction *)v;
	SmallVector<Metadata *, 1> metadata_data;
	metadata_data.push_back(ValueAsMetadata::get(ConstantInt::get(IntegerType::getInt32Ty(inst->getContext()), (uint32_t)tag)));

	inst->setMetadata("cai", MDNode::get(inst->getContext(), metadata_data));
}

class CaptiveAA : public FunctionPass, public AliasAnalysis
{
public:
	static char ID;

	CaptiveAA() : FunctionPass(ID) {}

	virtual void *getAdjustedAnalysisPointer(AnalysisID PI)
	{
		if (PI == &AliasAnalysis::ID) return (AliasAnalysis *)this;
		return this;
	}

private:
	virtual void getAnalysisUsage(AnalysisUsage &usage) const;
	virtual AliasAnalysis::AliasResult alias(const AliasAnalysis::Location &L1, const AliasAnalysis::Location &L2);
	virtual bool runOnFunction(Function &F);
};

char CaptiveAA::ID = 0;

static RegisterPass<CaptiveAA> P("captive-aa", "Captive-specific Alias Analysis", false, true);
static RegisterAnalysisGroup<AliasAnalysis> G(P);

void CaptiveAA::getAnalysisUsage(AnalysisUsage &usage) const
{
	AliasAnalysis::getAnalysisUsage(usage);
	usage.setPreservesAll();
}

static bool IsInstr(const Value *v)
{
	return (v->getValueID() >= Value::InstructionVal);
}

static bool IsIntToPtr(const Value *v)
{
	return (v->getValueID() == Value::InstructionVal + Instruction::IntToPtr);
}

static bool IsBitCast(const Value *v)
{
	return (v->getValueID() == Value::InstructionVal + Instruction::BitCast);
}

static bool IsAlloca(const Value *v)
{
	return (v->getValueID() == Value::InstructionVal + Instruction::Alloca);
}

AliasAnalysis::AliasResult CaptiveAA::alias(const AliasAnalysis::Location &L1, const AliasAnalysis::Location &L2)
{
	// First heuristic - really easy.  Taken from SCEV-AA
	if (L1.Size == 0 || L2.Size == 0) return NoAlias;

	// Second heuristic - obvious.
	if (L1.Ptr == L2.Ptr) return MustAlias;

	const Value *v1 = L1.Ptr, *v2 = L2.Ptr;

	if (IsAlloca(v1) && IsAlloca(v2)) {
		// They can't alias, because we've already checked for equality above.
		return NoAlias;
	} else if (IsAlloca(v1) || IsAlloca(v2)) {
		const AllocaInst *alloca = IsAlloca(v1) ? (const AllocaInst *)v1 : (const AllocaInst *)v2;
		const Value *other = !IsAlloca(v1) ? v1 : v2;

		// Allocas and IntToPtrs don't alias.
		if (IsIntToPtr(other))
			return AliasAnalysis::NoAlias;
	} else if (IsIntToPtr(v1) && IsIntToPtr(v2)) {
		const MDNode *md1 = ((Instruction *)v1)->getMetadata("cai");
		const MDNode *md2 = ((Instruction *)v1)->getMetadata("cai");

		if (md1 && md2) {
			if (md1->getOperand(0).get() == md2->getOperand(0)) {
				return AliasAnalysis::MayAlias;
			} else {
				return AliasAnalysis::NoAlias;
			}
		} else {
			/*fprintf(stderr, "*** UNDECORATED INTTOPTR\n");
			v1->dump();
			v2->dump();*/
		}
	}

	/*fprintf(stderr, "*** MAY ALIAS\n");
	v1->dump();
	v2->dump();*/

	return AliasAnalysis::MayAlias;
}

bool CaptiveAA::runOnFunction(llvm::Function &F)
{
	bool changed = false;
	AliasAnalysis::InitializeAliasAnalysis(this);

	for (auto& bb : F) {
		for (auto& insn : bb) {
			if (IsBitCast(&insn)) {
				BitCastInst *bc = (BitCastInst *)&insn;

				if (bc->getOperand(0) == &(F.getArgumentList().back())) {
					changed = true;

					SmallVector<Metadata *, 1> metadata_data;
					metadata_data.push_back(ValueAsMetadata::get(ConstantInt::get(IntegerType::getInt32Ty(bc->getContext()), 2)));

					bc->setMetadata("cai", MDNode::get(bc->getContext(), metadata_data));
				}
			}
		}
	}

	return changed;
}

bool LLVMJIT::add_pass(PassManagerBase *pm, Pass *pass)
{
	pm->add(new CaptiveAA());
	pm->add(pass);

	return true;
}

//#define DISABLE_AA

bool LLVMJIT::initialise_pass_manager(llvm::PassManagerBase* pm)
{
#ifdef DISABLE_AA
	PassManagerBuilder optManagerBuilder;

	optManagerBuilder.BBVectorize = false;
	optManagerBuilder.DisableTailCalls = true;
	optManagerBuilder.OptLevel = 3;

	optManagerBuilder.populateModulePassManager(*pm);
#else
	pm->add(createTypeBasedAliasAnalysisPass());

	add_pass(pm, createGlobalOptimizerPass());
	add_pass(pm, createIPSCCPPass());
	add_pass(pm, createDeadArgEliminationPass());
	add_pass(pm, createJumpThreadingPass());

	add_pass(pm, createInstructionCombiningPass());
	add_pass(pm, createCFGSimplificationPass());

	add_pass(pm, createPruneEHPass());
	add_pass(pm, createFunctionInliningPass(0));

	add_pass(pm, createFunctionAttrsPass());
	add_pass(pm, createAggressiveDCEPass());

	add_pass(pm, createArgumentPromotionPass());

	add_pass(pm, createSROAPass(false));

	add_pass(pm, createEarlyCSEPass());
	add_pass(pm, createJumpThreadingPass());
	add_pass(pm, createCorrelatedValuePropagationPass());
	add_pass(pm, createCFGSimplificationPass());
	add_pass(pm, createInstructionCombiningPass());

	add_pass(pm, createTailCallEliminationPass());
	add_pass(pm, createCFGSimplificationPass());

	add_pass(pm, createReassociatePass());
	//add_pass(pm, createLoopRotatePass());

	add_pass(pm, createInstructionCombiningPass());
	add_pass(pm, createIndVarSimplifyPass());
	//add_pass(pm, createLoopIdiomPass());
	//add_pass(pm, createLoopDeletionPass());

	//add_pass(pm, createLoopUnrollPass());

	add_pass(pm, createGVNPass());
	add_pass(pm, createMemCpyOptPass());
	add_pass(pm, createSCCPPass());

	add_pass(pm, createInstructionCombiningPass());
	add_pass(pm, createJumpThreadingPass());
	add_pass(pm, createCorrelatedValuePropagationPass());

	add_pass(pm, createDeadStoreEliminationPass());
	//add_pass(pm, createSLPVectorizerPass());
	add_pass(pm, createAggressiveDCEPass());
	add_pass(pm, createCFGSimplificationPass());
	add_pass(pm, createInstructionCombiningPass());
	add_pass(pm, createBarrierNoopPass());
	//add_pass(pm, createLoopVectorizePass(false));
	add_pass(pm, createInstructionCombiningPass());
	add_pass(pm, createCFGSimplificationPass());
	add_pass(pm, createDeadStoreEliminationPass());
	add_pass(pm, createStripDeadPrototypesPass());
	add_pass(pm, createGlobalDCEPass());
	add_pass(pm, createConstantMergePass());
#endif

	return true;
}
