#include <mmu.h>
#include <cpu.h>
#include <mm.h>
#include <printf.h>

using namespace captive::arch;

static const char *mem_access_types[] = { "read", "write", "fetch" };
static const char *mem_access_modes[] = { "user", "kernel" };
static const char *mem_fault_types[] = { "none", "read", "write", "fetch" };

MMU::MMU(CPU& cpu) : _cpu(cpu)
{
	//printf("mmu: allocating guest pdps\n");

	page_map_t *pm = (page_map_t *)Memory::phys_to_virt(CR3);
	page_dir_ptr_t *pdp = (page_dir_ptr_t *)Memory::phys_to_virt((pa_t)pm->entries[0].base_address());

	for (int i = 0; i < 4; i++) {
		pdp->entries[i].base_address((uint64_t)Memory::alloc_page().pa);
		pdp->entries[i].flags(0);
		pdp->entries[i].present(false);
		pdp->entries[i].writable(true);
		pdp->entries[i].allow_user(true);
	}

	Memory::flush_tlb();
}

MMU::~MMU()
{

}

void MMU::set_page_executed(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->executed(true);
}

void MMU::clear_page_executed(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->executed(false);
}

bool MMU::is_page_executed(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	return pt->executed();
}

bool MMU::clear_if_page_executed(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);

	if (!pt->executed()) return false;
	
	pt->executed(false);
	return true;
}

bool MMU::is_page_dirty(va_t va)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);

	return pt->dirty();
}

void MMU::set_page_dirty(va_t va, bool dirty)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries(va, pm, pdp, pd, pt);
	pt->dirty(dirty);
}

uint32_t MMU::page_checksum(va_t va)
{
	uint32_t checksum = 0;
	for (int i = 0; i < 0x400; i++) {
		checksum ^= ((uint32_t *)va)[i];
	}

	return checksum;
}

bool MMU::clear_vma()
{
	page_map_t *pm = (page_map_t *)Memory::phys_to_virt(CR3);
	page_dir_ptr_t *pdp = (page_dir_ptr_t *)Memory::phys_to_virt((pa_t)pm->entries[0].base_address());

	// Clear the present map on the 4G mapping
	for (int i = 0; i < 4; i++) {
		pdp->entries[i].present(false);
		pdp->entries[i].writable(true);
	}

	// Flush the TLB
	Memory::flush_tlb();

	return true;
}

void MMU::disable_writes()
{
	page_map_t *pm = (page_map_t *)Memory::phys_to_virt(CR3);
	page_dir_ptr_t *pdp = (page_dir_ptr_t *)Memory::phys_to_virt((pa_t)pm->entries[0].base_address());

	// Clear the present map on the 4G mapping
	for (int i = 0; i < 4; i++) {
		pdp->entries[i].writable(false);
	}

	// Flush the TLB
	if (in_kernel_mode()) {
		Memory::flush_tlb();
	} else {
		asm volatile("int $0x83\n" ::: "rax");
	}
}

bool MMU::handle_fault(gva_t va, gpa_t& out_pa, const access_info& info, resolution_fault& fault)
{
	page_map_entry_t *pm;
	page_dir_ptr_entry_t *pdp;
	page_dir_entry_t *pd;
	page_table_entry_t *pt;

	Memory::get_va_table_entries((va_t)(uint64_t)va, pm, pdp, pd, pt);

	//printf("mmu: handle fault: %x %p (%x) %p (%x) %p (%x) %p (%x)\n", va, pm, pm->data, pdp, pdp->data, pd, pd->data, pt, pt->data);

	if (!pm->present()) {
		// The associated Page Directory Pointer Table is not marked as
		// present, so invalidate the page directory pointer table, and
		// mark it as present.

		// Determine the base address of the page directory pointer table.
		page_dir_ptr_t *base = (page_dir_ptr_t *)((uint64_t)pdp & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			base->entries[i].present(false);
		}

		// Set the PRESENT flag for the page directory pointer table.
		pm->present(true);
	}

	if (!pdp->present()) {
		// The associated Page Directory Table is not marked as present,
		// so invalidate the page directory table and mark it as
		// present.

		// Determine the base address of the page directory table.
		page_dir_t *base = (page_dir_t *)((uint64_t)pd & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			base->entries[i].present(false);
		}

		// Set the PRESENT flag for the page directory table.
		pdp->present(true);
	}

	pdp->writable(true);

	if (!pd->present()) {
		// The associated Page Table is not marked as present, so
		// invalidate the page table and mark it as present.

		// Determine the base address of the page table.
		page_table_t *base = (page_table_t *)((uint64_t)pt & ~0xfffULL);

		// Loop over each entry and clear the PRESENT flag.
		for (int i = 0; i < 0x200; i++) {
			base->entries[i].present(false);
			base->entries[i].device(false);
			base->entries[i].allow_user(false);
		}

		// Set the PRESENT flag for the page table.
		pd->present(true);
	}

	// If the fault happened because of a device access, catch this early
	// before we go over the lookup rigmaroll.
	if (pt->device()) {
		out_pa = (gpa_t)((pt->base_address() & 0xffffffff) | (va & 0xfff));
		goto handle_device;
	}

	if (!enabled()) {
		fault = NONE;

		pt->base_address((uint64_t)gpa_to_hpa((gpa_t)va));
		pt->present(true);
		pt->allow_user(true);
		pt->writable(info.is_write());

		if (is_device((gpa_t)va)) {
			pt->device(true);
			pt->present(false);
		}

		if (pt->device()) {
			out_pa = (gpa_t)va;
			goto handle_device;
		}
	} else {
		gpa_t pa;
		if (!resolve_gpa(va, pa, info, fault)) {
			return false;
		}

		if (fault == NONE) {
			// Update the corresponding page table address entry and mark it as
			// present and writable.  Note, assigning the base address will mask
			// out the bottom twelve bits of the incoming address, to ensure it's
			// a page-aligned value.
			pt->base_address((uint64_t)gpa_to_hpa(pa));
			pt->present(true);
			pt->allow_user(info.is_user());
			pt->writable(info.is_write());

			if (is_device(pa)) {
				pt->device(true);
				pt->present(false);

				out_pa = pa;
				goto handle_device;
			}

			//printf("mmu: %08x no fault: va=%08x pa=%08x type=%s mode=%s\n", _cpu.read_pc(), va, pa, mem_access_types[info.type], mem_access_modes[info.mode]);
		} else {
			pt->present(false);
			pt->device(false);
			//pt->dirty(true);
			//printf("mmu: %08x fault: va=%08x type=%s mode=%s fault-type=%s\n", _cpu.read_pc(), va, mem_access_types[info.type], mem_access_modes[info.mode], mem_fault_types[fault]);
		}
	}

	if (pt->present() && info.is_write()) {
		va_t va_for_gpa = (va_t)(0x100000000ULL | pt->base_address());

		// TODO: TEST+CLEAR
		if (clear_if_page_executed(va_for_gpa)) {
			cpu().invalidate_executed_page((pa_t)pt->base_address(), (va_t)(uint64_t)va);
		}
	}

	Memory::flush_page((va_t)(uint64_t)va);
	return true;

handle_device:
	Memory::flush_page((va_t)(uint64_t)va);
	fault = DEVICE_FAULT;

	return true;
}

bool MMU::is_device(gpa_t gpa)
{
	if (gpa >= 0x101e0000 && gpa < 0x101e1000) return true;
	if (gpa >= 0x101e2000 && gpa < 0x101e3000) return true;
	if (gpa >= 0x101e3000 && gpa < 0x101e4000) return true;
	if (gpa >= 0x10140000 && gpa < 0x10141000) return true;
	if (gpa >= 0x10000000 && gpa < 0x10001000) return true;
	if (gpa >= 0x10003000 && gpa < 0x10004000) return true;
	if (gpa >= 0x10006000 && gpa < 0x10007000) return true;
	if (gpa >= 0x10007000 && gpa < 0x10008000) return true;
	if (gpa >= 0x101f1000 && gpa < 0x101f2000) return true;
	if (gpa >= 0x11001000 && gpa < 0x11002000) return true;

	return false;
}

#define CACHE_SIZE 4096

static struct {
	gva_t tag;
	gpa_t value;
} cache[CACHE_SIZE];

bool MMU::virt_to_phys(gva_t va, gpa_t& pa, resolution_fault& fault)
{
	access_info info;

	info.type = ACCESS_FETCH;
	info.mode = _cpu.kernel_mode() ? ACCESS_KERNEL : ACCESS_USER;
	
	if (va != 0 && cache[va % CACHE_SIZE].tag == va) {
		pa = cache[va % CACHE_SIZE].value;
		return true;
	} else {
		bool r = resolve_gpa(va, pa, info, fault, true);
		
		if (r) {
			cache[va % CACHE_SIZE].tag = va;
			cache[va % CACHE_SIZE].value = pa;
		}
		
		return r;
	}
}

void MMU::clear_cache()
{
	for (int i = 0; i < CACHE_SIZE; i++) {
		cache[i].tag = 0;
	}
}
