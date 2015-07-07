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
#include <llvm/IR/InlineAsm.h>

#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/raw_ostream.h>

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

#include <fstream>

USE_CONTEXT(JIT)
DECLARE_CHILD_CONTEXT(LLVM, JIT);

using namespace captive::jit;
using namespace captive::shared;
using namespace llvm;

LLVMJIT::LLVMJIT(engine::Engine& engine, util::ThreadPool& worker_threads) : JIT(worker_threads), _engine(engine)
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

void *LLVMJIT::compile_region(shared::RegionWorkUnit* rwu)
{
	RegionCompilationContext rcc;
	
	rcc.phys_region_index = rwu->region_index;
	rcc.rgn_module = new Module("region", rcc.ctx);
	
	rcc.types.voidty = Type::getVoidTy(rcc.ctx);
	rcc.types.pi1 = (rcc.types.i1 = Type::getInt1Ty(rcc.ctx))->getPointerTo(0);
	rcc.types.pi8 = (rcc.types.i8 = Type::getInt8Ty(rcc.ctx))->getPointerTo(0);
	rcc.types.pi16 = (rcc.types.i16 = Type::getInt16Ty(rcc.ctx))->getPointerTo(0);
	rcc.types.pi32 = (rcc.types.i32 = Type::getInt32Ty(rcc.ctx))->getPointerTo(0);
	rcc.types.pi64 = (rcc.types.i64 = Type::getInt64Ty(rcc.ctx))->getPointerTo(0);
	
	std::vector<Type *> jit_state_elements;
	jit_state_elements.push_back(rcc.types.pi8);					// CPU
	jit_state_elements.push_back(rcc.types.pi8);					// Registers
	jit_state_elements.push_back(rcc.types.i64);					// Register Size
	jit_state_elements.push_back(rcc.types.pi8->getPointerTo(0));	// Region Chaining Table
	jit_state_elements.push_back(rcc.types.pi64);					// Instruction Counter
	jit_state_elements.push_back(rcc.types.pi32);					// ISR
	
	rcc.types.jit_state_ty = StructType::get(rcc.ctx, jit_state_elements, false);
	rcc.types.jit_state_ptr = rcc.types.jit_state_ty->getPointerTo(0);
	
	FunctionType *rgn_func_ty = FunctionType::get(rcc.types.i32, { rcc.types.jit_state_ptr }, false);
	rcc.rgn_func = Function::Create(rgn_func_ty, Function::ExternalLinkage, "region", rcc.rgn_module);
	rcc.rgn_func->addFnAttr(Attribute::NoRedZone);
	
	llvm::Value *jit_state_ptr_val = &(rcc.rgn_func->getArgumentList().front());
	jit_state_ptr_val->setName("sausage");
	
	rcc.entry_block = BasicBlock::Create(rcc.ctx, "entry", rcc.rgn_func);
	rcc.dispatch_block = BasicBlock::Create(rcc.ctx, "dispatch", rcc.rgn_func);
	rcc.exit_normal_block = BasicBlock::Create(rcc.ctx, "exit_normal", rcc.rgn_func);
	rcc.exit_handle_block = BasicBlock::Create(rcc.ctx, "exit_handle_interrupt", rcc.rgn_func);
	rcc.alloca_block = rcc.entry_block;
	
	rcc.builder.SetInsertPoint(rcc.entry_block);
	rcc.jit_state = rcc.builder.CreateLoad(jit_state_ptr_val, "jit_state");
	rcc.cpu_obj = rcc.builder.CreateExtractValue(rcc.jit_state, {0}, "cpu_obj");
	rcc.reg_state = rcc.builder.CreateExtractValue(rcc.jit_state, {1}, "reg_state");

	rcc.pc_ptr = rcc.builder.CreateBitCast(rcc.builder.CreateGEP(rcc.reg_state, rcc.constu64(60)), rcc.types.pi32);
	set_aa_metadata(rcc.pc_ptr, AA_MD_REGISTER, rcc.consti64(60));

	rcc.entry_page = rcc.builder.CreateLShr(rcc.builder.CreateLoad(rcc.pc_ptr), 12);
	rcc.insn_counter = rcc.builder.CreateExtractValue(rcc.jit_state, {4}, "insn_counter");
	rcc.isr = rcc.builder.CreateExtractValue(rcc.jit_state, {5}, "isr");
	set_aa_metadata(rcc.isr, AA_MD_ISR);
	
	//SwitchInst *first_dispatch = rcc.builder.CreateSwitch(rcc.builder.CreateAnd(rcc.builder.CreateLoad(rcc.pc_ptr), rcc.constu32(0xfff), "pc_offset"), rcc.miss_block);
	
	rcc.builder.CreateBr(rcc.dispatch_block);

	// --- EXIT NORMAL
	rcc.builder.SetInsertPoint(rcc.exit_normal_block);
	rcc.builder.CreateRet(rcc.constu32(0));
	
	// --- EXIT HANDLE
	rcc.builder.SetInsertPoint(rcc.exit_handle_block);
	rcc.builder.CreateRet(rcc.constu32(1));
	
	BasicBlock *do_dispatch_block = BasicBlock::Create(rcc.ctx, "do_dispatch", rcc.rgn_func);

	// --- DISPATCH
	rcc.builder.SetInsertPoint(rcc.dispatch_block);
	Value *dispatch_loaded_pc = rcc.builder.CreateLoad(rcc.pc_ptr);
	rcc.builder.CreateCondBr(rcc.builder.CreateICmpEQ(rcc.builder.CreateLShr(dispatch_loaded_pc, 12), rcc.entry_page), do_dispatch_block, rcc.exit_normal_block);
	
	uint32_t jump_table_slots = 2048;
	for (uint32_t slot = 0; slot < jump_table_slots; slot++) {
		rcc.jump_table_entries[slot] = (Constant *)BlockAddress::get(rcc.exit_normal_block);
	}
	
	// Create Blocks
	for (uint32_t i = 0; i < rwu->block_count; i++) {
		BasicBlock *gbb = BasicBlock::Create(rcc.ctx, "gbb-" + std::to_string(rwu->blocks[i].offset), rcc.rgn_func);
		
		/*first_dispatch->addCase(rcc.constu32(rwu->blocks[i].offset), gbb);
		second_dispatch->addCase(rcc.constu32(rwu->blocks[i].offset), gbb);*/
		
		rcc.guest_basic_blocks[rwu->blocks[i].offset] = gbb;
		rcc.jump_table_entries[rwu->blocks[i].offset >> 1] = (Constant *)BlockAddress::get(gbb);
	}
	
	ArrayType *jump_table_type = ArrayType::get(rcc.types.pi8, jump_table_slots);
	Constant *jump_table_init = ConstantArray::get(jump_table_type, ArrayRef<Constant *>(rcc.jump_table_entries, jump_table_slots));
	rcc.jump_table = new GlobalVariable(*rcc.rgn_module, jump_table_type, true, GlobalValue::InternalLinkage, jump_table_init, "jump_table");

	// --- DO DISPATCH
	rcc.builder.SetInsertPoint(do_dispatch_block);
	Value *jt_slot = rcc.builder.CreateLShr(rcc.builder.CreateAnd(dispatch_loaded_pc, 0xfff), 1);
	
	std::vector<Value *> indicies;
	indicies.push_back(rcc.constu32(0));
	indicies.push_back(jt_slot);
	
	Value *jt_elem = rcc.builder.CreateInBoundsGEP(rcc.jump_table, indicies, "jt_element");
	set_aa_metadata(jt_elem, AA_MD_JT_ELEM);
	
	IndirectBrInst *ibr = rcc.builder.CreateIndirectBr(rcc.builder.CreateLoad(jt_elem), rwu->block_count + 1);
	ibr->addDestination(rcc.exit_normal_block);
	
	//SwitchInst *second_dispatch = rcc.builder.CreateSwitch(rcc.builder.CreateAnd(rcc.builder.CreateLoad(rcc.pc_ptr), rcc.constu32(0xfff), "pc_offset"), rcc.exit_block);
	
	// Lower Blocks
	for (uint32_t i = 0; i < rwu->block_count; i++) {
		ibr->addDestination(rcc.guest_basic_blocks[rwu->blocks[i].offset]);
		lower_block(rcc, &rwu->blocks[i]);
	}
	
	// Verify
	{
		PassManager verifyManager;
		verifyManager.add(createVerifierPass(true));
		verifyManager.run(*rcc.rgn_module);
	}
	
	// Dump
	/*{
		std::stringstream filename;
		filename << "region-" << std::hex << (uint64_t)(rwu->region_index << 12) << ".ll";
		print_module(filename.str(), rcc.rgn_module);
	}*/
	
	// Optimise
	{
		PassManager optManager;
		initialise_pass_manager(&optManager);

		optManager.run(*rcc.rgn_module);
	}

	// Dump
	/*{
		std::stringstream filename;
		filename << "region-" << std::hex << (uint64_t)(rwu->region_index << 12) << ".opt.ll";
		print_module(filename.str(), rcc.rgn_module);
	}*/
	
	// Initialise a new MCJIT engine
	TargetOptions target_opts;
	target_opts.DisableTailCalls = false;
	target_opts.PositionIndependentExecutable = true;
	target_opts.EnableFastISel = false;
	target_opts.NoFramePointerElim = true;
	target_opts.PrintMachineCode = false;

	ExecutionEngine *engine = EngineBuilder(std::unique_ptr<Module>(rcc.rgn_module))
		.setEngineKind(EngineKind::JIT)
		.setMCJITMemoryManager(std::unique_ptr<RTDyldMemoryManager>(new LLVMJITMemoryManager(_engine, *_shared_memory)))
		.setTargetOptions(target_opts)
		.create();

	if (!engine) {
		return NULL;
	}

	// We actually want compilation to happen.
	engine->DisableLazyCompilation();
	engine->finalizeObject();

	// Compile the function!
	void *ptr = (void *)engine->getFunctionAddress(rcc.rgn_func->getName());
	if (!ptr) {
		return NULL;
	}

	rcc.rgn_func->deleteBody();
	delete engine;

	return ptr;
}

void LLVMJIT::print_module(std::string filename, Module *module)
{
	std::ofstream file(filename);
	raw_os_ostream str(file);

	PassManager printManager;
	printManager.add(createPrintModulePass(str, ""));
	printManager.run(*module);
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

static bool IsGEP(const Value *v)
{
	return (v->getValueID() == Value::InstructionVal + Instruction::GetElementPtr);
}

static bool IsBitCast(const Value *v)
{
	return (v->getValueID() == Value::InstructionVal + Instruction::BitCast);
}

static bool IsAlloca(const Value *v)
{
	return (v->getValueID() == Value::InstructionVal + Instruction::Alloca);
}

static bool IsExtractValue(const Value *v)
{
	return (v->getValueID() == Value::InstructionVal + Instruction::ExtractValue);
}

static bool IsConstVal(const ::llvm::Value *v)
{
	return (v->getValueID() == ::llvm::Instruction::ConstantIntVal);
}

#define CONSTVAL(a) (assert((a)->getValueID() == Instruction::ConstantIntVal), (((ConstantInt *)(a))->getZExtValue()))

AliasAnalysis::AliasResult CaptiveAA::alias(const AliasAnalysis::Location &L1, const AliasAnalysis::Location &L2)
{
	// First heuristic - really easy.  Taken from SCEV-AA
	if (L1.Size == 0 || L2.Size == 0) return NoAlias;

	// Second heuristic - obvious.
	if (L1.Ptr == L2.Ptr) return MustAlias;

	const Value *v1 = L1.Ptr, *v2 = L2.Ptr;
	
	// First, if we've got two instructions...
	if (IsInstr(v1) && IsInstr(v2)) {
		const MDNode *md1 = ((Instruction *)v1)->getMetadata("cai");
		const MDNode *md2 = ((Instruction *)v2)->getMetadata("cai");

		// And they both have metadata...
		if (md1 && md2) {
			// And the metadata has different tags...
			if (md1->getOperand(0).get() != md2->getOperand(0).get()) {
				// Then they definitely DO NOT alias!
				return AliasAnalysis::NoAlias;
			}
		}
	}
	
	if (v1->getName() == "sausage" || v2->getName() == "sausage") return AliasAnalysis::NoAlias;

	if (IsAlloca(v1) || IsAlloca(v2)) {
		// They can't alias with anything, because we've already checked for equality above.
		return NoAlias;
	} else if ((IsGEP(v1) || IsBitCast(v1)) && (IsGEP(v2) || IsBitCast(v2))) {
		const MDNode *md1 = ((Instruction *)v1)->getMetadata("cai");
		const MDNode *md2 = ((Instruction *)v2)->getMetadata("cai");

		if (md1 && md2) {
			if (md1->getOperand(0).get() == md2->getOperand(0).get()) {
				uint32_t aa_class = CONSTVAL(((ConstantAsMetadata *)md1->getOperand(0).get())->getValue());

				if (aa_class == 2) { // Register
					if (md1->getNumOperands() > 1 && md2->getNumOperands() > 1) {
						uint32_t reg_offset1 = CONSTVAL(((ValueAsMetadata *)md1->getOperand(1).get())->getValue());
						uint32_t reg_offset2 = CONSTVAL(((ValueAsMetadata *)md2->getOperand(1).get())->getValue());

						// This would break if we got some weird overlapping offsets
						// but, of course, we never generate rubbish code like that!
						if (reg_offset1 == reg_offset2) {
							return AliasAnalysis::MustAlias;
						} else {
							return AliasAnalysis::NoAlias;
						}
					}
				}

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

	/*fprintf(stderr, "*** MAY ALIAS '%s' and '%s'\n", v1->getName().str().c_str(), v2->getName().str().c_str());
	v1->dump();
	v2->dump();*/
	
	return AliasAnalysis::MayAlias;
}

bool CaptiveAA::runOnFunction(Function& F)
{
	AliasAnalysis::InitializeAliasAnalysis(this);
	
	/*bool changed = false;
	for (auto& bb : F) {
		for (auto& insn : bb) {
			if (insn.getMetadata("cai") != NULL) continue;

			if (IsBitCast(&insn)) {
				BitCastInst *bc = (BitCastInst *)&insn;

				// FIXME:  HACKY HACK MCHACK
				if (bc->getOperand(0)->getName() == "reg_state") {
					changed = true;

					SmallVector<Metadata *, 2> metadata_data;
					metadata_data.push_back(ValueAsMetadata::get(ConstantInt::get(IntegerType::getInt32Ty(bc->getContext()), 2)));
					metadata_data.push_back(ConstantAsMetadata::get(ConstantInt::get(IntegerType::getInt32Ty(bc->getContext()), 0)));

					bc->setMetadata("cai", MDNode::get(bc->getContext(), metadata_data));
				}
			}
		}
	}*/
	
	return false;
}


bool LLVMJIT::add_pass(PassManagerBase *pm, Pass *pass)
{
	pm->add(new CaptiveAA());
	pm->add(pass);

	return true;
}

//#define DISABLE_AA

bool LLVMJIT::initialise_pass_manager(PassManagerBase* pm)
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
	//add_pass(pm, createLoopRotatePass());					//

	add_pass(pm, createInstructionCombiningPass());
	add_pass(pm, createIndVarSimplifyPass());
	//add_pass(pm, createLoopIdiomPass());					//
	//add_pass(pm, createLoopDeletionPass());				//

	//add_pass(pm, createLoopUnrollPass());					//

	add_pass(pm, createGVNPass());
	add_pass(pm, createMemCpyOptPass());
	add_pass(pm, createSCCPPass());

	add_pass(pm, createInstructionCombiningPass());
	add_pass(pm, createJumpThreadingPass());
	add_pass(pm, createCorrelatedValuePropagationPass());

	add_pass(pm, createDeadStoreEliminationPass());
	//add_pass(pm, createSLPVectorizerPass());				//
	add_pass(pm, createAggressiveDCEPass());
	add_pass(pm, createCFGSimplificationPass());
	add_pass(pm, createInstructionCombiningPass());
	add_pass(pm, createBarrierNoopPass());
	//add_pass(pm, createLoopVectorizePass(false));			//
	add_pass(pm, createInstructionCombiningPass());
	add_pass(pm, createCFGSimplificationPass());
	add_pass(pm, createDeadStoreEliminationPass());
	add_pass(pm, createStripDeadPrototypesPass());
	add_pass(pm, createGlobalDCEPass());
	add_pass(pm, createConstantMergePass());
#endif

	return true;
}

bool LLVMJIT::lower_block(RegionCompilationContext& rcc, const shared::BlockWorkUnit *bwu)
{
	BlockCompilationContext bcc(rcc);
	
	BasicBlock *entry_block = rcc.guest_basic_blocks[bwu->offset];
	bcc.alloca_block = rcc.alloca_block;
	
	for (uint32_t ir_idx = 0; ir_idx < bwu->ir_count; ir_idx++) {
		const IRInstruction *insn = &bwu->ir[ir_idx];
		lower_instruction(bcc, insn);
	}
	
	bcc.builder.SetInsertPoint(entry_block);

	if (bwu->interrupt_check) {
		std::vector<Type *> params;
		params.push_back(rcc.types.pi8);
		params.push_back(rcc.types.i32);

		FunctionType *fntype = FunctionType::get(rcc.types.i32, params, false);

		Constant *fn = rcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("jit_handle_interrupt", fntype);
		Value *interrupt_handled = rcc.builder.CreateCall2(fn, rcc.cpu_obj, rcc.builder.CreateLoad(rcc.isr));

		rcc.builder.CreateCondBr(rcc.builder.CreateICmpEQ(interrupt_handled, rcc.consti32(0)), get_ir_block(bcc, (IRBlockId)0), rcc.exit_handle_block);
	} else {
		rcc.builder.CreateBr(get_ir_block(bcc, (IRBlockId)0));
	}
	
	return true;
}

void LLVMJIT::set_aa_metadata(Value *v, metadata_tags tag)
{
	assert(v->getValueID() >= Value::InstructionVal);

	Instruction *inst = (Instruction *)v;
	SmallVector<Metadata *, 1> metadata_data;
	metadata_data.push_back(ConstantAsMetadata::get(ConstantInt::get(IntegerType::getInt32Ty(inst->getContext()), (uint32_t)tag)));

	inst->setMetadata("cai", MDNode::get(inst->getContext(), metadata_data));
}

void LLVMJIT::set_aa_metadata(Value *v, metadata_tags tag, Value *value)
{
	assert(v->getValueID() >= Value::InstructionVal);

	Instruction *inst = (Instruction *)v;
	SmallVector<Metadata *, 2> metadata_data;
	metadata_data.push_back(ConstantAsMetadata::get(ConstantInt::get(IntegerType::getInt32Ty(inst->getContext()), (uint32_t)tag)));
	metadata_data.push_back(ValueAsMetadata::get(value));

	inst->setMetadata("cai", MDNode::get(inst->getContext(), metadata_data));
}

Value *LLVMJIT::value_for_operand(BlockCompilationContext& bcc, const IROperand *oper)
{
	if (oper->is_vreg()) {
		return bcc.builder.CreateLoad(vreg_for_operand(bcc, oper));
	} else if (oper->is_constant()) {
		switch(oper->size) {
		case 1: return bcc.rcc.constu8(oper->value);
		case 2: return bcc.rcc.constu16(oper->value);
		case 4: return bcc.rcc.constu32(oper->value);
		case 8: return bcc.rcc.constu64(oper->value);
		default: return NULL;
		}
	} else if (oper->is_func()) {
		return bcc.rcc.constu64(oper->value);
	} else if (oper->is_pc()) {
		return bcc.rcc.builder.CreateLoad(bcc.rcc.pc_ptr);
	} else {
		return NULL;
	}
}

Type *LLVMJIT::type_for_operand(BlockCompilationContext& bcc, const IROperand *oper, bool ptr)
{
	switch (oper->size) {
	case 1: return ptr ? bcc.rcc.types.pi8 : bcc.rcc.types.i8;
	case 2: return ptr ? bcc.rcc.types.pi16 : bcc.rcc.types.i16;
	case 4: return ptr ? bcc.rcc.types.pi32 : bcc.rcc.types.i32;
	case 8: return ptr ? bcc.rcc.types.pi64 : bcc.rcc.types.i64;
	default: assert(false); return NULL;
	}
}

llvm::Value* LLVMJIT::get_ir_vreg(BlockCompilationContext& bcc, shared::IRRegId id, uint8_t size)
{
	auto llvm_vreg = bcc.ir_vregs.find(id);
	if (llvm_vreg == bcc.ir_vregs.end()) {
		IRBuilder<> alloca_block_builder(bcc.alloca_block);
		alloca_block_builder.SetInsertPoint(&(bcc.alloca_block->back()));

		Type *vreg_type = NULL;
		switch (size) {
		case 1: vreg_type = bcc.rcc.types.i8; break;
		case 2: vreg_type = bcc.rcc.types.i16; break;
		case 4: vreg_type = bcc.rcc.types.i32; break;
		case 8: vreg_type = bcc.rcc.types.i64; break;
		default: assert(false); return NULL;
		}

		AllocaInst *vreg_alloc = alloca_block_builder.CreateAlloca(vreg_type, NULL, "vreg-" + std::to_string(id));
		set_aa_metadata(vreg_alloc, AA_MD_VREG);
		
		bcc.ir_vregs[(uint32_t)id] = vreg_alloc;
		
		return vreg_alloc;
	} else {
		return llvm_vreg->second;
	}
}

Value *LLVMJIT::vreg_for_operand(BlockCompilationContext& bcc, const shared::IROperand* oper)
{
	if (oper->is_vreg()) {
		return get_ir_vreg(bcc, (IRRegId)oper->value, oper->size);
	} else {
		return NULL;
	}
}

BasicBlock *LLVMJIT::get_ir_block(BlockCompilationContext& bcc, IRBlockId id)
{
	auto llvm_block = bcc.ir_blocks.find(id);
	if (llvm_block == bcc.ir_blocks.end()) {
		BasicBlock *new_block = BasicBlock::Create(bcc.rcc.ctx, "bb-" + std::to_string(id), bcc.rcc.rgn_func);
		bcc.ir_blocks[(uint32_t)id] = new_block;
		return new_block;
	} else {
		return llvm_block->second;
	}
}

BasicBlock *LLVMJIT::block_for_operand(BlockCompilationContext& bcc, const IROperand *oper)
{
	if (oper->is_block()) {
		return get_ir_block(bcc, (IRBlockId)oper->value);
	} else {
		return NULL;
	}
}

bool LLVMJIT::lower_instruction(BlockCompilationContext& bcc, const shared::IRInstruction* insn)
{
	const shared::IROperand *op0 = &insn->operands[0];
	const shared::IROperand *op1 = &insn->operands[1];
	const shared::IROperand *op2 = &insn->operands[2];
	
	bcc.builder.SetInsertPoint(get_ir_block(bcc, insn->ir_block));
	
	switch (insn->type) {
	case IRInstruction::COUNT:
	{
		Value *counter_val = bcc.builder.CreateLoad(bcc.rcc.insn_counter);
		bcc.builder.CreateStore(bcc.builder.CreateAdd(bcc.rcc.consti64(1), counter_val), bcc.rcc.insn_counter);
		
		return true;
	}
	
	case IRInstruction::INCPC:
	{
		Value *pc_val = bcc.builder.CreateLoad(bcc.rcc.pc_ptr);
		bcc.builder.CreateStore(bcc.builder.CreateAdd(value_for_operand(bcc, op0), pc_val), bcc.rcc.pc_ptr);
		
		return true;		
	}
	
	case shared::IRInstruction::LDPC:
	{
		Value *dst = vreg_for_operand(bcc, op0);
		assert(dst);

		bcc.builder.CreateStore(bcc.builder.CreateLoad(bcc.rcc.pc_ptr), dst);
		return true;
	}

	case IRInstruction::MOV:
	{
		Value *src = value_for_operand(bcc, op0), *dst = vreg_for_operand(bcc, op1);
		assert(src && dst);
		bcc.builder.CreateStore(src, dst);
		
		return true;
	}
	
	// Arithmetic
	case shared::IRInstruction::ADD:
	case shared::IRInstruction::SUB:
	case shared::IRInstruction::MUL:
	case shared::IRInstruction::DIV:
	case shared::IRInstruction::MOD:
	case shared::IRInstruction::SHL:
	case shared::IRInstruction::SHR:
	case shared::IRInstruction::SAR:
	case shared::IRInstruction::AND:
	case shared::IRInstruction::OR:
	case shared::IRInstruction::XOR:
	{
		Value *src = value_for_operand(bcc, op0), *dst = vreg_for_operand(bcc, op1);
		assert(src && dst);

		Value *lhs = bcc.builder.CreateLoad(dst);
		Value *rhs = src;

		Value *result = NULL;
		switch (insn->type) {
		case shared::IRInstruction::ADD: result = bcc.builder.CreateAdd(lhs, rhs); break;
		case shared::IRInstruction::SUB: result = bcc.builder.CreateSub(lhs, rhs); break;
		case shared::IRInstruction::MUL: result = bcc.builder.CreateMul(lhs, rhs); break;
		case shared::IRInstruction::DIV: result = bcc.builder.CreateUDiv(lhs, rhs); break;
		case shared::IRInstruction::MOD: result = bcc.builder.CreateURem(lhs, rhs); break;

		case shared::IRInstruction::SHL: result = bcc.builder.CreateShl(lhs, rhs); break;
		case shared::IRInstruction::SHR: result = bcc.builder.CreateLShr(lhs, rhs); break;
		case shared::IRInstruction::SAR: result = bcc.builder.CreateAShr(lhs, rhs); break;

		case shared::IRInstruction::AND: result = bcc.builder.CreateAnd(lhs, rhs); break;
		case shared::IRInstruction::OR:  result = bcc.builder.CreateOr(lhs, rhs); break;
		case shared::IRInstruction::XOR: result = bcc.builder.CreateXor(lhs, rhs); break;

		default: assert(false);
		}

		assert(result);
		bcc.builder.CreateStore(result, dst);
		return true;
	}
	
	// Comparison
	case IRInstruction::CMPEQ:
	case IRInstruction::CMPNE:
	case IRInstruction::CMPLT:
	case IRInstruction::CMPLTE:
	case IRInstruction::CMPGT:
	case IRInstruction::CMPGTE:
	{
		Value *lh = value_for_operand(bcc, op0), *rh = value_for_operand(bcc, op1), *dst = vreg_for_operand(bcc, op2);
		assert(lh && rh && dst);

		Value *result = NULL;
		switch(insn->type) {
		case IRInstruction::CMPEQ: result = bcc.builder.CreateICmpEQ(lh, rh); break;
		case IRInstruction::CMPNE: result = bcc.builder.CreateICmpNE(lh, rh); break;

		case IRInstruction::CMPLT: result = bcc.builder.CreateICmpULT(lh, rh); break;
		case IRInstruction::CMPLTE: result = bcc.builder.CreateICmpULE(lh, rh); break;

		case IRInstruction::CMPGT: result = bcc.builder.CreateICmpUGT(lh, rh); break;
		case IRInstruction::CMPGTE: result = bcc.builder.CreateICmpUGE(lh, rh); break;

		default: assert(false);
		}

		result = bcc.builder.CreateCast(Instruction::ZExt, result, bcc.rcc.types.i8);
		bcc.builder.CreateStore(result, dst);

		return true;
	}
	
	case IRInstruction::CALL:
	{
		std::vector<Type *> param_types;
		param_types.push_back(bcc.rcc.types.pi8);

		std::vector<Value *> param_vals;
		param_vals.push_back(bcc.rcc.cpu_obj);

		for (int i = 1; i < 6; i++) {
			const IROperand *oper = &insn->operands[i];
			if (oper->type == shared::IROperand::NONE) continue;

			switch (oper->size) {
			case 1: param_types.push_back(bcc.rcc.types.i8); param_vals.push_back(value_for_operand(bcc, oper)); break;
			case 2: param_types.push_back(bcc.rcc.types.i16); param_vals.push_back(value_for_operand(bcc, oper)); break;
			case 4: param_types.push_back(bcc.rcc.types.i32); param_vals.push_back(value_for_operand(bcc, oper)); break;
			case 8: param_types.push_back(bcc.rcc.types.i64); param_vals.push_back(value_for_operand(bcc, oper)); break;
			default: assert(false);
			}
		}

		PointerType *fntype = FunctionType::get(bcc.rcc.types.voidty, param_types, false)->getPointerTo(0);
		assert(fntype);

		Value *fn = value_for_operand(bcc, op0);
		assert(fn);

		fn = bcc.builder.CreateIntToPtr(fn, fntype);
		assert(fn);
		
		bcc.builder.CreateCall(fn, param_vals);
		return true;
	}
	
	case shared::IRInstruction::CLZ:
	{
		std::vector<Type *> params;
		params.push_back(bcc.rcc.types.i32);
		params.push_back(bcc.rcc.types.i1);

		FunctionType *fntype = FunctionType::get(bcc.rcc.types.i32, params, false);
		Constant *fn = bcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("llvm.ctlz.i32", fntype);
		assert(fn);

		Value *src = value_for_operand(bcc, op0), *dst = vreg_for_operand(bcc, op1);
		assert(src && dst);

		Value *result = bcc.builder.CreateCall2(fn, src, bcc.rcc.consti1(0));
		bcc.builder.CreateStore(result, dst);

		return true;
	}
	
	case shared::IRInstruction::SX:
	case shared::IRInstruction::ZX:
	case shared::IRInstruction::TRUNC:
	{
		Value *src = value_for_operand(bcc, op0), *dst = vreg_for_operand(bcc, op1);

		Instruction::CastOps op = Instruction::Trunc;
		if (insn->type == shared::IRInstruction::SX) {
			op = Instruction::SExt;
		} else if (insn->type == shared::IRInstruction::ZX) {
			op = Instruction::ZExt;
		}

		bcc.builder.CreateStore(bcc.builder.CreateCast(op, src, type_for_operand(bcc, op1, false)), dst);
		return true;
	}
	
	case shared::IRInstruction::READ_REG:
	{
		Value *offset = value_for_operand(bcc, op0), *dst = vreg_for_operand(bcc, op1);

		assert(offset && dst);

		/*offset = bcc.builder.CreateCast(Instruction::ZExt, offset, bcc.rcc.types.i64);

		Value *regptr = bcc.builder.CreateIntToPtr(bcc.builder.CreateAdd(bcc.rcc.reg_state, offset), type_for_operand(bcc, op1, true));
		set_aa_metadata(regptr, AA_MD_REGISTER, offset);

		bcc.builder.CreateStore(bcc.builder.CreateLoad(regptr), dst);*/
				
		Value *regptr = bcc.builder.CreateBitCast(bcc.builder.CreateGEP(bcc.rcc.reg_state, offset), type_for_operand(bcc, op1, true));
		set_aa_metadata(regptr, AA_MD_REGISTER, offset);
		
		bcc.builder.CreateStore(bcc.builder.CreateLoad(regptr), dst);

		return true;
	}

	case shared::IRInstruction::WRITE_REG:
	{
		Value *offset = value_for_operand(bcc, op1), *val = value_for_operand(bcc, op0);

		assert(offset && val);

		/*offset = bcc.builder.CreateCast(Instruction::ZExt, offset, bcc.rcc.types.i64);

		Value *regptr = bcc.builder.CreateAdd(bcc.rcc.reg_state, offset);
		regptr = bcc.builder.CreateIntToPtr(regptr, type_for_operand(bcc, op0, true));
		set_aa_metadata(regptr, AA_MD_REGISTER, offset);

		bcc.builder.CreateStore(val, regptr);*/
		
		Value *regptr = bcc.builder.CreateBitCast(bcc.builder.CreateGEP(bcc.rcc.reg_state, offset), type_for_operand(bcc, op0, true));
		set_aa_metadata(regptr, AA_MD_REGISTER, offset);
		
		bcc.builder.CreateStore(val, regptr);

		return true;
	}

	case shared::IRInstruction::READ_MEM:
	case shared::IRInstruction::READ_MEM_USER:
	{
		Value *addr = value_for_operand(bcc, op0), *dst = vreg_for_operand(bcc, op1);
		assert(addr && dst);

		if (insn->type == shared::IRInstruction::READ_MEM) {
			Type *memptrtype = type_for_operand(bcc, op1, true);
			assert(memptrtype);

			Value *memptr = bcc.builder.CreateIntToPtr(addr, memptrtype);
			set_aa_metadata(memptr, AA_MD_MEMORY);

			bcc.builder.CreateStore(bcc.builder.CreateLoad(memptr, true), dst);
		} else {
			Type *dst_type = type_for_operand(bcc, op1, true);

			std::vector<Type *> params;
			params.push_back(bcc.rcc.types.pi8);
			params.push_back(bcc.rcc.types.i32);
			params.push_back(dst_type);

			FunctionType *fntype = FunctionType::get(bcc.rcc.types.voidty, params, false);

			Constant *fn = NULL;
			if (dst_type == bcc.rcc.types.pi8) {
				fn = bcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("mem_user_read8", fntype);
			} else if (dst_type == bcc.rcc.types.pi16) {
				fn = bcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("mem_user_read16", fntype);
			} else {
				fn = bcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("mem_user_read32", fntype);
			}

			bcc.builder.CreateCall3(fn, bcc.rcc.cpu_obj, addr, dst);
		}
		return true;
	}

	case shared::IRInstruction::WRITE_MEM:
	case shared::IRInstruction::WRITE_MEM_USER:
	{
		Value *addr = value_for_operand(bcc, op1), *src = value_for_operand(bcc, op0);

		assert(addr && src);

		if (insn->type == shared::IRInstruction::WRITE_MEM) {
			Value *memptr = bcc.builder.CreateIntToPtr(addr, type_for_operand(bcc, op0, true));
			set_aa_metadata(memptr, AA_MD_MEMORY);

			bcc.builder.CreateStore(src, memptr, true);
		} else {
			Type *src_type = type_for_operand(bcc, op0, false);

			std::vector<Type *> params;
			params.push_back(bcc.rcc.types.pi8);
			params.push_back(bcc.rcc.types.i32);
			params.push_back(src_type);

			FunctionType *fntype = FunctionType::get(bcc.rcc.types.voidty, params, false);

			Constant *fn = NULL;
			if (src_type == bcc.rcc.types.i8) {
				fn = bcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("mem_user_write8", fntype);
			} else if (src_type == bcc.rcc.types.i16) {
				fn = bcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("mem_user_write16", fntype);
			} else {
				fn = bcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("mem_user_write32", fntype);
			}

			bcc.builder.CreateCall3(fn, bcc.rcc.cpu_obj, addr, src);
		}

		return true;
	}

	case shared::IRInstruction::SET_CPU_MODE: {
		std::vector<Type *> params;
		params.push_back(bcc.rcc.types.pi8);
		params.push_back(bcc.rcc.types.i8);

		FunctionType *fntype = FunctionType::get(bcc.rcc.types.voidty, params, false);
		Constant *fn = bcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("cpu_set_mode", fntype);

		assert(fn);

		Value *v1 = value_for_operand(bcc, op0);
		assert(v1);

		bcc.builder.CreateCall2(fn, bcc.rcc.cpu_obj, v1);
		return true;
	}

	case shared::IRInstruction::WRITE_DEVICE: {
		std::vector<Type *> params;
		params.push_back(bcc.rcc.types.pi8);
		params.push_back(bcc.rcc.types.i32);
		params.push_back(bcc.rcc.types.i32);
		params.push_back(bcc.rcc.types.i32);

		FunctionType *fntype = FunctionType::get(bcc.rcc.types.voidty, params, false);
		Constant *fn = bcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("cpu_write_device", fntype);

		assert(fn);

		Value *v1 = value_for_operand(bcc, op0);
		assert(v1);
		Value *v2 = value_for_operand(bcc, op1);
		assert(v2);
		Value *v3 = value_for_operand(bcc, op2);
		assert(v3);

		bcc.builder.CreateCall4(fn, bcc.rcc.cpu_obj, v1, v2, v3);
		return true;
	}

	case shared::IRInstruction::READ_DEVICE: {
		std::vector<Type *> params;
		params.push_back(bcc.rcc.types.pi8);
		params.push_back(bcc.rcc.types.i32);
		params.push_back(bcc.rcc.types.i32);
		params.push_back(bcc.rcc.types.pi32);

		FunctionType *fntype = FunctionType::get(bcc.rcc.types.voidty, params, false);
		Constant *fn = bcc.builder.GetInsertBlock()->getParent()->getParent()->getOrInsertFunction("cpu_read_device", fntype);

		assert(fn);

		Value *v1 = value_for_operand(bcc, op0);
		assert(v1);
		Value *v2 = value_for_operand(bcc, op1);
		assert(v2);
		Value *v3 = vreg_for_operand(bcc, op2);
		assert(v3);

		bcc.builder.CreateCall4(fn, bcc.rcc.cpu_obj, v1, v2, v3);
		return true;
	}
	
	// Control Flow
	case IRInstruction::JMP:
	{
		bcc.builder.CreateBr(block_for_operand(bcc, op0));
		return true;	
	}	

	case IRInstruction::BRANCH:
	{
		Value *cond = value_for_operand(bcc, op0);
		BasicBlock *tt = block_for_operand(bcc, op1);
		BasicBlock *ft = block_for_operand(bcc, op2);
		
		bcc.builder.CreateCondBr(bcc.builder.CreateCast(Instruction::Trunc, cond, bcc.rcc.types.i1), tt, ft);
		return true;	
	}
	
	case IRInstruction::DISPATCH:
	{
		assert(op0->is_constant() && op1->is_constant());
		uint32_t target_address = op0->value;
		uint32_t fallthrough_address = op1->value;
		
		BasicBlock *target_block = bcc.rcc.exit_normal_block;
		BasicBlock *fallthrough_block = bcc.rcc.exit_normal_block;
		
		if (target_address >> 12 == bcc.rcc.phys_region_index) {
			if (bcc.rcc.guest_basic_blocks.count(target_address & 0xfff)) {
				target_block = bcc.rcc.guest_basic_blocks[target_address & 0xfff];
			}
		}
		
		if (fallthrough_address >> 12 == bcc.rcc.phys_region_index) {
			if (bcc.rcc.guest_basic_blocks.count(fallthrough_address & 0xfff)) {
				fallthrough_block = bcc.rcc.guest_basic_blocks[fallthrough_address & 0xfff];
			}
		}
				
		if (fallthrough_address == 0) {
			// Non-predicated Direct
			bcc.builder.CreateBr(target_block);
		} else {
			// Predicated Direct
			Value *target_match = bcc.builder.CreateICmpEQ(bcc.builder.CreateAnd(bcc.builder.CreateLoad(bcc.rcc.pc_ptr), 0xfff), bcc.rcc.constu32(target_address & 0xfff));
			bcc.builder.CreateCondBr(target_match, target_block, fallthrough_block);
		}
		
		return true;
	}
	
	case IRInstruction::RET:
	{
		bcc.builder.CreateBr(bcc.rcc.dispatch_block);
		return true;
	}
	
	case IRInstruction::NOP: return true;
	
	default: ERROR << "Unhandled instruction type: " << insn->type; assert(false);
	//default: return false;
	}
}
