#include <hypervisor/soft/soft.h>

using namespace captive::engine;
using namespace captive::hypervisor;
using namespace captive::hypervisor::soft;

Soft::Soft()
{

}

Soft::~Soft()
{

}

Guest *Soft::create_guest(const GuestConfiguration& config)
{
	return new SoftGuest(*this, config);
}

SoftGuest::SoftGuest(Hypervisor& owner, const GuestConfiguration& config) : Guest(owner, config)
{
	
}

SoftGuest::~SoftGuest()
{
	
}

bool SoftGuest::start(Engine& engine)
{
	return false;
}
