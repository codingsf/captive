#include <jit/ir.h>

using namespace captive::arch::jit;

IRContext::IRContext()
{

}

IRContext::~IRContext()
{
	for (auto block : _blocks) {
		delete block.second;
	}
}

IRBlock& IRContext::get_block_by_id(shared::IRBlockId id)
{
	block_map_t::iterator iter = _blocks.find(id);
	if (iter == _blocks.end()) {
		IRBlock *new_block = new IRBlock(*this, id);
		_blocks[id] = new_block;
		return *new_block;
	} else {
		return *(iter->second);
	}
}

IRRegister& IRContext::get_register_by_id(shared::IRRegId id, uint8_t width)
{
	reg_map_t::iterator iter = _registers.find(id);
	if (iter == _registers.end()) {
		IRRegister *new_reg = new IRRegister(*this, id, width);
		_registers[id] = new_reg;
		return *new_reg;
	} else {
		assert(iter->second->width() == width);
		return *(iter->second);
	}
}

void IRContext::dump() const
{
	for (auto block : _blocks) {
		printf("block %d:\n", block.second->id());
		for (auto insn : block.second->instructions()) {
			printf("  ");
			insn->dump();
			printf("\n");
		}
	}
}

IRBlock::~IRBlock()
{
	for (auto insn : _instructions) {
		delete insn;
	}
}

IRInstruction::~IRInstruction()
{
	for (auto oper : _all_operands) {
		delete oper;
	}
}

void IRInstruction::dump() const
{
	printf("%s ", mnemonic());

	bool first = true;
	for (auto oper : _all_operands) {
		if (first) {
			first = false;
		} else {
			printf(", ");
		}

		oper->dump();
	}
}

void IROperand::dump() const
{
	printf("?");
}

void IRRegisterOperand::dump() const
{
	printf("i%d %%r%d", _rg.width(), _rg.id());
}

void IRConstantOperand::dump() const
{
	printf("i%d $0x%x", _width, _value);
}

void IRFunctionOperand::dump() const
{
	printf("*%p", _fnp);
}

void IRBlockOperand::dump() const
{
	printf("b%d", _block.id());
}