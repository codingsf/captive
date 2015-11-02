#include <captive.h>
#include <platform/platform.h>
#include <hypervisor/config.h>
#include <hypervisor/guest.h>

USE_CONTEXT(Hypervisor)
DECLARE_CHILD_CONTEXT(Guest, Hypervisor);

#define MAX_PHYS_MEM_SIZE		0x100000000
#define MIN_PHYS_MEM_SIZE		0

using namespace captive::hypervisor;

Guest::Guest(Hypervisor& owner, engine::Engine& engine, jit::JIT& jit, platform::Platform& pfm) : _owner(owner), _engine(engine), _jit(jit), _pfm(pfm)
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
