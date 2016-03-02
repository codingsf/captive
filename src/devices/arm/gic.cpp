#include <devices/arm/gic.h>
#include <hypervisor/guest.h>
#include <hypervisor/cpu.h>
#include <captive.h>

DECLARE_CONTEXT(GIC);
DECLARE_CHILD_CONTEXT(GICDistributor, GIC);
DECLARE_CHILD_CONTEXT(GICCPU, GIC);

//#define DEBUG_IRQ

using namespace captive::devices::arm;

GICDistributorInterface::GICDistributorInterface(GIC& owner) : owner(owner), ctrl(0)
{
	for (int i = 0; i < 24; i++) {
		cpu_targets[i] = 0xffffffff;
	}
}

GICDistributorInterface::~GICDistributorInterface()
{

}

bool GICDistributorInterface::read(uint64_t off, uint8_t len, uint64_t& data)
{
	DEBUG << CONTEXT(GICDistributor) << "Register Read @ " << std::hex << off;
	
	switch (off) {
	case 0x00:
		data = ctrl;
		return true;
		
	case 0x04:
		data = 0x22;
		return true;
		
	case 0x100 ... 0x108:
		data = 0; //set_enable[(off & 0xf) >> 2];
		return true;
		
	case 0x180 ... 0x188:
		data = 0; //clear_enable[(off & 0xf) >> 2];
		return true;
		
	case 0x200 ... 0x208:
		data = 0; //set_pending[(off & 0xf) >> 2];
		return true;
		
	case 0x280 ... 0x288:
		data = 0; //clear_pending[(off & 0xf) >> 2];
		return true;
		
	case 0x400 ... 0x45c:
		data = 0; //prio[(off & 0x7f) >> 2];
		return true;
		
	case 0x800 ... 0x85c:
		data = cpu_targets[(off & 0x7f) >> 2];
		return true;

	case 0xc00 ... 0xc14:
		data = config[(off & 0x1f) >> 2];
		return true;
		
	case 0xfe0:
		data = 0x90;
		return true;
	case 0xfe4:
		data = 0x13;
		return true;
	case 0xfe8:
		data = 0x4;
		return true;
	case 0xfec:
		data = 0x0;
		return true;
	}
	
	ERROR << CONTEXT(GICDistributor) << "Unknown Register Read @ " << std::hex << off;
	return false;
}

void GICDistributorInterface::set_enabled(uint32_t base, uint8_t bits)
{
	for (int i = 0; i < 8; i++) {
		if (bits & (1 << i)) {
			GIC::gic_irq& irq = owner.get_gic_irq(base + i);
			
			irq.enabled = true;
			if (irq.raised && !irq.edge_triggered) {
				irq.pending = true;
			}
		}
	}
}

void GICDistributorInterface::clear_enabled(uint32_t base, uint8_t bits)
{
	for (int i = 0; i < 8; i++) {
		if (bits & (1 << i)) {
			owner.get_gic_irq(base + i).enabled = false;
		}
	}
}

void GICDistributorInterface::set_pending(uint32_t base, uint8_t bits)
{
	for (int i = 0; i < 8; i++) {
		if (bits & (1 << i)) {
			owner.get_gic_irq(base + i).pending = true;
		}
	}
}

void GICDistributorInterface::clear_pending(uint32_t base, uint8_t bits)
{
	for (int i = 0; i < 8; i++) {
		if (bits & (1 << i)) {
			owner.get_gic_irq(base + i).pending = false;
		}
	}
}

bool GICDistributorInterface::write(uint64_t off, uint8_t len, uint64_t data)
{
	DEBUG << CONTEXT(GICDistributor) << "Register Write @ " << std::hex << off << " = " << data;
	
	switch (off) {
	case 0x00:
		ctrl = data & 1;
		owner.update();
		return true;
		
	case 0x100 ... 0x108:
	{
		uint32_t base = (off - 0x100) * 8;
		for (int i = 0; i < len; i++, base += 8, data >>= 8) {
			set_enabled(base, (uint8_t)(data & 0xff));
		}
		
		owner.update();
		return true;
	}
	
	case 0x180 ... 0x188:
	{
		uint32_t base = (off - 0x180) * 8;
		for (int i = 0; i < len; i++, base += 8, data >>= 8) {
			clear_enabled(base, (uint8_t)(data & 0xff));
		}
		
		owner.update();
		return true;
	}
		
	case 0x200 ... 0x208:
	{
		uint32_t base = (off - 0x200) * 8;
		for (int i = 0; i < len; i++, base += 8, data >>= 8) {
			set_pending(base, (uint8_t)(data & 0xff));
		}
		
		owner.update();
		return true;
	}
		
	case 0x280 ... 0x288:
	{
		uint32_t base = (off - 0x280) * 8;
		for (int i = 0; i < len; i++, base += 8, data >>= 8) {
			clear_pending(base, (uint8_t)(data & 0xff));
		}
		
		owner.update();
		return true;
	}

	case 0x400 ... 0x45c:
	{
		uint32_t irqi = (off - 0x400);
		
		for (int i = 0; i < len; i++, data >>= 8) {
			owner.get_gic_irq(irqi + i).priority = data & 0xff;
		}
		
		owner.update();
		return true;
	}
	
	case 0x800 ... 0x85c:
		cpu_targets[(off & 0x7f) >> 2] = data;
		return true;
		
	case 0xc00 ... 0xc14:
#ifdef DEBUG_IRQ
		fprintf(stderr, "gic: config %x\n", data);
#endif
		config[(off & 0x1f) >> 2] = data;
		return true;
		
	case 0xf00:
		sgi(data);
		return true;
		
	case 0xf10 ... 0xf1c:
		return true;

	case 0xf20 ... 0xf2c:
		return true;
	}
	
	ERROR << CONTEXT(GICDistributor) << "Unknown Register Write @ " << std::hex << off << " = " << data;
	return false;
}

void GICDistributorInterface::sgi(uint32_t data)
{
	int cpu = hypervisor::Guest::current_core->id();
	int mask;
	uint32_t irq = data & 0x3ff;
	
	switch ((data >> 24) & 3) {
	case 0:
		mask = (data >> 16) & 0xf;
		break;
	case 1:
		mask = 0xf ^ (1 << cpu);
		break;
	case 2:
		mask = 1 << cpu;
		break;
	default:
		fprintf(stderr, "error: bad soft int target filter\n");
		return;
	}
	
	owner.get_gic_irq(irq).pending = true;
	owner.update();
}

GICCPUInterface::GICCPUInterface(GIC& owner, irq::IRQLine& irq, int id) 
	: owner(owner),
		irq(irq),
		id(id),
		ctrl(0),
		prio_mask(0),
		binpnt(3), 
		current_pending(1023),
		running_irq(1023),
		running_priority(0x100)
{
	for (int i = 0; i < 96; i++) {
		last_active[i] = 1023;
	}
}

GICCPUInterface::~GICCPUInterface()
{

}

bool GICCPUInterface::read(uint64_t off, uint8_t len, uint64_t& data)
{
	switch (off) {
	case 0x00:
		data = ctrl;
		return true;
		
	case 0x04:		// CPU Priority Mask
		data = prio_mask;
		return true;
		
	case 0x08:		// Binary Point
		data = binpnt;
		return true;
		
	case 0x0c:		// Interrupt Acknowledge
		data = acknowledge();		
		return true;
				
	case 0x14:		// Running Interrupt Priority
		data = running_priority;
		return true;
		
	case 0x18:		// Highest Pending Interrupt
		data = current_pending;
		return true;
	}
	
	fprintf(stderr, "gic: cpu: unknown register read %02lx\n", off);
	return false;
}

bool GICCPUInterface::write(uint64_t off, uint8_t len, uint64_t data)
{
	switch (off) {
	case 0x00:
		ctrl = data & 1;
		update();
		return true;
		
	case 0x04:
		prio_mask = data & 0xff;
		update();
		return true;
		
	case 0x08:
		binpnt = data & 0x7;
		update();
		return true;
		
	case 0x10:
		complete(data);
		return true;
	}
	
	fprintf(stderr, "gic: cpu: unknown register write %02lx\n", off);
	return false;
}

void GICCPUInterface::update()
{
	current_pending = 1023;
	if (!enabled() || !owner.distributor.enabled()) {
		irq.rescind();
		return;
	}
	
	uint32_t best_prio = 0x100;
	uint32_t best_irq = 1023;
	
	for (int irqi = 0; irqi < 96; irqi++) {
		GIC::gic_irq& irq = owner.get_gic_irq(irqi);
		
		if (irq.enabled && (irq.pending || (!irq.edge_triggered && irq.raised))) {
			if (irq.priority < best_prio) {
				best_prio = irq.priority;
				best_irq = irqi;
			}
		}
	}

	bool raise = false;
	if (best_prio < prio_mask) {
		current_pending = best_irq;
		if (best_prio < running_priority) {
			raise = true;
		}
	}
	
	if (raise) {
		irq.raise();
	} else {
		irq.rescind();
	}
}

uint32_t GICCPUInterface::acknowledge()
{
#ifdef DEBUG_IRQ
	fprintf(stderr, "gic: acknowledge %d\n", current_pending);
#endif
	
	this->irq.acknowledge();

	uint32_t irq = current_pending;
	if (irq == 1023 || owner.get_gic_irq(irq).priority >= running_priority) {
		return 1023;
	}
	
	last_active[irq] = running_irq;
	
	owner.get_gic_irq(irq).pending = false;
	running_irq = irq;
	if (irq == 1023) {
		running_priority = 0x100;
	} else {
		running_priority = owner.get_gic_irq(irq).priority;
	}
		
	update();
	return irq;
}

void GICCPUInterface::complete(uint32_t irq)
{
#ifdef DEBUG_IRQ
	fprintf(stderr, "gic: complete %d running=%d\n", irq, running_irq);
#endif
	
	if (irq >= 96) {
		return;
	}
	
	if (running_irq == 1023) {
		return;
	}
	
	if (irq != running_irq) {
		int tmp = running_irq;
		
		while (last_active[tmp] != 1023) {
			if (last_active[tmp] == irq) {
				last_active[tmp] = last_active[irq];
				break;
			}
			
			tmp = last_active[tmp];
		}
	} else {
		running_irq = last_active[running_irq];
		if (running_irq == 1023) {
			running_priority = 0x100;
		} else {
			running_priority = owner.get_gic_irq(running_irq).priority;
		}
	}
	
	update();
}

GIC::GIC() : distributor(GICDistributorInterface(*this))
{
	bzero(&irqs, sizeof(irqs));
	
	for (int i = 0; i < 96; i++) {
		irqs[i].index = i;
	}
}

GIC::~GIC()
{
	for (auto core : cores) {
		delete core;
	}
	
	cores.clear();
}

void GIC::add_core(irq::IRQLine& irq, int id)
{
	GICCPUInterface *iface = new GICCPUInterface(*this, irq, id);
	cores.push_back(iface);
}

void GIC::irq_raised(irq::IRQLine& line)
{
	gic_irq& irq = get_gic_irq(line.index());

	if (!irq.raised) {
#ifdef DEBUG_IRQ
		fprintf(stderr, "gic: raise %d\n", irq.index);
#endif
		
		irq.raised = true;
		if (irq.edge_triggered) irq.pending = true;
	
		update();
	}
}

void GIC::irq_rescinded(irq::IRQLine& line)
{
	gic_irq& irq = get_gic_irq(line.index());
	
	if (irq.raised) {
#ifdef DEBUG_IRQ
		fprintf(stderr, "gic: rescind %d\n", irq.index);
#endif

		irq.raised = false;
		update();
	}
}

void GIC::update()
{
	for (auto core : cores)
		core->update();
}
