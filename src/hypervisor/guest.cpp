#include <captive.h>
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

bool GuestConfiguration::validate() const
{
	if (!have_cpus()) {
		ERROR << "Guest configuration doesn't define any CPUs";
		return false;
	}
	
	if (!have_memory_regions()) {
		ERROR << "Guest configuration doesn't define any memory regions";
		return false;
	}
	
	for (auto& rgn : memory_regions) {
		if (rgn.base_address() + rgn.size() > MAX_PHYS_MEM_SIZE) {
			ERROR << "Guest memory region exceeds 32-bit memory space";
			return false;
		}
	}
	
	return true;
}
