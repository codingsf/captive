#include <jit/ir.h>
#include <shared-jit.h>

#include <sstream>

using namespace captive::jit;
using namespace captive::jit::ir;
//using namespace captive::shared;

IRContext::IRContext()
{

}

IRContext *IRContext::build_context(const shared::TranslationBlock* tb)
{
	IRContext *ctx = new IRContext();

	for (uint32_t idx = 0; idx < 0; idx++) {
		const shared::IRInstruction *rinsn = NULL;

		IRBlock& block = ctx->get_or_create_block(rinsn->ir_block);
		IRInstruction *insn = IRInstruction::create_from_instruction(*ctx, block, rinsn);

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

IRInstruction* IRInstruction::create_from_instruction(IRContext& ctx, IRBlock& owner, const shared::IRInstruction *bc)
{
	IRInstruction *insn = NULL;

	switch (bc->type) {
	case shared::IRInstruction::MOV:
		insn = new instructions::Move(
			owner,
			*IROperand::create_from_bytecode(ctx, &bc->operands[0]),
			*IROperand::create_from_bytecode(ctx, &bc->operands[1]));
		break;

	case shared::IRInstruction::CALL:
		insn = new instructions::Call(owner, *IROperand::create_from_bytecode(ctx, &bc->operands[0]));
		break;

	case shared::IRInstruction::JMP:
		insn = new instructions::Jump(owner, *IROperand::create_from_bytecode(ctx, &bc->operands[0]));
		break;

	case shared::IRInstruction::RET:
		insn = new instructions::Return(owner);
		break;

	case shared::IRInstruction::READ_REG:
		insn = new instructions::RegisterLoad(
			owner,
			*IROperand::create_from_bytecode(ctx, &bc->operands[0]),
			*IROperand::create_from_bytecode(ctx, &bc->operands[1]));
		break;

	case shared::IRInstruction::WRITE_REG:
		insn = new instructions::RegisterStore(
			owner,
			*IROperand::create_from_bytecode(ctx, &bc->operands[0]),
			*IROperand::create_from_bytecode(ctx, &bc->operands[1]));
		break;

	default:
		insn = new instructions::Invalid(owner);
		break;
	}

	return insn;
}

IROperand* IROperand::create_from_bytecode(IRContext& ctx, const shared::IROperand* op)
{
	IROperand *oper = NULL;

	switch (op->type) {
	case shared::IROperand::VREG:
		oper = new operands::RegisterOperand(ctx.get_or_create_vreg(op->value, op->size));
		break;

	case shared::IROperand::CONSTANT:
		oper = new operands::ImmediateOperand(op->value, op->size);
		break;

	case shared::IROperand::BLOCK:
		oper = new operands::BlockOperand(ctx.get_or_create_block(op->value));
		break;

	case shared::IROperand::FUNC:
		oper = new operands::FunctionOperand((void *)op->value);
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
