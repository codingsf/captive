#include <arm-mmu.h>
#include <arm-cpu.h>
#include <arm-env.h>
#include <mm.h>
#include <printf.h>

using namespace captive::arch;
using namespace captive::arch::arm;

ArmMMU::ArmMMU(ArmCPU& cpu) : MMU(cpu.env()), _cpu(cpu), _coco((devices::CoCo&)*cpu.env().lookup_core_device(15))
{

}

ArmMMU::~ArmMMU()
{

}

bool ArmMMU::enable()
{
	if (_enabled) return true;

	clear_vma();

	_enabled = true;
	printf("mmu: enabled\n");
	return true;
}

bool ArmMMU::disable()
{
	if (!_enabled) return true;

	clear_vma();

	_enabled = false;
	printf("mmu: disabled\n");
	return true;
}

va_t ArmMMU::temp_map(gpa_t gpa)
{
	va_t vaddr_base = (va_t)0x220000000;
	va_t vaddr = vaddr_base;

	for (int i = 0; i < 4; i++) {
		page_map_entry_t *pm;
		page_dir_ptr_entry_t *pdp;
		page_dir_entry_t *pd;
		page_table_entry_t *pt;

		Memory::get_va_table_entries(vaddr, pm, pdp, pd, pt);

		pt->base_address(0x100000000 | (((uint64_t)gpa) + (0x1000 * i)));
		pt->present(true);

		vaddr = (va_t)((uint64_t)vaddr + 0x1000);
	}

	return vaddr_base;
}

bool ArmMMU::resolve_gpa(gva_t va, gpa_t& pa)
{
	if (!_enabled) {
		pa = va;
		return true;
	}

	//printf("mmu: resolve va=%08x, ttbr0=%x\n", va, _coco.TTBR0());

	// Acquire the actual TTBR
	tt_base ttbr = (tt_base)temp_map(_coco.TTBR0());
	if (!ttbr) {
		return false;
	}

	// Calculate the ttbr index
	uint16_t ttbr_idx = va >> 20;
	//printf("ttb entry: type=%d, addr=%x\n", ttbr[ttbr_idx].type(), ttbr[ttbr_idx].base_addr());

	switch (ttbr[ttbr_idx].type()) {
	case tt_entry::TT_ENTRY_COARSE:
		return resolve_coarse_page(va, pa, ttbr[ttbr_idx]);
	case tt_entry::TT_ENTRY_FINE:
		return resolve_fine_page(va, pa, ttbr[ttbr_idx]);
	case tt_entry::TT_ENTRY_SECTION:
		return resolve_section(va, pa, ttbr[ttbr_idx]);
	case tt_entry::TT_ENTRY_FAULT:
		return false;
	}

	return false;
}

bool ArmMMU::resolve_coarse_page(gva_t va, gpa_t& pa, tt_entry& entry)
{
	printf("ttb coarse\n");
	return false;
}

bool ArmMMU::resolve_fine_page(gva_t va, gpa_t& pa, tt_entry& entry)
{
	printf("ttb fine\n");
	return false;
}

bool ArmMMU::resolve_section(gva_t va, gpa_t& pa, tt_entry& entry)
{
	st_base st = (st_base)temp_map(entry.base_addr());
	uint16_t st_idx = va & ((1 << 19) - 1);

	printf("ttb section: %x\n", st[st_idx].data);
	return false;
}
