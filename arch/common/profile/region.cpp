#include <profile/region.h>
#include <profile/block.h>

using namespace captive::arch::profile;

Region::Region(Image& owner, gpa_t address) : _owner(owner), _address(address), _status(NOT_IN_TRANSLATION)
{

}

Block& Region::get_block(gpa_t gpa)
{
	// Mask off the incoming GVA into a block offset;
	gpa_t block_offset = gpa & 0xfffULL;

	// Try to lookup the block in the region, allocating a new one if it
	// doesn't exist.
	Block *block;
	if (!blocks.try_get_value(block_offset, block)) {
		block = new Block(*this, gpa);
		blocks.insert(block_offset, block);
	}

	// Return the block.
	return *block;
}

uint32_t Region::hot_block_count()
{
	uint32_t r = 0;
	for (auto block : blocks) {
		if (block.t2->interp_count() > 10) {
			r++;
		}
	}

	return r;
}
