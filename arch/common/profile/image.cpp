#include <profile/image.h>
#include <profile/region.h>

using namespace captive::arch::profile;

Image::Image()
{

}

Image::~Image()
{

}

Block& Image::get_block(gva_t gva)
{
	return get_region(gva).get_block(gva);
}

Region& Image::get_region(gva_t gva)
{

}
