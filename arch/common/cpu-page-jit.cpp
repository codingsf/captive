#include <cpu.h>
#include <interp.h>
#include <decode.h>
#include <disasm.h>
#include <jit.h>
#include <safepoint.h>
#include <shared-jit.h>
#include <shared-memory.h>

#include <profile/image.h>
#include <profile/page.h>

extern safepoint_t cpu_safepoint;

using namespace captive::arch;
using namespace captive::arch::jit;
using namespace captive::arch::profile;
using namespace captive::shared;

bool CPU::run_page_jit()
{
	printf("cpu: starting page-jit cpu execution\n");

	// Create a safepoint for returning from a memory access fault
	int rc = record_safepoint(&cpu_safepoint);
	if (rc > 0) {
		// Reset the executing translation flag.
		_exec_txl = false;

		// If the return code is greater than zero, then we returned
		// from a fault.

		// If we're tracing, add a descriptive message, and close the
		// trace packet.
		if (unlikely(trace().enabled())) {
			trace().add_str("memory exception taken");
			trace().end_record();
		}

		// Instruct the interpreter to handle the memory fault, passing
		// in the the type of fault.
		interpreter().handle_memory_fault((MMU::resolution_fault)rc);

		// Make sure interrupts are enabled.
		__local_irq_enable();
	}

	return run_page_jit_safepoint();
}

bool CPU::run_page_jit_safepoint()
{
	bool step_ok = true;

	do {
		// We need to be in kernel mode to do some accounting.
		switch_to_kernel_mode();

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

		if (!mmu().virt_to_phys(virt_pc, phys_pc)) abort();

		mmu().set_page_executed(virt_pc);

		Page& page = profile_image().get_page(phys_pc);
		page.add_entry(phys_pc);

		if (page.size() > 10) {
			if (!translate_page(page)) abort();
		}

		interpret_block();
	} while(step_ok);

	return true;
}

bool CPU::translate_page(Page& page)
{
	struct PageWorkUnit *pwu = (struct PageWorkUnit *)Memory::shared_memory().allocate(sizeof(struct PageWorkUnit));
	memcpy(pwu->page, page.data(), 4096);

	printf("jit: translating page: %08x, heat=%d\n", page.base_address, page.size());

	for (auto entry : page.entries()) {
		pwu->entries[entry / 8] |= 1 << (entry % 8);
	}

	asm volatile("out %0, $0xff" :: "a"(20), "D"((uint64_t)pwu));

	Memory::shared_memory().free(pwu);
	return true;
}
