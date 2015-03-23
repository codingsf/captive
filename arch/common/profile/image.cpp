#include <profile/image.h>
#include <profile/region.h>

using namespace captive::arch::profile;

Image::Image()
{

}

Image::~Image()
{

}

Block& Image::get_block(gpa_t gpa)
{
	return get_region(gpa).get_block(gpa);
}

Region& Image::get_region(gpa_t gpa)
{
	// Mask the incoming VA into a region base address.
	gpa_t region_base_address = gpa & ~0xfffULL;

	// Lookup the region from the image, allocating a new one if necessary.
	region_map_t::iterator f = regions.find(region_base_address);

	if (f == regions.end()) {
		Region *rgn = new Region(*this, region_base_address);
		regions[region_base_address] = rgn;
		return *rgn;
	} else {
		return *(f->second);
	}
}

void Image::invalidate()
{
	printf("profile: invalidating regions\n");
	regions.clear();
}

void Image::invalidate(gpa_t gpa)
{
	printf("profile: invalidating region %x\n", gpa);
	get_region(gpa).invalidate();
}
