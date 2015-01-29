#include <captive.h>
#include <hypervisor/config.h>

using namespace captive::hypervisor;

bool GuestMemoryRegionConfiguration::validate() const
{
	return true;
}

bool GuestCPUConfiguration::validate() const
{
	return true;
}

bool GuestConfiguration::validate() const
{
	if (!have_memory_regions()) {
		ERROR << "Guest configuration doesn't define any memory regions";
		return false;
	}

	for (auto& rgn : memory_regions) {
		if (!rgn.validate())
			return false;
	}

	return true;
}