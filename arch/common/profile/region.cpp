#include <profile/region.h>
#include <profile/block.h>
#include <profile/translation.h>
#include <shared-jit.h>

using namespace captive::arch::profile;

Region::Region(Image& owner, gpa_t address) : _owner(owner), _address(address), _status(NOT_IN_TRANSLATION), _generation(0), _txln(NULL)
{

}

Block& Region::get_block(gpa_t gpa)
{
	// Mask off the incoming GVA into a block offset;
	gpa_t block_offset = gpa & 0xfffULL;

	// Try to lookup the block in the region, allocating a new one if it
	// doesn't exist.

	block_map_t::iterator f = blocks.find(block_offset);
	if (f == blocks.end()) {
		Block *block = new Block(*this, gpa);
		blocks[block_offset] = block;
		return *block;
	} else {
		return *(f->second);
	}
}

uint32_t Region::hot_block_count()
{
	uint32_t r = 0;
	for (auto block : blocks) {
		if (block.second->interp_count() > 100) {
			r++;
		}
	}

	return r;
}

void Region::reset_heat()
{
	for (auto block : blocks) {
		block.second->reset_interp_count();
	}
}

void Region::invalidate()
{
	_status = INVALID;

	if (_txln) {
		printf("region: deleting translation\n");
		// FIXME: TODO
		//delete _txln;
		_txln = NULL;
	}
}

void Region::invalidate_vaddr()
{
	vaddrs.clear();
}
