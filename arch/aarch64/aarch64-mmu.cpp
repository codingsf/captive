#include <aarch64-mmu.h>
#include <aarch64-cpu.h>
#include <aarch64-env.h>
#include <mm.h>
#include <printf.h>

using namespace captive::arch;
using namespace captive::arch::aarch64;

aarch64_mmu::aarch64_mmu(aarch64_cpu& cpu) : MMU(cpu), _enabled(false)
{

}

aarch64_mmu::~aarch64_mmu()
{

}

MMU *aarch64_mmu::create(aarch64_cpu& cpu)
{
	return new aarch64_mmu(cpu);
}

bool aarch64_mmu::enable()
{
	if (_enabled) return true;

#ifdef DEBUG_MMU
	printf("mmu: enable\n");
#endif
	
	invalidate_virtual_mappings();
	cpu().invalidate_translations();
	
	_enabled = true;
	return true;
}

bool aarch64_mmu::disable()
{
	if (!_enabled) return true;
	
#ifdef DEBUG_MMU
	printf("mmu: disable %x\n", cpu().read_pc());
#endif

	invalidate_virtual_mappings();
	cpu().invalidate_translations();

	_enabled = false;
	return true;
}

bool aarch64_mmu::resolve_gpa(resolution_context& context, bool have_side_effects)
{
	context.allowed_permissions = context.requested_permissions;
	context.fault = MMU::NONE;
	context.pa = context.va;
	
	return true;
}
