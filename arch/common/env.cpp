#include <env.h>
#include <cpu.h>
#include <printf.h>
#include <string.h>
#include <device.h>
#include <mm.h>

typedef void (*trap_fn_t)(void);

extern "C" void trap_unk(void);
extern "C" void trap_dbz(void);
extern "C" void trap_dbg(void);
extern "C" void trap_nmi(void);
extern "C" void trap_pf(void);
extern "C" void trap_gpf(void);

static trap_fn_t trap_fns[] = {
	trap_dbz,
	trap_dbg,
	trap_nmi,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_gpf,
	trap_pf,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
	trap_unk,
};

struct IDT {
	uint16_t off_low;
	uint16_t sel;
	uint8_t zero0;
	uint8_t type;
	uint16_t off_mid;
	uint32_t off_high;
	uint32_t zero1;
} packed;

using namespace captive::arch;

Environment::Environment()
{
	bzero(devices, sizeof(devices));
}

Environment::~Environment()
{

}

static void set_idt(IDT* idt, trap_fn_t fn)
{
	idt->zero0 = 0;
	idt->zero1 = 0;

	idt->off_low = ((uint64_t)fn) & 0xffff;
	idt->off_mid = (((uint64_t)fn) >> 16) & 0xffff;
	idt->off_high = (((uint64_t)fn) >> 32);

	idt->sel = 0x8;
	idt->type = 0x8e;
}

bool Environment::init()
{
	struct {
		uint16_t limit;
		uint64_t base;
	} packed GDTR;

	GDTR.limit = 8 * 3;
	GDTR.base = 0x200000010;

	asm volatile("lgdt %0\n" :: "m"(GDTR));

	struct {
		uint16_t limit;
		uint64_t base;
	} packed IDTR;

	IDTR.limit = sizeof(struct IDT) * 32;
	IDTR.base = 0x200000030;

	IDT *idt = (IDT *)0x200000030;

	for (int i = 0; i < sizeof(trap_fns) / sizeof(trap_fns[0]); i++) {
		set_idt(idt++, trap_fns[i]);
	}

	asm volatile("lidt %0\n" :: "m"(IDTR));

	return true;
}

bool Environment::run(unsigned int ep)
{
	CPU *core = create_cpu();
	if (!core) {
		printf("error: unable to create core\n");
		return false;
	}

	if (!core->init(ep)) {
		printf("error: unable to init core\n");
		return false;
	}

	captive::arch::active_cpu = core;

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
