#include <profile/image.h>
#include <profile/region.h>

using namespace captive::arch::profile;

Image::Image()
{

}

Image::~Image()
{

}

Block& Image::get_block(gpa_t gva)
{
	return get_region(gva).get_block(gva);
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
