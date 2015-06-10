#include <profile/image.h>
#include <profile/region.h>
#include <profile/page.h>

using namespace captive::arch::profile;

Image::Image()
{
	pages = new Page[1048576];
}

Image::~Image()
{

}

Page& Image::get_page(gpa_t gpa)
{
	gpa_t base_address = gpa & ~0xfff;
	Page *page = &pages[base_address];
	page->base_address = base_address;

	return *page;
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
	//printf("profile: invalidating all regions\n");

	for (auto rgn : regions) {
		rgn.second->invalidate();
	}

	regions.clear();
}

void Image::invalidate_vaddr()
{
	for (auto rgn : regions) {
		rgn.second->invalidate_vaddr();
	}
}

void Image::invalidate(gpa_t gpa)
{
	gpa_t region_base_address = gpa & ~0xfffULL;

	//printf("profile: invalidating region %08x\n", region_base_address);

	region_map_t::iterator f = regions.find(region_base_address);
	if (f != regions.end()) {
		Region& rgn = *(f->second);
		rgn.invalidate();

		regions.erase(f);
	}
}
