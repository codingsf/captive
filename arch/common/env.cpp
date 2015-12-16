#include <env.h>
#include <cpu.h>
#include <printf.h>
#include <string.h>
#include <device.h>
#include <mm.h>

typedef void (*trap_fn_t)(struct mcontext *);

extern "C" void trap_unk(struct mcontext *);
extern "C" void trap_unk_arg(struct mcontext *);

extern "C" void trap_pf(struct mcontext *);
extern "C" void trap_gpf(struct mcontext *);
extern "C" void trap_signal(struct mcontext *);
extern "C" void trap_illegal(struct mcontext *);

extern "C" void int80_handler(struct mcontext *);
extern "C" void int81_handler(struct mcontext *);
extern "C" void int82_handler(struct mcontext *);
extern "C" void int83_handler(struct mcontext *);
extern "C" void int85_handler(struct mcontext *);

extern "C" void trap_irq0(struct mcontext *);
extern "C" void trap_irq1(struct mcontext *);
extern "C" void trap_irq2(struct mcontext *);
extern "C" void trap_irq3(struct mcontext *);

struct IDT {
	uint16_t off_low;
	uint16_t sel;
	uint8_t zero0;
	uint8_t type;
	uint16_t off_mid;
	uint32_t off_high;
	uint32_t zero1;
} packed;

using namespace captive;
using namespace captive::arch;

Environment::Environment(PerCPUData *per_cpu_data) : per_cpu_data(per_cpu_data)
{
	bzero(devices, sizeof(devices));
}

Environment::~Environment()
{

}

static void set_idt(IDT* idt, trap_fn_t fn, bool allow_user = false)
{
	idt->zero0 = 0;
	idt->zero1 = 0;

	idt->off_low = ((uint64_t)fn) & 0xffff;
	idt->off_mid = (((uint64_t)fn) >> 16) & 0xffff;
	idt->off_high = (((uint64_t)fn) >> 32);

	idt->sel = 0x8;
	idt->type = 0x8e | (allow_user ? 0x60 : 0);
}

static inline void wrmsr(uint32_t msr_id, uint64_t msr_value)
{
	uint32_t low = msr_value & 0xffffffff;
	uint32_t high = (msr_value >> 32);

	asm volatile ( "rex.b wrmsr" : : "c" (msr_id), "a" (low), "d" (high) );
}

void Environment::install_gdt()
{
	struct {
		uint16_t limit;
		uint64_t base;
	} packed GDTR;

	GDTR.limit = 8 * 7;
	GDTR.base = 0x200000100;

	asm volatile("lgdt %0\n" :: "m"(GDTR));
}

void Environment::install_idt()
{
	struct {
		uint16_t limit;
		uint64_t base;
	} packed IDTR;

	IDTR.limit = sizeof(struct IDT) * 0x100;
	IDTR.base = 0x680040001000ULL;

	IDT *idt = (IDT *)IDTR.base;

	// Initialise the table with unknowns
	for (int i = 0; i < 0x100; i++) {
		set_idt(&idt[i], trap_unk);
	}

	// Exceptions with arguments
	set_idt(&idt[0x08], trap_unk_arg);
	set_idt(&idt[0x0a], trap_unk_arg);
	set_idt(&idt[0x0b], trap_unk_arg);
	set_idt(&idt[0x0c], trap_unk_arg);
	set_idt(&idt[0x11], trap_unk_arg);
	set_idt(&idt[0x1e], trap_unk_arg);

	// Fault Handlers
	set_idt(&idt[0x06], trap_illegal);
	set_idt(&idt[0x0d], trap_gpf);
	set_idt(&idt[0x0e], trap_pf);

	// IRQ Handler
	set_idt(&idt[0x30], trap_irq0);
	set_idt(&idt[0x31], trap_irq1);
	set_idt(&idt[0x32], trap_irq2);
	set_idt(&idt[0x33], trap_irq3);

	// Software-interrupt handlers
	set_idt(&idt[0x80], int80_handler, true);
	set_idt(&idt[0x81], int81_handler, true);
	set_idt(&idt[0x82], int82_handler, true);
	set_idt(&idt[0x83], int83_handler, true);
	set_idt(&idt[0x85], int85_handler, true);

	asm volatile("lidt %0\n" :: "m"(IDTR));
}

void Environment::install_tss()
{
	asm volatile("ltr %0\n" :: "a"((uint16_t)0x2b));
}

void Environment::setup_interrupts()
{
	// Enable interrupts
	asm volatile("sti\n");
}

bool Environment::init()
{
	//install_gdt();
	install_idt();
	install_tss();
	setup_interrupts();
	
	return true;
}

bool Environment::run()
{
	CPU *core = create_cpu();
	if (!core) {
		printf("error: unable to create core\n");
		return false;
	}

	CPU::set_active_cpu(core);

	if (!core->init()) {
		printf("error: unable to init core\n");
		return false;
	}
	
	prepare_bootloader();
	prepare_boot_cpu(core);
	
	core->mmu().set_page_device(GPA_TO_HVA(0x10000000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10001000));
	core->mmu().set_page_device(GPA_TO_HVA(0x1001A000));
	core->mmu().set_page_device(GPA_TO_HVA(0x1e000000));
	core->mmu().set_page_device(GPA_TO_HVA(0x1e001000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10011000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10018000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10019000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10012000));
	core->mmu().set_page_device(GPA_TO_HVA(0x1000f000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10010000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10013000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10014000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10015000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10017000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10004000));
	core->mmu().set_page_device(GPA_TO_HVA(0x1000d000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10009000));
	core->mmu().set_page_device(GPA_TO_HVA(0x1000a000));
	core->mmu().set_page_device(GPA_TO_HVA(0x1000b000));
	core->mmu().set_page_device(GPA_TO_HVA(0x1000c000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10030000));
	core->mmu().set_page_device(GPA_TO_HVA(0x1000e000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10005000));
	core->mmu().set_page_device(GPA_TO_HVA(0x100e1000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10002000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10006000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10007000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10020000));
	core->mmu().set_page_device(GPA_TO_HVA(0x10100000));

	bool result = core->run();
	delete core;

	return result;
}

bool Environment::read_core_device(CPU& cpu, uint32_t id, uint32_t reg, uint32_t& data)
{
	if (id > 15 || devices[id] == NULL) {
		printf("attempted read of invalid device %d @ %08x (%08x)\n", id, cpu.read_pc(), *(uint32_t *)cpu.read_pc());
		return false;
	}

	return devices[id]->read(cpu, reg, data);
}

bool Environment::write_core_device(CPU& cpu, uint32_t id, uint32_t reg, uint32_t data)
{
	if (id > 15 || devices[id] == NULL) {
		printf("attempted write of invalid device %d @ %08x (%08x)\n", id, cpu.read_pc(), *(uint32_t *)cpu.read_pc());
		return false;
	}

	return devices[id]->write(cpu, reg, data);
}
