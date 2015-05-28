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

	for (auto reg : _registers) {
		delete reg.second;
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
	printf("digraph b {\n");
	for (auto block : _blocks) {
		char label[256] = {0};

		sprintf(label, "<f0>");

		for (auto in : block.second->live_ins()) {
			sprintf(label, "%s r%d", label, in->id());
		}

		sprintf(label, "%s|<f1>b%04d|<f2>", label, block.second->id());

		for (auto out : block.second->live_outs()) {
			sprintf(label, "%s r%d", label, out->id());
		}

		printf("  b%04d [shape=Mrecord, label=\"%s\"]\n", block.second->id(), label);

		for (auto succ : block.second->successors()) {
			printf("  b%04d -> b%04d\n", block.second->id(), succ->id());
		}
	}
	printf("}\n");

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
	//delete this;
}

void IRBlock::compute_initial_liveness()
{
	_live_in.clear();
	_live_out.clear();

	for (auto insn : _instructions) {
		insn->_live_in.clear();
		insn->_live_out.clear();
	}

	// Iterate over the list of instructions, backwards.
	for (auto II = _instructions.rbegin(), IE = _instructions.rend(); II != IE; ++II) {
		IRInstruction *insn = *II;

		// Iterate over vregs used by this instruction, and mark as a LIVE IN
		for (auto use : insn->_uses) {
			insn->_live_in.insert(use);
			_live_in.insert(use);
		}

		// Iterate over vregs defined by this instruction, and remove them as a LIVE IN
		for (auto def : insn->_defs) {
			_live_in.erase(def);
		}

		// Add each LIVE IN as a LIVE IN for this instruction
		for (auto live : _live_in) {
			insn->_live_in.insert(live);
		}

		if (insn->_next) {
			for (auto in : insn->_next->_live_in) {
				insn->_live_out.insert(in);
			}
		} else {
			for (auto out : _live_out) {
				insn->_live_out.insert(out);
			}
		}
	}

	_live_in = _instructions.front()->live_ins();
}

void IRBlock::finalise_liveness(std::set<IRBlock *>& work_list)
{
	work_list.erase(this);

	for (auto pred : _pred) {
		std::set<IRRegister *> current_outs;

		for (auto in : _live_in) {
			pred->_live_out.insert(in);
			current_outs.insert(in);
		}

		for (auto II = pred->_instructions.rbegin(), IE = pred->_instructions.rend(); II != IE; ++II) {
			IRInstruction *insn = *II;

			for (auto out : current_outs) {
				insn->_live_out.insert(out);

				// If the instruction defines (and does not read) the vreg, then stop propagation
				if (insn->_defs.count(out) != 0 && insn->_uses.count(out) == 0) {
					current_outs.erase(out);
				}
			}

			for (auto out : current_outs) {
				insn->_live_in.insert(out);
			}
		}

		if (current_outs.size() != 0) {
			for (auto out : current_outs) {
				pred->_live_in.insert(out);
			}

			work_list.insert(pred);
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
	//delete this;
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

	if (_alloc_class == Register) {
		printf("-R(%d)", _alloc_data);
	} else if (_alloc_class == Stack) {
		printf("-S(%d)", _alloc_data);
	}
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
	printf("pc #%08x", _val);
}