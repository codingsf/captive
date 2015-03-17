#include <profile/block.h>

using namespace captive::arch::profile;

Block::Block(Region& owner, gpa_t address) : _owner(owner), _address(address)
{

}
