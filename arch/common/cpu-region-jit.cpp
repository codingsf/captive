#include <cpu.h>
#include <interp.h>
#include <decode.h>
#include <jit.h>
#include <jit/translation-context.h>
#include <profile/image.h>
#include <profile/region.h>
#include <profile/block.h>

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

		if (block.have_translation()) {
			block.execute(this, reg_state());
			continue;
		}

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
	//printf("analysing:\n");
	for (auto region : profile_image()) {
		if (region.second->hot_block_count() > 20 && region.second->status() != Region::IN_TRANSLATION) {
			compile_region(*region.second);
			//printf("hot region: %08x, hot-blocks=%d\n", region.t2->address(), region.t2->hot_block_count());
		}
	}
}

void CPU::compile_region(Region& rgn)
{
	rgn.status(Region::IN_TRANSLATION);

	TranslationBlocks *tb = (TranslationBlocks *)cpu_data().guest_data->ir_desc_buffer;

	TranslationContext ctx(cpu_data().guest_data->ir_buffer, cpu_data().guest_data->ir_buffer_size, 0, (uint64_t)cpu_data().guest_data->code_buffer);
	uint8_t decode_data[128];
	Decode *insn = (Decode *)&decode_data[0];

	//printf("compiling region %x\n", rgn.address());
	for (auto block : rgn) {
		//printf("  generating block %x id=%d heat=%d\n", block.second->address(), ctx.current_block(), block.second->interp_count());

		tb->descriptors[tb->block_count].block_id = ctx.current_block();
		tb->descriptors[tb->block_count].block_addr = block.second->address();
		tb->descriptors[tb->block_count].heat = block.second->interp_count();
		tb->block_count++;

		uint32_t pc = block.second->address();
		do {
			// Attempt to decode the current instruction.
			if (!decode_instruction_phys(pc, insn)) {
				printf("cpu: unhandled decode fault @ %08x\n", pc);
				assert(false);
			}

			//printf("jit: translating insn @ [%08x] %s\n", insn->pc, trace().disasm().disassemble(insn->pc, decode_data));

			// Translate this instruction into the context.
			if (!jit().translate(insn, ctx)) {
				printf("jit: instruction translation failed\n");
				assert(false);
			}

			pc += insn->length;
		} while (!insn->end_of_block);

		// Finish off with a RET.
		ctx.add_instruction(jit::IRInstructionBuilder::create_ret());
	}

	printf("compiling region %x\n", rgn.address());

	// Make region translation hypercall
	asm volatile("out %0, $0xff" :: "r"(8));
}
