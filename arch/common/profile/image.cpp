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
	Region *rgn;
	if (!regions.try_get_value(region_base_address, rgn)) {
		rgn = new Region(*this, region_base_address);
		regions.insert(region_base_address, rgn);
	}

	return *rgn;
}
