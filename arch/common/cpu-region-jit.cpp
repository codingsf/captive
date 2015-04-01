#include <cpu.h>
#include <interp.h>
#include <decode.h>
#include <disasm.h>
#include <jit.h>
#include <jit/translation-context.h>
#include <shared-memory.h>
#include <shared-jit.h>
#include <profile/image.h>
#include <profile/region.h>
#include <profile/block.h>
#include <profile/translation.h>

using namespace captive::arch;
using namespace captive::arch::jit;
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
		mmu().set_page_executed(virt_pc);

		if (block.has_translation()) {
			_exec_txl = true;
			step_ok = (bool)block.translation()->execute(*this);
			_exec_txl = false;
			continue;
		} else {
			block.owner().add_virtual_base(virt_pc);
		}

		if (trace_interval > 100000) {
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
	for (auto region : profile_image()) {
		if (region.second->hot_block_count() > 10 && region.second->status() != Region::IN_TRANSLATION) {
			compile_region(*region.second);
		}
	}
}

void CPU::compile_region(Region& rgn)
{
	using namespace captive::shared;

	rgn.status(Region::IN_TRANSLATION);

	RegionWorkUnit *rwu = (RegionWorkUnit *)Memory::shared_memory().allocate(sizeof(RegionWorkUnit), SharedMemory::ZERO);
	assert(rwu);

	rgn.set_work_unit(rwu);

	rwu->blocks = (TranslationBlocks *)Memory::shared_memory().allocate(sizeof(*rwu->blocks));
	assert(rwu->blocks);

	rwu->blocks->block_count = 0;

	// TODO: Don't hard-code this
	rwu->blocks->descriptors = (TranslationBlockDescriptor *)Memory::shared_memory().allocate(sizeof(TranslationBlockDescriptor) * 1024);
	assert(rwu->blocks->descriptors);

	TranslationContext ctx(Memory::shared_memory());
	uint8_t decode_data[128];
	Decode *insn = (Decode *)&decode_data[0];

	rwu->region_base_address = (uint32_t)rgn.address();
	rwu->work_unit_id = rgn.generation();

	//printf("compiling region %x\n", rgn.address());
	for (auto block : rgn) {
		// Make sure we start this GBB in a new IRBB
		ctx.current_block(ctx.alloc_block());

		//printf("  generating block %x id=%d heat=%d\n", block.second->address(), ctx.current_block(), block.second->interp_count());

		//rwu->blocks->descriptors = (TranslationBlockDescriptor *)Memory::shared_memory().reallocate(rwu->blocks->descriptors, sizeof(*rwu->blocks->descriptors) * (rwu->blocks->block_count + 1));
		rwu->blocks->descriptors[rwu->blocks->block_count].block_id = ctx.current_block();
		rwu->blocks->descriptors[rwu->blocks->block_count].block_addr = block.second->address() & 0xfff;
		rwu->blocks->descriptors[rwu->blocks->block_count].heat = block.second->interp_count();
		rwu->blocks->block_count++;

		uint32_t pc = block.second->address();
		do {
			// Attempt to decode the current instruction.
			if (!decode_instruction_phys(pc, insn)) {
				printf("cpu: unhandled decode fault @ %08x\n", pc);
				assert(false);
			}

			//printf("jit: translating insn @ [%08x] %s\n", insn->pc, trace().disasm().disassemble(insn->pc, decode_data));

			if (unlikely(cpu_data().verify_enabled)) {
				ctx.add_instruction(jit::IRInstructionBuilder::create_verify());
			}

			// Translate this instruction into the context.
			if (!jit().translate(insn, ctx)) {
				rgn.invalidate();

				Memory::shared_memory().free(rwu->blocks->descriptors);
				Memory::shared_memory().free(rwu->blocks);
				Memory::shared_memory().free(rwu->ir);
				Memory::shared_memory().free(rwu);

				return;
			}

			pc += insn->length;
		} while (!insn->end_of_block);

		// Finish off with a RET.
		ctx.add_instruction(jit::IRInstructionBuilder::create_ret());
	}

	// Fill in the RWU with the IR buffer
	rwu->ir = ctx.buffer();

	// Validate the RWU
	rwu->valid = true;

	// Make region translation hypercall
	//printf("jit: dispatching region %08x rwu=%p\n", rwu->region_base_address, rwu);
	asm volatile("out %0, $0xff" :: "a"(8), "D"((uint64_t)rwu));
}

void CPU::register_region(captive::shared::RegionWorkUnit* rwu)
{
	//printf("jit: register region %08x rwu=%p fn=%lx gen=%d\n", rwu->region_base_address, rwu, rwu->function_addr, rwu->work_unit_id);

	Region& rgn = profile_image().get_region(rwu->region_base_address);

	if (rwu->function_addr) {
		if (rwu->work_unit_id < rgn.generation()) {
			printf("jit: discarding stale translation, rwu %08x gen=%d cur gen=%d\n", rwu->region_base_address, rwu->work_unit_id, rgn.generation());
		} else {
			Translation *txln = new Translation((Translation::translation_fn_t)rwu->function_addr);

			rgn.translation(txln);
			for (int i = 0; i < rwu->blocks->block_count; i++) {
				rgn.get_block(rwu->blocks->descriptors[i].block_addr).translation(txln);
			}
		}
	}

	rgn.set_work_unit(NULL);

	Memory::shared_memory().free(rwu->blocks->descriptors);
	Memory::shared_memory().free(rwu->blocks);
	Memory::shared_memory().free(rwu->ir);
	Memory::shared_memory().free(rwu);

	rgn.status(Region::NOT_IN_TRANSLATION);
}
