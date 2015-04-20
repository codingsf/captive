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

	gpa_t prev_pc = 0;
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
			prev_pc = phys_pc;
			step_ok = interpret_block();
			continue;
		}

		Block& block = profile_image().get_block(phys_pc);

		if (block.has_translation()) {
			prev_pc = 0;

			jit_state.region_chaining_table[block.owner().address() >> 12] = block.translation()->fn_ptr();

			_exec_txl = true;
			step_ok = (bool)block.translation()->execute(*this);
			_exec_txl = false;
			continue;
		} else {
			block.owner().add_virtual_base(virt_pc);
		}

		mmu().set_page_executed(virt_pc);

		if (trace_interval > 10000) {
			trace_interval = 0;
			analyse_regions();
		} else {
			trace_interval++;
		}

		if (prev_pc == 0 || ((prev_pc & ~0xfffULL) != (phys_pc & ~0xfffULL))) {
			block.entry_block(true);
		}

		prev_pc = phys_pc;

		if (block.owner().status() == Region::NOT_IN_TRANSLATION)
			block.inc_interp_count();

		step_ok = interpret_block();
	} while(step_ok);

	return true;
}

void CPU::analyse_regions()
{
	for (auto region : profile_image()) {
		if (region.second->hot_block_count() > 20 && region.second->status() != Region::IN_TRANSLATION) {
			compile_region(*region.second);
			region.second->reset_heat();
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

	rwu->region_base_address = (uint32_t)rgn.address();
	rwu->work_unit_id = rgn.generation();

	rwu->block_count = 0;
	rwu->blocks = NULL;

	// Loop over each block in the region.
	for (auto block : rgn) {
		rwu->blocks = (TranslationBlock *)Memory::shared_memory().reallocate(rwu->blocks, sizeof(TranslationBlock) * (rwu->block_count + 1));

		if (!compile_block(*block.second, rwu->blocks[rwu->block_count])) {
			goto fail;
		}

		rwu->block_count++;
	}

	// Validate the RWU
	rwu->valid = true;

	// Make region translation hypercall
	printf("jit: dispatching region %08x rwu=%p\n", rwu->region_base_address, rwu);
	asm volatile("out %0, $0xff" :: "a"(8), "D"((uint64_t)rwu));
	return;

fail:
	for (uint32_t i = 0; i < rwu->block_count; i++) {
		Memory::shared_memory().free(rwu->blocks[i].ir_insn);
	}

	Memory::shared_memory().free(rwu->blocks);
	Memory::shared_memory().free(rwu);
}

bool CPU::compile_block(profile::Block& block, captive::shared::TranslationBlock& tb)
{
	using namespace captive::shared;

	TranslationContext ctx(Memory::shared_memory(), tb);
	uint8_t decode_data[128];
	Decode *insn = (Decode *)&decode_data[0];

	// We MUST begin in block zero.
	assert(ctx.current_block() == 0);

	//printf("  generating block %x id=%d heat=%d\n", block.second->address(), ctx.current_block(), block.second->interp_count());

	tb.block_addr = block.address();
	tb.heat = block.interp_count();
	tb.is_entry = block.entry_block() ? 1 : 0;

	uint32_t pc = block.address();
	do {
		// Attempt to decode the current instruction.
		if (!decode_instruction_phys(pc, insn)) {
			printf("cpu: unhandled decode fault @ %08x\n", pc);
			assert(false);
		}

		//printf("jit: translating insn @ [%08x] %s\n", insn->pc, trace().disasm().disassemble(insn->pc, decode_data));

		if (unlikely(cpu_data().verify_enabled)) {
			ctx.add_instruction(IRInstruction::verify());
		}

		// Translate this instruction into the context.
		if (!jit().translate(insn, ctx)) {
			Memory::shared_memory().free(tb.ir_insn);
			return false;
		}

		pc += insn->length;
	} while (!insn->end_of_block);

	assert(insn->end_of_block);
	JumpInfo jump_info = get_instruction_jump_info(insn);

	tb.destination_target = jump_info.target;
	if (insn->is_predicated) {
		if (jump_info.type == JumpInfo::DIRECT) {
			tb.destination_type = TranslationBlock::PREDICATED_DIRECT;
		} else {
			tb.destination_type = TranslationBlock::PREDICATED_INDIRECT;
		}

		tb.fallthrough_target = pc;
	} else {
		if (jump_info.type == JumpInfo::DIRECT) {
			tb.destination_type = TranslationBlock::NON_PREDICATED_DIRECT;
		} else {
			tb.destination_type = TranslationBlock::NON_PREDICATED_INDIRECT;
		}
	}

	// Finish off with a RET.
	ctx.add_instruction(IRInstruction::ret());

	return true;
}

void CPU::register_region(captive::shared::RegionWorkUnit* rwu)
{
	printf("jit: register region %08x rwu=%p fn=%lx gen=%d\n", rwu->region_base_address, rwu, rwu->function_addr, rwu->work_unit_id);

	Region& rgn = profile_image().get_region(rwu->region_base_address);

	jit_state.region_chaining_table[rgn.address() >> 12] = (void *)rwu->function_addr;

	if (rwu->function_addr) {
		if (rwu->work_unit_id < rgn.generation()) {
			printf("jit: discarding stale translation, rwu %08x gen=%d cur gen=%d\n", rwu->region_base_address, rwu->work_unit_id, rgn.generation());
			Memory::shared_memory().free((void *)rwu->function_addr);
		} else {
			Translation *txln = new Translation((Translation::translation_fn_t)rwu->function_addr);

			rgn.translation(txln);

			// Only register entry blocks
			for (int i = 0; i < rwu->block_count; i++) {
				if (rwu->blocks[i].is_entry) {
					//printf("jit: registering block %08x\n", rwu->blocks->descriptors[i].block_addr);
					rgn.get_block(rwu->blocks[i].block_addr).translation(txln);
				}
			}
		}
	}

	rgn.set_work_unit(NULL);

	for (int i = 0; i < rwu->block_count; i++) {
		Memory::shared_memory().free(rwu->blocks[i].ir_insn);
	}

	Memory::shared_memory().free(rwu->blocks);

	rgn.status(Region::NOT_IN_TRANSLATION);
}
