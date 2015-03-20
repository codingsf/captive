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

extern "C" void int80_handler(struct mcontext *);
extern "C" void int81_handler(struct mcontext *);
extern "C" void int82_handler(struct mcontext *);

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
	IDTR.base = 0x200000300;

	IDT *idt = (IDT *)IDTR.base;

	// Initialise the table with unknowns
	for (int i = 0; i < 0x100; i++) {
		set_idt(&idt[i], trap_unk);
	}

	// NMI for signalling
	set_idt(&idt[0x2], trap_signal, false);

	// Exceptions with arguments
	set_idt(&idt[0x08], trap_unk_arg);
	set_idt(&idt[0x0a], trap_unk_arg);
	set_idt(&idt[0x0b], trap_unk_arg);
	set_idt(&idt[0x0c], trap_unk_arg);
	set_idt(&idt[0x11], trap_unk_arg);
	set_idt(&idt[0x1e], trap_unk_arg);

	// Fault Handlers
	set_idt(&idt[0x0d], trap_gpf);
	set_idt(&idt[0x0e], trap_pf);

	set_idt(&idt[0x80], int80_handler, true);
	set_idt(&idt[0x81], int81_handler, true);
	set_idt(&idt[0x82], int82_handler, true);

	asm volatile("lidt %0\n" :: "m"(IDTR));
}

void Environment::install_tss()
{
	asm volatile("ltr %0\n" :: "a"((uint16_t)0x2b));
}

bool Environment::init()
{
	install_gdt();
	install_idt();
	install_tss();

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

	bool result = core->run();
	delete core;

	return result;
}

bool Environment::read_core_device(CPU& cpu, uint32_t id, uint32_t reg, uint32_t& data)
{
	if (id > 15 || devices[id] == NULL) {
		printf("attempted read of invalid device %d\n", id);
		return false;
	}

	return devices[id]->read(cpu, reg, data);
}

bool Environment::write_core_device(CPU& cpu, uint32_t id, uint32_t reg, uint32_t data)
{
	if (id > 15 || devices[id] == NULL) {
		printf("attempted write of invalid device %d\n", id);
		return false;
	}

	return devices[id]->write(cpu, reg, data);
}
