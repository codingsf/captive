#include <mmu.h>
#include <cpu.h>
#include <mm.h>
#include <printf.h>

using namespace captive::arch;

//#define TRACK_CONTEXT_ID
//#define TRACK_SMC
//#define USE_CONTEXT_ID

#define ITLB_SIZE	8192

static struct {
	gva_t tag;
	gpa_t value;
} itlb[ITLB_SIZE];

MMU::MMU(CPU& cpu) : _cpu(cpu), _context_id(0), _enabled(false)
{
	context_id(0);
		
	for (int i = 0; i < ITLB_SIZE; i++) {
		itlb[i].tag = 0;
	}
	
	init_modes();
}

MMU::~MMU()
{

}

extern "C" uint64_t int60_modes[4];

void MMU::init_modes()
{
	_mode_to_cr3[MMUMode::NONE] = VM_PHYS_PML4_0;			// Special pre-defined mode for disabling memory accesses
	_mode_to_cr3[MMUMode::ONE_TO_ONE] = VM_PHYS_PML4_1;		// Special pre-defined mode for one-to-one guest mapping
	_mode_to_cr3[MMUMode::LOW_VM_0] = alloc_pgt();
	_mode_to_cr3[MMUMode::HIGH_VM_0] = alloc_pgt();
	_mode_to_cr3[MMUMode::LOW_VM_1] = alloc_pgt();
	_mode_to_cr3[MMUMode::HIGH_VM_1] = alloc_pgt();

	// Memoize virtual addresses for the PML4s for each mode.
	for (unsigned int i = 0; i < ARRAY_SIZE(_mode_to_pml4); i++) {
		_mode_to_pml4[i] = (page_map_t *)vm_phys_to_virt(_mode_to_cr3[i]);
	}
		
	// Initialise the upper mappings for each page table.
	for (unsigned int mode_index = 0; mode_index < ARRAY_SIZE(_mode_to_pml4); mode_index++) {
		for (unsigned int entry_index = 0x100; entry_index < 0x200; entry_index++) {
			_mode_to_pml4[mode_index]->entries[entry_index] = _mode_to_pml4[MMUMode::NONE]->entries[entry_index];
		}
	}
	
	int60_modes[0] = _mode_to_cr3[MMUMode::LOW_VM_0];
	int60_modes[1] = _mode_to_cr3[MMUMode::HIGH_VM_0];
	int60_modes[2] = _mode_to_cr3[MMUMode::LOW_VM_1];
	int60_modes[3] = _mode_to_cr3[MMUMode::HIGH_VM_1];
	
	// Start in one-to-one mode.
	mode(MMUMode::ONE_TO_ONE);
}

bool MMU::enable()
{
	if (_enabled) return true;
	
	invalidate_virtual_mappings();
	cpu().invalidate_translations();
	
	mode(MMUMode::LOW_VM_0);
	
	_enabled = true;
	return true;
}

bool MMU::disable()
{
	if (!_enabled) return true;
	
	invalidate_virtual_mappings();
	cpu().invalidate_translations();
	
	mode(MMUMode::ONE_TO_ONE);

	_enabled = false;
	return true;
}

void MMU::context_id(uint32_t ctxid)
{
	_context_id = ctxid;
}

void MMU::page_table_change()
{
	printf("mmu: page table change (ctxid=%x, mode=%u)\n", _context_id, mode());
	
	invalidate_virtual_mappings();
}

void MMU::invalidate_virtual_mappings()
{
	switch (mode()) {
	case MMUMode::NONE:
	case MMUMode::ONE_TO_ONE:
		return;
		
	default:
		break;
	}
	
	for (unsigned int i = 0; i < 0x100; i++) {
		_mode_to_pml4[MMUMode::LOW_VM_0]->entries[i].present(false);
		_mode_to_pml4[MMUMode::HIGH_VM_0]->entries[i].present(false);
		_mode_to_pml4[MMUMode::LOW_VM_1]->entries[i].present(false);
		_mode_to_pml4[MMUMode::HIGH_VM_1]->entries[i].present(false);
	}

	// Flush the TLB
	flush_tlb();
	
	// Notify the CPU to invalidate virtual mappings
	_cpu.invalidate_virtual_mappings_all();
	
	for (int i = 0; i < ITLB_SIZE; i++) {
		itlb[i].tag = 0;
	}
}

void MMU::invalidate_virtual_mapping(gva_t va)
{
#ifdef TRACK_CONTEXT_ID
	printf("mmu: invalidate address %p (ctxid=%x)\n", va, _context_id);
#endif
	
	assert(false);
	
	/*page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;
	
	Memory::get_va_table_entries((hva_t)va, pm, pdp, pd, pt);
	pt->flags(0);

	Memory::flush_page((hva_t)va);
	
	// Notify the CPU to invalidate this virtual mapping
	_cpu.invalidate_virtual_mapping_current(va);
	
	itlb[(va >> 12) % ITLB_SIZE].tag = 0;*/
}

void MMU::invalidate_virtual_mapping_by_context_id(uint32_t context_id)
{
	assert(false);
	
	/*page_map_t *pm = (page_map_t *)vm_phys_to_virt(CR3);

	// Lower 4G
	pm->entries[0].present(false);
	pm->entries[0].writable(true);
	
	// Flush the TLB
	Memory::flush_tlb();

	// Notify the CPU to invalidate virtual mappings
	_cpu.invalidate_virtual_mappings_all();
	
	for (int i = 0; i < ITLB_SIZE; i++) {
		itlb[i].tag = 0;
	}*/
}

void MMU::disable_writes()
{
	/*page_map_t *pm = current_pml4();
	
	for (unsigned int i = 0; i < 0x100; i++) {
		pm->entries[i].writable(false);
	}

	// Flush the TLB
	if (in_user_mode()) {
		asm volatile("int $0x82" ::: "rax");
	} else {
		flush_tlb();
	}*/
}

bool MMU::handle_fault(struct resolution_context& rc)
{
	switch (mode()) {
	case MMUMode::NONE:
		printf("fatal: MMU NOT READY va=%p rip=%p\n", rc.va, 0);
		abort();

	case MMUMode::ONE_TO_ONE:
		printf("fatal: ACCESS TO UNAVAILABLE GPM\n");
		abort();
		
	default:
		break;
	}
	
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;
	
	get_va_table_entries(rc.va, pm, pdp, pd, pt);
	
	assert(!(pm->huge() || pdp->huge() || pd->huge()));
	
	printf("FAULT: PRE-VA=%p\n", rc.va);
	
	gva_t real_gva;
	// Turn the faulting VA into a real GVA, based on the current mode.
	switch (mode()) {
	case MMUMode::LOW_VM_0:
		real_gva = rc.va;
		break;
	case MMUMode::HIGH_VM_0:
		assert(false);
		//rc.va = rc.va;
		break;
	case MMUMode::LOW_VM_1:
		real_gva = rc.va + 0xffff000000000000ULL;
		break;
	case MMUMode::HIGH_VM_1:
		real_gva = rc.va + 0xffff800000000000ULL;
		break;
		
	default:
		printf("fatal: not implemented %p %u\n", rc.va, mode());
		abort();
	}
	
	printf("FAULT: POST-VA=%p\n", real_gva);
	
	rc.va = real_gva;
	
	if (!resolve_gpa(rc, true)) {
		printf("fatal: guest mmu txln failed\n");
		return false;
	}
	
	printf("fatal: guest mmu txln succeeded va=%p, pa=%p, fault=%u, allowed=%x\n", rc.va, rc.pa, rc.fault, rc.allowed_permissions);
	
	pt->base_address(guest_phys_to_vm_phys(rc.pa));
	
	if (rc.allowed_permissions != MMU::NO_PERMISSION) {
		if (!!(rc.requested_permissions & MMU::KERNEL_READ)) {
			if (!!(rc.allowed_permissions & MMU::KERNEL_READ)) {
				pt->present(true);
			} else {
				pt->present(false);
			}
		}
		
		if (!!(rc.requested_permissions & MMU::KERNEL_WRITE)) {
			if (!!(rc.allowed_permissions & MMU::KERNEL_WRITE)) {
				pt->present(true);
				pt->writable(true);
			} else {
				pt->writable(false);
			}
		}
		
		pt->allow_user(false);
	}
	
	return true;
	
	/*
	Memory::get_va_table_entries(host_va, pm, pdp, pd, pt);

	if (!pm->present() || !pm->writable()) {
		// The associated Page Directory Pointer Table is not marked as
		// present, so invalidate the page directory pointer table, and
		// mark it as present.

		// Determine the base address of the page directory pointer table.
		page_dir_ptr_t *base = (page_dir_ptr_t *)((uint64_t)pdp & ~0xfffULL);
		
		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			if (!pm->present()) base->entries[i].present(false);
			if (!pm->writable()) base->entries[i].writable(false);
		}

		// Set the PRESENT and WRITABLE flag for the page directory pointer table.
		pm->present(true);
		pm->writable(true);
	}

	if (!pdp->present() || !pdp->writable()) {
		// The associated Page Directory Table is not marked as present,
		// so invalidate the page directory table and mark it as
		// present.

		// Determine the base address of the page directory table.
		page_dir_t *base = (page_dir_t *)((uint64_t)pd & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			if (!pdp->present()) base->entries[i].present(false);
			if (!pdp->writable()) base->entries[i].writable(false);
		}

		// Set the PRESENT flag for the page directory table.
		pdp->present(true);
		pdp->writable(true);
	}

	bool maybe_smc;
	if (!pd->present() || !pd->writable()) {
		// The associated Page Table is not marked as present, so
		// invalidate the page table and mark it as present.

		// Determine the base address of the page table.
		page_table_t *base = (page_table_t *)((uint64_t)pt & ~0xfffULL);

		// Loop over each entry and clear the flags.
		for (int i = 0; i < 0x200; i++) {
			if (!pd->present()) base->entries[i].flags(0);
			else if (!pd->writable()) base->entries[i].writable(false);
		}

		// Set the PRESENT flag for the page table.
		pd->present(true);
		pd->writable(true);
	}
	
	// If the fault happened because of a device access, catch this early
	// before we go over the lookup rigmaroll.
	if (pt->device()) {
		rc.pa = (gpa_t)((pt->base_address() & 0xffffffff) | (rc.va & 0xfff));
		goto handle_device;
	}

	if (!enabled()) {
		rc.fault = NONE;
		rc.pa = (gpa_t)rc.va;
		rc.allowed_permissions = ALL_READ_WRITE_FETCH;

		pt->base_address(guest_phys_to_vm_phys(rc.pa));
		pt->present(true);
		pt->allow_user(true);
		pt->writable(true);
		
		//printf("mmu: %08x disabled: va=%08x pa=%08x hpa=%lx, hva=%lx\n", _cpu.read_pc(), rc.va, rc.pa, pt->base_address(), GPA_TO_HVA(pt->base_address()));

		if (is_page_device((hva_t)guest_phys_to_vm_virt(pt->base_address()))) {
			pt->device(true);
			pt->present(false);

			goto handle_device;
		}
	} else {
		if (!resolve_gpa(rc, true)) {
			return false;
		}
		
		if (rc.fault == NONE) {
			// Update the corresponding page table address entry and mark it as
			// present and writable.  Note, assigning the base address will mask
			// out the bottom twelve bits of the incoming address, to ensure it's
			// a page-aligned value.
			pt->base_address((uint64_t)guest_phys_to_vm_phys(rc.pa));
			pt->present(true);
			pt->allow_user((rc.allowed_permissions & USER_ALL) != 0);
			pt->writable((rc.allowed_permissions & (USER_WRITE | KERNEL_WRITE)) != 0);
			pt->executable((rc.allowed_permissions & (USER_FETCH | KERNEL_FETCH)) != 0);

			if (is_page_device((hva_t)guest_phys_to_vm_virt(rc.pa))) {
				pt->device(true);
				pt->present(false);

				Memory::flush_page(host_va);
				goto handle_device;
			}

			/*printf("mmu: [%08x] no fault: va=%08x, pa=%08x, rperms=%x, aperms=%x, ctxid=%x\n",
					_cpu.read_pc(),
					rc.va,
					rc.pa,
					rc.requested_permissions,
					rc.allowed_permissions,
					*cpu().tagged_registers().CTXID);* /
		} else {
			pt->present(false);
			pt->device(false);
			//pt->dirty(true);
			//printf("mmu: %08x fault: va=%08x type=%s mode=%s fault-type=%s\n", _cpu.read_pc(), va, mem_access_types[info.type], mem_access_modes[info.mode], mem_fault_types[fault]);
		}

		//printf("mmu: %08x enabled: va=%08x pa=%08x hpa=%lx, hva=%lx, permissions=%02x fault=%d\n", _cpu.read_pc(), rc.va, rc.pa, pt->base_address(), GPA_TO_HVA(pt->base_address()), rc.allowed_permissions, rc.fault);
	}

	if (pt->present() && rc.is_write() && rc.fault == NONE) {
		if (clear_if_page_executed((hva_t)guest_phys_to_vm_virt(pt->base_address()))) {
#ifdef TRACK_SMC
			printf("mmu: self modifying code @ va=%08x, pa=%08x, pc=%08x\n", rc.va, rc.pa, _cpu.read_pc());
#endif
			
			cpu().invalidate_translation_phys((gpa_t)(pt->base_address() & 0xfffff000ULL));

			//printf("PC: %08x, VA: %08x\n", _cpu.read_pc(), (uint32_t)va);
			if ((read_pc() & ~0xfff) == (uint32_t)(rc.va & ~0xfff)) {
				//printf("mmu: smc: same page %08x\n", rc.va);
				rc.fault = SMC_FAULT;
			} else {
				//printf("mmu: smc: other page %08x\n", rc.va);
			}
		}
	}

	Memory::flush_page(host_va);
	return true;

handle_device:
	rc.fault = DEVICE_FAULT;
	return true;*/
}

MMUMode::MMUMode MMU::gva_to_mode(gva_t gva) const
{
	return (MMUMode::MMUMode)(((gva >> 47) & 3) + 2);
	/*if (gva < 0x0000800000000000ULL) {
		return MMUMode::LOW_VM_0;
	} else if (gva < 0x0001000000000000) {
		return MMUMode::HIGH_VM_0;
	} else if (gva < 0xffff000000000000) {
		assert(false && "INVALID VIRTUAL ADDRESS TO MODE MAPPING");
	} else if (gva < 0xffff800000000000) {
		return MMUMode::LOW_VM_1;
	} else {
		return MMUMode::HIGH_VM_1;
	}*/
}

bool MMU::quick_fetch(gva_t va, gpa_t& pa, bool user_mode)
{
	page_map_t *pml4;
	
	MMUMode::MMUMode mode = gva_to_mode(va);
	
	printf("XXXXXXX QUICK FETCH VA=%p, MODE=%u\n", va, mode);
	
	pml4 = _mode_to_pml4[mode];
	
	table_idx_t pm_idx = 0, pdp_idx, pd_idx, pt_idx;
	va_table_indicies(convert_guest_va_to_local_va(va), pm_idx, pdp_idx, pd_idx, pt_idx);

	page_map_entry_t* pm;
	pm = &(pml4->entries[pm_idx]);
	if (!pm->present()) {
		return false;
	}

	page_dir_ptr_entry_t* pdp;
	pdp = &((page_dir_ptr_t *)vm_phys_to_virt(pm->base_address()))->entries[pdp_idx];
	if (!pdp->present()) {
		return false;
	}

	page_dir_entry_t* pd;
	pd = &((page_dir_t *)vm_phys_to_virt(pdp->base_address()))->entries[pd_idx];
	if (!pd->present()) {
		return false;
	}
	
	if (pd->huge()) {
		if (!pd->allow_user() && user_mode) {
			return false;
		}
		
		pa = vm_phys_to_guest_phys(pd->base_address() | (va & 0x1fffffULL));
	} else {
		page_table_entry_t* pt = &((page_table_t *)vm_phys_to_virt(pd->base_address()))->entries[pt_idx];
		if (!pt->present() || (!pt->allow_user() && user_mode)) {
			return false;
		}
		
		pa = vm_phys_to_guest_phys(pt->base_address() | (va & 0xfffULL));
	}
	
	return true;
}

bool MMU::translate_fetch(struct resolution_context& rc)
{
	uint64_t va_page = rc.va >> 12;
	uint32_t cache_idx = va_page % ITLB_SIZE;
	
	if (va_page != 0 && itlb[cache_idx].tag == va_page) {
		rc.fault = NONE;
		rc.pa = itlb[cache_idx].value | (rc.va & 0xfff);
		return true;
	} else {
		rc.requested_permissions = _cpu.kernel_mode() ? KERNEL_FETCH : USER_FETCH;

		if (!resolve_gpa(rc, true))
			return false;
		
		if (rc.fault == NONE) {
			itlb[cache_idx].tag = va_page;
			itlb[cache_idx].value = rc.pa & ~0xfff;
		}
		
		return true;
	}
}
