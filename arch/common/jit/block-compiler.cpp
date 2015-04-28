#include <jit/block-compiler.h>
#include <jit/translation-context.h>
#include <x86/encode.h>
#include <shared-jit.h>
#include <local-memory.h>
#include <printf.h>

using namespace captive::arch::jit;
using namespace captive::arch::x86;
using namespace captive::shared;

BlockCompiler::BlockCompiler()
{

}

static const char *ir_type_names[] = {
	"INVALID",

	"VERIFY",

	"NOP",
	"TRAP",

	"MOV",
	"CMOV",
	"LDPC",

	"ADD",
	"SUB",
	"MUL",
	"DIV",
	"MOD",

	"SHL",
	"SHR",
	"SAR",
	"CLZ",

	"AND",
	"OR",
	"XOR",

	"CMPEQ",
	"CMPNE",
	"CMPGT",
	"CMPGTE",
	"CMPLT",
	"CMPLTE",

	"SX",
	"ZX",
	"TRUNC",

	"READ_REG",
	"WRITE_REG",
	"READ_MEM",
	"WRITE_MEM",
	"READ_MEM_USER",
	"WRITE_MEM_USER",

	"CALL",
	"JMP",
	"BRANCH",
	"RET",

	"SET_CPU_MODE",
	"WRITE_DEVICE",
	"READ_DEVICE",
};

bool BlockCompiler::compile(shared::TranslationBlock& tb, block_txln_fn& fn)
{
	if (!optimise(tb)) {
		printf("jit: optimisation failed\n");
		return false;
	}

	if (!allocate(tb)) {
		printf("jit: register allocation failed\n");
		return false;
	}

	if (!lower(tb, fn)) {
		printf("jit: instruction lowering failed\n");
		return false;
	}

	return true;
}

bool BlockCompiler::optimise(shared::TranslationBlock& tb)
{
	ControlFlowGraph cfg;
	do {
		if (!cfg.build(tb)) {
			return false;
		}
	} while(merge_blocks(tb, cfg));

	return true;
}

bool BlockCompiler::merge_blocks(shared::TranslationBlock& tb, const ControlFlowGraph& cfg)
{
	for (auto block : cfg.blocks()) {
		if (cfg.successors(block).size() == 1) {
			auto succ = cfg.successors(block).front();
			if (cfg.predecessors(succ).size() == 1) {
				printf("candidate for merge: %d -> %d\n", succ, block);

				for (uint32_t i = 0; i < tb.ir_insn_count; i++) {
					if (tb.ir_insn[i].ir_block == succ) {
						tb.ir_insn[i].ir_block = block;
					} else if (tb.ir_insn[i].ir_block == block) {
						if (tb.ir_insn[i].type == IRInstruction::JMP) {
							tb.ir_insn[i].type = IRInstruction::NOP;
						}
					}
				}

				return true;
			}
		}
	}

	return false;
}

bool BlockCompiler::ControlFlowGraph::build(const shared::TranslationBlock& tb)
{
	all_blocks.clear();
	block_succs.clear();
	block_preds.clear();

	for (uint32_t i = 0; i < tb.ir_insn_count; i++) {
		const IRInstruction *insn = &tb.ir_insn[i];

		all_blocks.insert(insn->ir_block);

		switch (insn->type) {
		case IRInstruction::JMP:
			block_succs[insn->ir_block].push_back((IRBlockId)insn->operands[0].value);
			block_preds[(IRBlockId)insn->operands[0].value].push_back(insn->ir_block);
			break;

		case IRInstruction::BRANCH:
			printf("succ of %d == %d\n", insn->ir_block, insn->operands[1].value);
			printf("succ of %d == %d\n", insn->ir_block, insn->operands[2].value);
			printf("pred of %d == %d\n", insn->operands[1].value, insn->ir_block);
			printf("pred of %d == %d\n", insn->operands[2].value, insn->ir_block);
			break;
		}
	}

	return true;
}

bool BlockCompiler::allocate(shared::TranslationBlock& tb)
{
	return true;
}

bool BlockCompiler::lower(shared::TranslationBlock& tb, block_txln_fn& fn)
{
	LocalMemory allocator;
	X86Encoder encoder(allocator);

	//encoder.push(REG_RBP);
	//encoder.mov(REG_RSP, REG_RBP);

	for (uint32_t i = 0; i < tb.ir_insn_count; i++) {
		printf("ir: %d: %s\n", tb.ir_insn[i].ir_block, ir_type_names[tb.ir_insn[i].type]);

		switch (tb.ir_insn[i].type) {
		case IRInstruction::CALL:
			break;

		case IRInstruction::NOP: break;

		case IRInstruction::RET:
			encoder.xorr(REG_RAX, REG_RAX);
			//encoder.leave();
			encoder.ret();
			break;

		default:
			printf("jit: unsupport ir instruction %s\n", ir_type_names[tb.ir_insn[i].type]);
			return false;
		}
	}

	printf("jit: encoding complete, buffer=%p, size=%d\n", encoder.get_buffer(), encoder.get_buffer_size());
	fn = (block_txln_fn)encoder.get_buffer();
	return true;
}
