#include <devices/arm/gic.h>
#include <captive.h>

DECLARE_CONTEXT(GIC);
DECLARE_CHILD_CONTEXT(GICDistributor, GIC);
DECLARE_CHILD_CONTEXT(GICCPU, GIC);

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
		data = set_enable[(off & 0xf) >> 2];
		return true;
		
	case 0x180 ... 0x188:
		data = clear_enable[(off & 0xf) >> 2];
		return true;
		
	case 0x200 ... 0x208:
		data = set_pending[(off & 0xf) >> 2];
		return true;
		
	case 0x280 ... 0x288:
		data = clear_pending[(off & 0xf) >> 2];
		return true;
		
	case 0x400 ... 0x45c:
		data = prio[(off & 0x7f) >> 2];
		return true;
		
	case 0x800 ... 0x85c:
		data = cpu_targets[(off & 0x7f) >> 2];
		return true;

	case 0xc00 ... 0xc14:
		data = config[(off & 0x1f) >> 2];
		return true;
	}
	
	fprintf(stderr, "gic: distributor: unknown register read %02x\n", off);
	return false;
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
		set_enable[(off & 0xf) >> 2] = data;
		return true;
		
	case 0x180 ... 0x188:
		clear_enable[(off & 0xf) >> 2] = data;
		return true;
		
	case 0x200 ... 0x208:
		set_pending[(off & 0xf) >> 2] = data;
		return true;
		
	case 0x280 ... 0x288:
		clear_pending[(off & 0xf) >> 2] = data;
		return true;

	case 0x400 ... 0x45c:
		prio[(off & 0x7f) >> 2] = data;
		return true;

	case 0x800 ... 0x85c:
		cpu_targets[(off & 0x7f) >> 2] = data;
		return true;
		
	case 0xc00 ... 0xc14:
		config[(off & 0x1f) >> 2] = data;
		return true;
		
	case 0xf00:
		fprintf(stderr, "gic: write to icdsgir %x\n", data);
		return true;
	}
	
	fprintf(stderr, "gic: distributor: unknown register write %02x\n", off);
	return false;
}

void GICDistributorInterface::post_interrupts()
{
	owner.cpu[0].pending.clear();
	
	for (auto raised : owner.raised) {
		owner.cpu[0].pending.insert(raised);
	}
}

GICCPUInterface::GICCPUInterface(GIC& owner, irq::IRQLine& irq) : owner(owner), irq(irq), ctrl(0), prio_mask(0), binpnt(3), current_pending(1023)
{

}

GICCPUInterface::~GICCPUInterface()
{

}

bool GICCPUInterface::read(uint64_t off, uint8_t len, uint64_t& data)
{
	switch (off) {
	case 0x00:
		data = ctrl & 1;
		return true;
		
	case 0x04:		// CPU Priority Mask
		data = prio_mask & 0xf0;
		return true;
		
	case 0x08:		// Binary Point
		data = binpnt & 0x7;
		return true;
		
	case 0x0c:		// Interrupt Acknowledge
		data = acknowledge();		
		return true;
				
	case 0x14:		// Running Interrupt
		fprintf(stderr, "gic: run\n");
		return true;
		
	case 0x18:		// Highest Pending Interrupt
		fprintf(stderr, "gic: pend\n");
		return true;
	}
	
	fprintf(stderr, "gic: cpu: unknown register read %02x\n", off);
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
		prio_mask = data & 0xf0;
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
	
	fprintf(stderr, "gic: cpu: unknown register write %02x\n", off);
	return false;
}

void GICCPUInterface::update()
{
	current_pending = 1023;

	if (!owner.distributor.enabled() || !(ctrl & 1)) {
		irq.rescind();		
		return;
	}
	
	bool raise = false;
	
	if (pending.size() > 0) {
		current_pending = *pending.begin();
		raise = true;
	}
	
	if (raise) {
		//fprintf(stderr, "*** raising for %d\n", current_pending);
		irq.raise();
	} else {
		irq.rescind();
	}
}

uint32_t GICCPUInterface::acknowledge()
{
	if (current_pending == 1023) return current_pending;
	
	current_running = current_pending;
	update();
	
	return current_running;
}

void GICCPUInterface::complete(uint32_t irq)
{
	current_running = 1023;
	pending.erase(irq);
}

GIC::GIC(irq::IRQLine& irq0, irq::IRQLine& irq1) : 
		distributor(GICDistributorInterface(*this)),
		cpu { GICCPUInterface(*this, irq0), GICCPUInterface(*this, irq1) }
{
}

GIC::~GIC()
{
}

void GIC::irq_raised(irq::IRQLine& line)
{
	raised.insert(line.index());
	update();
}

void GIC::irq_rescinded(irq::IRQLine& line)
{
	raised.erase(line.index());
	update();
}

void GIC::update()
{
	distributor.post_interrupts();
	cpu[0].update();
	cpu[1].update();
}
