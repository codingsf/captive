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

void IRContext::remove_block(IRBlock& block)
{
	_blocks.erase(block.id());
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
		printf("block %d: ", block.second->id());

		bool first = true;

		printf("PRED={ ");
		for (auto pred : block.second->predecessors()) {
			printf("%d ", pred->id());
		}
		printf("} ");

		printf("SUCC={ ");
		for (auto succ : block.second->successors()) {
			printf("%d ", succ->id());
		}
		printf("} ");

		printf("IN={ ");
		for (auto in : block.second->live_ins()) {
			if (first) {
				first = false;
			} else {
				first = true;
				printf(", ");
			}

			printf("r%d ", in->id());
		}
		printf("} ");

		first = true;

		printf("OUT={ ");
		for (auto out : block.second->live_outs()) {
			if (first) {
				first = false;
			} else {
				first = true;
				printf(", ");
			}

			printf("r%d ", out->id());
		}
		printf("}\n");

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

void IRBlock::remove_from_parent()
{
	 _owner.remove_block(*this);
}

void IRBlock::calculate_liveness()
{
	std::set<IRRegister *> current_live;

	for (auto II = _instructions.rbegin(), IE = _instructions.rend(); II != IE; ++II) {
		IRInstruction *insn = *II;

		insn->_live_in.clear();
		insn->_live_out.clear();

		for (auto use : insn->_uses) {
			insn->_live_in.insert(&use->reg());
			current_live.insert(&use->reg());
		}

		for (auto def : insn->_defs) {
			current_live.erase(&def->reg());
		}

		for (auto live : current_live) {
			insn->_live_in.insert(live);
		}

		if (insn->_next) {
			for (auto in : insn->_next->_live_in) {
				insn->_live_out.insert(in);
			}
		}
	}
}

IRInstruction::~IRInstruction()
{
	for (auto oper : _all_operands) {
		delete oper;
	}
}

void IRInstruction::remove_from_parent()
{
	_owner->remove_instruction(*this);
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

	if (_live_in.size() > 0) {
		printf(" IN={ ");
		for (auto in : _live_in) {
			printf("r%d ", in->id());
		}
		printf("}");
	}

	if (_live_out.size() > 0) {
		printf(" OUT={ ");
		for (auto out : _live_out) {
			printf("r%d ", out->id());
		}
		printf("}");
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

void IRPCOperand::dump() const
{
	printf("pc");
}