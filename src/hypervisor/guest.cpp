#include <hypervisor/guest.h>

using namespace captive::hypervisor;

Guest::Guest(Hypervisor& owner) : _owner(owner)
{

}

Guest::~Guest()
{
	
}
