#include <hypervisor/soft/soft.h>

using namespace captive::hypervisor;
using namespace captive::hypervisor::soft;

Soft::Soft()
{

}

Soft::~Soft()
{

}

Guest *Soft::create_guest()
{
	return new SoftGuest(*this);
}

SoftGuest::SoftGuest(Hypervisor& owner, GuestConfiguration& config) : Guest(owner, config)
{
	
}

SoftGuest::~SoftGuest()
{
	
}

bool SoftGuest::start()
{
	return false;
}
