#include <hypervisor/hypervisor.h>
#include <captive.h>

DECLARE_CONTEXT(Hypervisor);

using namespace captive::hypervisor;

Hypervisor::Hypervisor()
{

}

Hypervisor::~Hypervisor()
{

}

bool Hypervisor::init()
{
	return true;
}
