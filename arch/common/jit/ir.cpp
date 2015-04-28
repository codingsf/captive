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
