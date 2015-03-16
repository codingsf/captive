#include <profile/region.h>

using namespace captive::arch::profile;

Region::Region(Image& owner) : _owner(owner)
{

}

Block& Region::get_block(gva_t gva)
{
}
