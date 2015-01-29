#include <captive.h>
#include <hypervisor/config.h>
#include <hypervisor/guest.h>

#define MAX_PHYS_MEM_SIZE		0x100000000
#define MIN_PHYS_MEM_SIZE		0

using namespace captive::hypervisor;

Guest::Guest(Hypervisor& owner, const GuestConfiguration& config) : _owner(owner), _config(config)
{

}

Guest::~Guest()
{
	
}

bool Guest::init()
{
	return config().validate();
}
