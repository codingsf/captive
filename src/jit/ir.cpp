#include <jit/ir.h>

#include <sstream>

using namespace captive::jit;
using namespace captive::jit::ir;

IRContext::IRContext()
{

}

IRContext *IRContext::build_context(const RawBytecodeDescriptor* bcd)
{
	IRContext *ctx = new IRContext();

	for (uint32_t idx = 0; idx < bcd->bytecode_count; idx++) {
		const RawBytecode *bc = &bcd->bc[idx];

		IRBlock& block = ctx->get_or_create_block(bc->block_id);
		IRInstruction *insn = IRInstruction::create_from_bytecode(*ctx, block, bc);

		block.append_instruction(insn);
	}

	return ctx;
}

std::string IRContext::to_string() const
{
	std::stringstream str;

	for (auto block : blocks) {
		str << block.second->to_string() << std::endl;
	}

	return str.str();
}

std::string IRBlock::to_string() const
{
	std::stringstream str;

	str << "BLOCK " << _name << " {";

	bool first = true;
	for (auto succ : succs) {
		if (first) {
			first = false;
		} else {
			str << ", ";
		}

		str << succ->_name;
	}

	str << "}\n";

	for (auto insn : instructions) {
		str << "  " << insn->to_string() << std::endl;
	}

	return str.str();
}

std::string IRInstruction::to_string() const
{
	std::stringstream str;

	str << mnemonic() << " ";

	bool first = true;
	for (auto oper : _all) {
		if (first) {
			first = false;
		} else {
			str << ", ";
		}

		str << oper->to_string();
	}

	return str.str();
}

std::string operands::BlockOperand::to_string() const
{
	return "b" + _block.name();
}

std::string operands::RegisterOperand::to_string() const
{
	return "r" + _rg.name();
}

IRBlock& IRContext::get_or_create_block(uint32_t id)
{
	IRBlock *block = blocks[id];

	if (block == NULL) {
		block = new IRBlock(*this, std::to_string(id));
		blocks[id] = block;
	}

	return *block;
}

ir::IRRegister& IRContext::get_or_create_vreg(uint32_t id, uint8_t size)
{
	IRRegister *reg = registers[id];

	if (reg == NULL) {
		reg = new IRRegister(*this, std::to_string(id), size);
		registers[id] = reg;
	} else {
		assert(reg->size() == size);
	}

	return *reg;
}

IRInstruction* IRInstruction::create_from_bytecode(IRContext& ctx, IRBlock& owner, const RawBytecode* bc)
{
	IRInstruction *insn = NULL;

	switch (bc->insn.type) {
	case RawInstruction::MOV:
		insn = new instructions::Move(
			owner,
			*IROperand::create_from_bytecode(ctx, &bc->insn.operands[0]),
			*IROperand::create_from_bytecode(ctx, &bc->insn.operands[1]));
		break;

	case RawInstruction::CALL:
		insn = new instructions::Call(owner, *IROperand::create_from_bytecode(ctx, &bc->insn.operands[0]));
		break;

	case RawInstruction::JMP:
		insn = new instructions::Jump(owner, *IROperand::create_from_bytecode(ctx, &bc->insn.operands[0]));
		break;

	case RawInstruction::RET:
		insn = new instructions::Return(owner);
		break;

	case RawInstruction::READ_REG:
		insn = new instructions::RegisterLoad(
			owner,
			*IROperand::create_from_bytecode(ctx, &bc->insn.operands[0]),
			*IROperand::create_from_bytecode(ctx, &bc->insn.operands[1]));
		break;

	case RawInstruction::WRITE_REG:
		insn = new instructions::RegisterStore(
			owner,
			*IROperand::create_from_bytecode(ctx, &bc->insn.operands[0]),
			*IROperand::create_from_bytecode(ctx, &bc->insn.operands[1]));
		break;

	default:
		insn = new instructions::Invalid(owner);
		break;
	}

	return insn;
}

IROperand* IROperand::create_from_bytecode(IRContext& ctx, const RawOperand* op)
{
	IROperand *oper = NULL;

	switch (op->type) {
	case RawOperand::VREG:
		oper = new operands::RegisterOperand(ctx.get_or_create_vreg(op->val, op->size));
		break;

	case RawOperand::CONSTANT:
		oper = new operands::ImmediateOperand(op->val, op->size);
		break;

	case RawOperand::BLOCK:
		oper = new operands::BlockOperand(ctx.get_or_create_block(op->val));
		break;

	case RawOperand::FUNC:
		oper = new operands::FunctionOperand((void *)op->val);
		break;

	default:
		oper = new operands::InvalidOperand();
		break;
	}

	return oper;
}

void TerminatorIRInstruction::add_destination(IRBlock& block)
{
	owner().add_successor(block);
	block.add_predecessor(owner());
}
