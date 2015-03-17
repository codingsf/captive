#include <cpu.h>
#include <interp.h>
#include <profile/image.h>
#include <profile/region.h>
#include <profile/block.h>

using namespace captive::arch;
using namespace captive::arch::profile;

bool CPU::run_region_jit()
{
	bool step_ok = true;
	int trace_interval = 0;

	printf("cpu: starting region-jit cpu execution\n");

	if (!check_safepoint()) {
		return false;
	}

	do {
		// Check the ISR to determine if there is an interrupt pending,
		// and if there is, instruct the interpreter to handle it.
		if (unlikely(cpu_data().isr)) {
			interpreter().handle_irq(cpu_data().isr);
		}

		// Check to see if there are any pending actions coming in from
		// the hypervisor.
		if (unlikely(cpu_data().async_action)) {
			if (handle_pending_action(cpu_data().async_action)) {
				cpu_data().async_action = 0;
			}
		}

		gva_t virt_pc = (gva_t)read_pc();
		gpa_t phys_pc;

		MMU::access_info info;
		info.mode = kernel_mode() ? MMU::ACCESS_KERNEL : MMU::ACCESS_USER;
		info.type = MMU::ACCESS_FETCH;

		MMU::resolution_fault fault;

		if (!mmu().resolve_gpa(virt_pc, phys_pc, info, fault, false)) {
			return false;
		}

		if (fault != MMU::NONE) {
			step_ok = interpret_block();
			continue;
		}

		Block& block = profile_image().get_block(phys_pc);

		if (trace_interval > 10000) {
			trace_interval = 0;
			analyse_regions();
		} else {
			trace_interval++;
		}

		block.inc_interp_count();
		step_ok = interpret_block();

	} while(step_ok);

	return true;
}

void CPU::analyse_regions()
{
	printf("analysing:\n");
	for (auto region : profile_image().regions) {
		printf("region: %08x, hot-blocks=%d\n", region.t2->address(), region.t2->hot_block_count());
	}
}
