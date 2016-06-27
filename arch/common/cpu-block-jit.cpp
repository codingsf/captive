#include <cpu.h>
#include <decode.h>
#include <disasm.h>
#include <jit.h>
#include <safepoint.h>
#include <jit/translation-context.h>
#include <jit/block-compiler.h>
#include <shared-jit.h>

#include <profile/image.h>
#include <profile/region.h>
#include <profile/block.h>

#include <set>
#include <map>
#include <list>

//#define REG_STATE_PROTECTION
//#define DEBUG_TRANSLATION

extern safepoint_t cpu_safepoint;

using namespace captive::arch;
using namespace captive::arch::jit;
using namespace captive::arch::profile;

extern "C" int block_trampoline(void *, void*);

extern "C" void prebuilt_11000(void);
extern "C" void prebuilt_11010(void);

bool CPU::run_block_jit()
{
	printf("cpu: starting block-jit cpu execution\n");
	printf("--------------------------------------------------------------------\n");
	
	//trace().enable();
	//jit().trace(true);

	// Create a safepoint for returning from a memory access fault
	int rc = record_safepoint(&cpu_safepoint);
	if (rc > 0) {
		// We're no longer executing a translation
		_exec_txl = false;
		
		// Make sure interrupts are enabled.
		__local_irq_enable();
	}

	//ensure_privilege_mode();
	
	if (kernel_mode()) {
		if (!in_kernel_mode()) {
			switch_to_kernel_mode();
		}
	} else {
		if (!in_user_mode()) {
			switch_to_user_mode();
		}
	}
	
	return run_block_jit_safepoint();
}

bool CPU::run_block_jit_safepoint()
{
	bool step_ok = true;

	Region *rgn = NULL;
	uint32_t region_virt_base = 1;
	uint32_t region_phys_base = 1;

	do {
		// Check the ISR to determine if there is an interrupt pending,
		// and if there is, instruct the interpreter to handle it.
		if (unlikely(cpu_data().isr)) {
			// TODO: Edge?
			handle_irq(cpu_data().isr);
		}
		
		// Check to see if there are any pending actions coming in from
		// the hypervisor.
		if (unlikely(cpu_data().async_action)) {
			bool result = handle_pending_action(cpu_data().async_action);
			cpu_data().async_action = 0;
			step_ok = !cpu_data().halt;
			
			if (!result) continue;
		}
		
		gva_t virt_pc = (gva_t)read_pc();

		if (PAGE_ADDRESS_OF(virt_pc) != region_virt_base) {
			hpa_t phys_pc;
			if (!Memory::quick_fetch((hva_t)virt_pc, phys_pc, !kernel_mode())) {
				// This will perform a FETCH with side effects, so that we can impose the
				// correct permissions checking for the block we're about to execute.
				MMU::resolution_context rc(virt_pc);
				if (unlikely(!mmu().translate_fetch(rc))) fatal("mmu: failed to translate for fetch\n");

				// If there was a fault, then switch back to the safe-point.
				if (unlikely(rc.fault)) {
					if (!handle_mmu_fault(rc.fault)) return false;
					continue;
				}
				
				phys_pc = rc.pa;
			}

			rgn = image->get_region((gpa_t)phys_pc);
			region_virt_base = PAGE_ADDRESS_OF(virt_pc);
			region_phys_base = PAGE_ADDRESS_OF(phys_pc);

			mmu().set_page_executed(GPA_TO_HVA(region_phys_base));
		}
		
		assert_privilege_mode();
		
		Block *blk = rgn->get_block(PAGE_OFFSET_OF(virt_pc));
		if (blk->txln) {
			auto ptr = block_txln_cache->entry_ptr(virt_pc >> 2);
			ptr->tag = virt_pc;
			ptr->fn = (void *)blk->txln;
			
			_exec_txl = true;
			step_ok = block_trampoline(&jit_state, (void*)blk->txln) == 0;	
			_exec_txl = false;
		} else {
			blk->txln = compile_block(rgn, blk, *tagged_registers().ISA, region_phys_base | PAGE_OFFSET_OF(virt_pc));
			mmu().disable_writes();
			
			_exec_txl = true;
			step_ok = block_trampoline(&jit_state, (void*)blk->txln) == 0;
			_exec_txl = false;
		}
		
		/*if (PAGE_ADDRESS_OF(read_pc()) == PAGE_ADDRESS_OF(virt_pc)) {
			printf("*** Intra-page dispatch\n");
		}*/
		
		//printf("slc: f=%x, t=%x, %lx\n", virt_pc, read_pc(), jit_state.self_loop_count);
	} while(step_ok);
		
	if (!step_ok) printf("step was not okay\n");
	
	return true;
}

captive::shared::block_txln_fn CPU::compile_block(Region *rgn, Block *blk, uint8_t isa_mode, gpa_t pa)
{
	TranslationContext ctx(malloc::data_alloc);
	if (!translate_block(ctx, isa_mode, pa)) {
		fatal("jit: block translation failed\n");
	}
	
	BlockCompiler compiler(ctx, malloc::code_alloc, isa_mode, pa, tagged_registers());
	captive::shared::block_txln_fn fn;
	if (!compiler.compile(fn)) {
		fatal("jit: block compilation failed\n");
	}

	malloc::data_alloc.free((void *)ctx.get_ir_buffer());
	return fn;
}

//#define DEBUG_TRANSLATION

bool CPU::translate_block(TranslationContext& ctx, uint8_t isa, gpa_t pa)
{
	using namespace captive::shared;

	// We MUST begin in block zero.
	assert(ctx.current_block() == 0);
	
#ifdef DEBUG_TRANSLATION
	printf("jit: translating block %x in mode %d\n", pa, isa);
#endif
	
	std::set<uint32_t> seen_pcs;
	seen_pcs.insert(pa);
	Decode *insn = get_decode(0);

	int insn_count = 0;
	
	gpa_t pc = pa;
	gpa_t page = PAGE_ADDRESS_OF(pc);
	do {
		// Attempt to decode the current instruction.
		if (!decode_instruction_phys(isa, pc, insn)) {
			printf("jit: unhandled decode fault @ isa=%d %08x (%08x)\n", isa, pc, *(uint32_t *)(GPA_TO_HVA(insn->pc)));
			return false;
		}
		
		ctx.add_instruction(IRInstruction::barrier(IROperand::pc(insn->pc), IROperand::const32(*(uint32_t *)(GPA_TO_HVA(insn->pc)))));
		
#ifdef DEBUG_TRANSLATION
		printf("jit: translating insn @ [%08x] (%08x) %s\n", insn->pc, *(uint32_t *)GPA_TO_HVA(insn->pc), trace().disasm().disassemble(insn->pc, (const uint8_t *)insn));
#endif

		if (unlikely(cpu_data().verbose_enabled)) {
			ctx.add_instruction(IRInstruction::count(IROperand::pc(insn->pc), IROperand::const32(0)));
		}

		// Translate this instruction into the context.
		if (unlikely(jit().trace())) {
			ctx.add_instruction(IRInstruction::trace_start());
		}
		
		if (!jit().translate(insn, ctx)) {
			printf("jit: instruction translation failed: ir=%08x %s\n", *(uint32_t *)(insn->pc), trace().disasm().disassemble(insn->pc, (const uint8_t *)insn));
			return false;
		}
		
		if (unlikely(jit().trace())) {
			ctx.add_instruction(IRInstruction::trace_end());
		}

		pc += insn->length;
		insn_count++;

		if(insn->end_of_block) {
			JumpInfo ji = get_instruction_jump_info(insn);
			if(!insn->is_predicated && ji.type == JumpInfo::DIRECT && !seen_pcs.count(ji.target)) {
				pc = ji.target;
				seen_pcs.insert(ji.target);
				continue;
			}

			break;
		}
	} while (PAGE_ADDRESS_OF(pc) == page && insn_count < 200);
	
	// Branch optimisation log
	bool can_dispatch = false;
	uint32_t target_pc = 0, fallthrough_pc = 0;

	if (insn->end_of_block) {
		JumpInfo ji = get_instruction_jump_info(insn);
		
		if (ji.type == JumpInfo::DIRECT) {
				target_pc = ji.target;
								
				fallthrough_pc = insn->pc + insn->length;
			
				uint32_t target_page = target_pc &  0xfffff000;
				uint32_t ft_page = fallthrough_pc & 0xfffff000;
			
				// Can only dispatch if both jump targets land on the same page as the jump source.
				if((target_page == page) && (ft_page == page)) {
					can_dispatch = true;
				}
					
				if (!insn->is_predicated) {
					if ((target_page == page)) can_dispatch = true;
					fallthrough_pc = 0;
				}
		}
	}
	
	if (can_dispatch) {
		Region *rgn = image->get_region(page);
		
		void *target_block = NULL, *ft_block = NULL;
		
		if (target_pc) {
			target_block = (void*)rgn->get_block(PAGE_OFFSET_OF(target_pc))->txln;
		}
		
		if (fallthrough_pc) {
			ft_block = (void*)rgn->get_block(PAGE_OFFSET_OF(fallthrough_pc))->txln;
		}
		
		if (target_pc == pa) {
			ctx.add_instruction(IRInstruction::loop());
		} else {
			if (target_block) {
				ctx.add_instruction(IRInstruction::dispatch(IROperand::const32(target_pc & 0xfff), IROperand::const32(fallthrough_pc & 0xfff), IROperand::const64((uint64_t)target_block), IROperand::const64((uint64_t)ft_block)));
			} else {
				ctx.add_instruction(IRInstruction::ret());
			}
		}
	} else {
		ctx.add_instruction(IRInstruction::ret());
	}
	
	return true;
}
