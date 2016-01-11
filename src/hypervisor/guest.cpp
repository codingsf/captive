#include <captive.h>
#include <hypervisor/config.h>
#include <hypervisor/guest.h>
#include <platform/platform.h>

USE_CONTEXT(Hypervisor)
DECLARE_CHILD_CONTEXT(Guest, Hypervisor);

#define MAX_PHYS_MEM_SIZE		0x100000000
#define MIN_PHYS_MEM_SIZE		0

using namespace captive::hypervisor;

__thread CPU *Guest::current_core;

Guest::Guest(Hypervisor& owner, engine::Engine& engine, platform::Platform& pfm) : _owner(owner), _engine(engine), _pfm(pfm)
{

}

Guest::~Guest()
{

}

bool Guest::init()
{
	if (!platform().config().validate())
		return false;

	return true;
}
