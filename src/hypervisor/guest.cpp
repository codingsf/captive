#include <hypervisor/guest.h>

using namespace captive::hypervisor;

Guest::Guest(Hypervisor& owner, const GuestConfiguration& config) : _owner(owner), _config(config)
{

}

Guest::~Guest()
{
	
}
