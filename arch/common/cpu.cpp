#include <cpu.h>
#include <barrier.h>
#include <mmu.h>
#include <decode.h>
#include <interp.h>
#include <disasm.h>
#include <string.h>
#include <safepoint.h>
#include <priv.h>
#include <jit.h>
#include <jit/translation-context.h>
#include <shared-memory.h>
#include <profile/image.h>
#include <profile/region.h>
#include <profile/block.h>

using namespace captive::arch;
using namespace captive::arch::jit;
using namespace captive::arch::profile;

safepoint_t cpu_safepoint;

CPU *CPU::current_cpu;

CPU::CPU(Environment& env, PerCPUData *per_cpu_data)
	: _pc_reg_ptr(NULL),
	_env(env),
	_per_cpu_data(per_cpu_data),
	_exec_txl(false),
	region_txln_cache(new region_txln_cache_t()),
	block_txln_cache(new block_txln_cache_t())
{
	// Zero out the local state.
	bzero(&local_state, sizeof(local_state));

	// Initialise the decode cache
	memset(decode_cache, 0xff, sizeof(decode_cache));

	// Initialise the profiling image
	image = new profile::Image();

	jit_state.cpu = this;
	
	jit_state.block_txln_cache = block_txln_cache->ptr();
	
	if (_per_cpu_data->execution_mode == 2) { // If we're a region jit
		jit_state.region_txln_cache = region_txln_cache->ptr();
	} else {
		jit_state.region_txln_cache = NULL;
	}
	
	jit_state.insn_counter = &(per_cpu_data->insns_executed);
	jit_state.exit_chain = 0;
	
	// Populate the FS register with the address of the JIT state structure.
	__wrmsr(0xc0000100, (uint64_t)&jit_state);

	invalidate_virtual_mappings();
}

CPU::~CPU()
{
	if (region_txln_cache)
		delete region_txln_cache;

	if (block_txln_cache)
		delete block_txln_cache;
}

bool CPU::handle_pending_action(uint32_t action)
{
	switch (action) {
	case 2:
	{
		struct mallinfo mi = dlmallinfo();
		printf("*** malloc info ***\n");
		printf("used: %d\nfree: %d\n", mi.uordblks, mi.fordblks);

		dump_state();
		return true;
	}

	case 3:
		trace().enable();
		return true;

	case 4:
		asm volatile("out %0, $0xff\n" :: "a"(4));
		return true;
	}

	return false;
}


bool CPU::run()
{
	switch (_per_cpu_data->execution_mode) {
	case 0:
		return run_interp();
	case 1:
		return run_block_jit();
	case 2:
		return run_region_jit();
	case 3:
		return run_test();
	default:
		return false;
	}
}

bool CPU::run_test()
{
	volatile uint32_t *v1 = (volatile uint32_t *)0x1000;
	volatile uint32_t *v2 = (volatile uint32_t *)0x100001000;

	*v1; *v2;

	printf("dirty: map0=%d map1=%d val0=%d val1=%d\n", mmu().is_page_dirty((va_t)v1), mmu().is_page_dirty((va_t)v2), *v1, *v2);
	*v1 = 1;
	printf("dirty: map0=%d map1=%d val0=%d val1=%d\n", mmu().is_page_dirty((va_t)v1), mmu().is_page_dirty((va_t)v2), *v1, *v2);


	return true;
}

bool CPU::run_interp()
{
	printf("cpu: starting interpretive cpu execution\n");

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

	// Make sure we're operating in the correct mode
	ensure_privilege_mode();

	return run_interp_safepoint();
}

bool CPU::run_interp_safepoint()
{
	bool step_ok = true;

	do {
		/*if (cpu_data().insns_executed > 392000000)
			trace().enable();*/

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

		step_ok = interpret_block();
	} while(step_ok);

	return true;
}

bool CPU::interpret_block()
{
	bool step_ok = false;

	// Make sure we're in the correct privilege mode to execute this block
	// of code.
	ensure_privilege_mode();

	uint32_t page = read_pc() & ~0xfffULL;

	// Now, execute one basic-block of instructions.
	Decode *insn;
	do {
		// Get the address of the next instruction to execute
		uint32_t pc = read_pc();
		if ((pc & ~0xfff) != page) return true;

		// Obtain a decode object for this PC, and perform the decode.
		insn = get_decode(pc);
		if (1) { //insn->pc != pc) {
			if (unlikely(!decode_instruction_virt(pc, insn))) {
				printf("cpu: unhandled decode fault @ %08x\n", pc);
				return false;
			}
		}

		// Perhaps trace this instruction
		if (unlikely(trace().enabled())) {
			trace().start_record(get_insns_executed(), pc, insn);
		}

		// Perhaps verify this instruction
		if (unlikely(cpu_data().verify_enabled) && !verify_check()) {
			return false;
		}

		// Execute the instruction, with interrupts disabled.
		//__local_irq_disable();
		step_ok = interpreter().step_single(insn);
		//__local_irq_enable();

		// Increment the instruction counter.
		if (unlikely(cpu_data().verbose_enabled)) {
			inc_insns_executed();
		}

		// Perhaps finish tracing this instruction.
		if (unlikely(trace().enabled())) {
			trace().end_record();
		}
	} while(!insn->end_of_block && step_ok);
}

void CPU::invalidate_translations()
{
	image->invalidate();
	invalidate_virtual_mappings();
}

void CPU::invalidate_translation(pa_t phys_addr, va_t virt_addr)
{
	if (virt_addr >= (va_t)0x100000000) return;

	Region *rgn = image->get_region((uint32_t)(uint64_t)phys_addr);

	if (rgn) {
		rgn->invalidate();
	}
	
	invalidate_virtual_mappings();
}

void CPU::invalidate_virtual_mappings()
{
	if (block_txln_cache) {
		block_txln_cache->invalidate_dirty();
	}

	if (region_txln_cache) {
		region_txln_cache->invalidate_dirty();
	}
}

void CPU::invalidate_virtual_mapping(gva_t va)
{
	if (block_txln_cache) {
		block_txln_cache->invalidate_entry(va);
	}

	if(region_txln_cache) {
		region_txln_cache->invalidate_entry(va >> 12);
	}
}

void CPU::handle_irq_raised(uint8_t irq_line)
{
	//printf("*** raised %d\n", irq_line);
	jit_state.exit_chain = 1; //cpu_data().isr;
}

void CPU::handle_irq_rescinded(uint8_t irq_line)
{
	//printf("*** rescinded %d\n", irq_line);
	//jit_state.exit_chain = cpu_data().isr;
}

static uint32_t pc_ring_buffer[256];
static uint32_t pc_ring_buffer_ptr;

bool CPU::verify_check()
{
	Barrier *enter = (Barrier *)cpu_data().verify_data->barrier_enter;
	Barrier *exit = (Barrier *)cpu_data().verify_data->barrier_exit;

	// If we're not the primary CPU, copy our state into the shared memory region.
	if (cpu_data().verify_tid > 0) {
		memcpy((void *)cpu_data().verify_data->cpu_data, (const void *)reg_state(), reg_state_size());
	}

	// Wait on the entry barrier
	enter->wait(cpu_data().verify_tid);

	// Tick!
	asm volatile("out %0, $0xff" :: "a"(9));

	pc_ring_buffer[pc_ring_buffer_ptr] = read_pc();
	pc_ring_buffer_ptr = (pc_ring_buffer_ptr + 1) % 256;

	// If we're the primary CPU, perform verification.
	if (cpu_data().verify_tid == 0) {
		// Compare OUR register state to the other CPU's.
		if (memcmp((const void *)reg_state(), (const void *)cpu_data().verify_data->cpu_data, reg_state_size())) {
			cpu_data().verify_data->verify_failed = true;
		} else {
			cpu_data().verify_data->verify_failed = false;
		}

	}

	// Wait on the exit barrier
	exit->wait(cpu_data().verify_tid);

	// Check to see if we failed, and if so, dump our state and return a failure code.
	if (cpu_data().verify_data->verify_failed) {
		gva_t this_pc = read_pc();
		printf("*** DIVERGENCE DETECTED @ %08x ***\n", this_pc);
		dump_state();

		for (int i = pc_ring_buffer_ptr; i < pc_ring_buffer_ptr + 256; i++) {
			gva_t virt_pc = pc_ring_buffer[i % 256];
			gpa_t phys_pc = 0;

			MMU::access_info info;
			info.mode = MMU::ACCESS_KERNEL;
			info.type = MMU::ACCESS_READ;

			MMU::resolution_fault fault;
			bool ok = mmu().resolve_gpa(virt_pc, phys_pc, info, fault, false);

			if (virt_pc == this_pc)
				printf("=>");
			else
				printf("  ");

			uint32_t pc_data = 0;
			if (ok && fault == 0) {
				pc_data = *(uint32_t *)(0x100000000ULL | phys_pc);
			}

			printf(" VIRT=%08x, PHYS=%08x, DATA=%08x\n", virt_pc, phys_pc, pc_data);
		}

		return false;
	} else {
		return true;
	}
}
