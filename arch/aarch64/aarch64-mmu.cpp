#include <aarch64-mmu.h>
#include <aarch64-cpu.h>
#include <aarch64-env.h>
#include <mm.h>
#include <printf.h>

//#define DEBUG_MMU

#ifdef DEBUG_MMU
# define PRINTF_MMU(a...) printf("mmu: "#a)
#else
# define PRINTF_MMU(a...)
#endif

using namespace captive::arch;
using namespace captive::arch::aarch64;

aarch64_mmu::aarch64_mmu(aarch64_cpu& cpu) : MMU(cpu)
{

}

aarch64_mmu::~aarch64_mmu()
{

}

MMU *aarch64_mmu::create(aarch64_cpu& cpu)
{
	return new aarch64_mmu(cpu);
}

struct tt_descriptor
{
	union
	{
		struct {
			uint64_t bits;
		} bits;
		
		struct {
			uint8_t type:2;
			uint64_t ignored:62;
		} type;
		
		struct {
			uint64_t ignored;
		} invalid;
		
		struct {
			uint8_t type:2;
			uint64_t lower_block_attr:10;
			uint64_t output_address:36;
			uint8_t reserved:4;
			uint16_t upper_block_attr:12;
		} block;
		
		struct {
			uint8_t type:2;
			uint16_t ignored:10;
			uint64_t next:36;
			uint16_t x:16;
		} table;
	};
} __packed;

bool aarch64_mmu::resolve_gpa(resolution_context& context, bool have_side_effects)
{
	if (!enabled()) {
		context.allowed_permissions = context.requested_permissions;
		context.fault = MMU::NONE;
		context.pa = context.va;
		PRINTF_MMU("resolve %p -> %p\n", context.va, context.pa);
	} else {
		aarch64_cpu& core = (aarch64_cpu&)cpu();
		PRINTF_MMU("va = %p, ttbr0 = %p, ttbr1 = %p\n", context.va, core.reg_offsets.TTBR0[0], core.reg_offsets.TTBR1[0]);
		
		uint16_t idx[4];
		
		idx[0] = (context.va >> (12+27)) & 0x1ff;
		idx[1] = (context.va >> (12+18)) & 0x1ff;
		idx[2] = (context.va >> (12+9)) & 0x1ff;
		idx[3] = (context.va >> (12)) & 0x1ff;
		
		PRINTF_MMU("idx: [0] = %u, [1] = %u, [2] = %u, [3] = %u\n", idx[0], idx[1], idx[2], idx[3]);
		
		uint64_t ttbr;
		
		if (context.va < 0x800000000000) {
			ttbr = core.reg_offsets.TTBR0[0] & ~0xfffull;
		} else {
			ttbr = core.reg_offsets.TTBR1[0] & ~0xfffull;
		}
		
		PRINTF_MMU("ttbr = %016x\n", ttbr);
	
		tt_descriptor *l0 = (tt_descriptor *)guest_phys_to_vm_virt((uintptr_t)ttbr);
		l0 = &l0[idx[0]];
		
		PRINTF_MMU("l0 = %016x\n", l0->bits.bits);

		switch (l0->type.type) {
		case 0:
		case 2:	// Invalid
			context.allowed_permissions = MMU::NO_PERMISSION;
			
			if (context.requested_permissions & (MMU::USER_WRITE | MMU::KERNEL_WRITE)) {
				context.fault = MMU::WRITE_FAULT;
			} else if (context.requested_permissions & (MMU::USER_READ | MMU::KERNEL_READ)) {
				context.fault = MMU::READ_FAULT;
			} else if (context.requested_permissions & (MMU::USER_FETCH | MMU::KERNEL_FETCH)) {
				context.fault = MMU::FETCH_FAULT;
			} else {
				return false;
			}
			
			return true;
			
		case 1:	// Block
			PRINTF_MMU("block\n");
			return false;
			
		case 3: // Table
			PRINTF_MMU("table target=%p\n", l0->table.next << 12);
			break;
		}
		
		tt_descriptor *l1 = (tt_descriptor *)guest_phys_to_vm_virt((gpa_t)(l0->table.next << 12));
		l1 = &l1[idx[1]];
		
		PRINTF_MMU("l1 = %016x\n", l1->bits.bits);
		
		switch (l1->type.type) {
		case 0:
		case 2:	// Invalid
			context.allowed_permissions = MMU::NO_PERMISSION;
			
			if (context.requested_permissions & (MMU::USER_WRITE | MMU::KERNEL_WRITE)) {
				context.fault = MMU::WRITE_FAULT;
			} else if (context.requested_permissions & (MMU::USER_READ | MMU::KERNEL_READ)) {
				context.fault = MMU::READ_FAULT;
			} else if (context.requested_permissions & (MMU::USER_FETCH | MMU::KERNEL_FETCH)) {
				context.fault = MMU::FETCH_FAULT;
			} else {
				return false;
			}
			
			return true;
			
		case 1:	// Block
			PRINTF_MMU("block\n");
			return false;
			
		case 3: // Table
			PRINTF_MMU("table target=%p\n", l1->table.next << 12);
			break;
		}
		
		tt_descriptor *l2 = (tt_descriptor *)guest_phys_to_vm_virt((uintptr_t)(l1->table.next << 12));
		l2 = &l2[idx[2]];
		
		PRINTF_MMU("l2 = %016x type = %u\n", l2->bits.bits, l2->type.type);
		
		switch (l2->type.type) {
		case 1:
			PRINTF_MMU("block %p\n", l2->block.output_address << 12);
			context.fault = NONE;
			context.allowed_permissions = context.requested_permissions;
			context.pa = (l2->block.output_address << 12) | (context.va & 0x1fffff);
			return true;
			
		default:
			return false;
		}
		
		return false;
	}
	
	return true;
}
