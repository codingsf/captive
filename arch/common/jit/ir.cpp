#include <jit/ir.h>

using namespace captive::arch::jit;

IRContext::IRContext()
{

}

IRContext::~IRContext()
{
	for (auto block : _blocks) {
		delete block->second;
	}
}

IRBlock& IRContext::get_block_by_id(shared::IRBlockId id)
{
	block_map_t::iterator iter = _blocks.find(id);
	if (iter == _blocks.end()) {
		IRBlock *new_block = new IRBlock(*this);
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
		IRRegister *new_reg = new IRRegister(*this, width);
		_registers[id] = new_reg;
		return *new_reg;
	} else {
		assert(iter->second->width() == width);
		return *(iter->second);
	}
}
