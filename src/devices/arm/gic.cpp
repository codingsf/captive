#include <devices/arm/gic.h>

using namespace captive::devices::arm;

GIC::GIC(irq::IRQLine& irq) : irq(irq), current_pending(1023), cpu { 0, 0, 3 }, distrib { 0, 2 }
{
	for (int i = 0; i < 24; i++) {
		distrib.cpu_targets[i] = 0;
	}
}

GIC::~GIC() {

}

bool GIC::read(uint64_t off, uint8_t len, uint64_t& data)
{
	switch (off) {
	case 0x000:		// CPU Control Register
		data = cpu.ctrl & 1;
		return true;
		
	case 0x004:		// CPU Priority Mask
		data = cpu.prio_mask & 0xf0;
		return true;
		
	case 0x008:		// Binary Point
		data = cpu.binpnt & 0x7;
		return true;
		
	case 0x00c:		// Interrupt Acknowledge
		data = acknowledge();		
		return true;
				
	case 0x014:		// Running Interrupt
		fprintf(stderr, "gic: run\n");
		return true;
		
	case 0x018:		// Highest Pending Interrupt
		fprintf(stderr, "gic: pend\n");
		return true;
		
	case 0x1000:	// Distributor Control
		data = distrib.ctrl & 1;
		return true;

	case 0x1004:	// Controller Type
		data = distrib.type;
		return true;
		
	case 0x1100 ... 0x1108:
		data = distrib.set_enable[(off & 0xf) >> 2];
		return true;
		
	case 0x1180 ... 0x1188:
		data = distrib.clear_enable[(off & 0xf) >> 2];
		return true;
		
	case 0x1200 ... 0x1208:
		data = distrib.set_pending[(off & 0xf) >> 2];
		return true;
		
	case 0x1280 ... 0x1288:
		data = distrib.clear_pending[(off & 0xf) >> 2];
		return true;
		
	case 0x1400 ... 0x145c:
		data = distrib.prio[(off & 0x7f) >> 2];
		return true;
		
	case 0x1800 ... 0x185c:
		data = distrib.cpu_targets[(off & 0x7f) >> 2];
		return true;

	case 0x1c00 ... 0x1c14:
		data = distrib.config[(off & 0x1f) >> 2];
		return true;
	}
	
	fprintf(stderr, "gic: unhandled register read: %x\n", off);
	return false;
}

bool GIC::write(uint64_t off, uint8_t len, uint64_t data)
{
	switch (off) {
	case 0x000:
		cpu.ctrl = data & 1;
		update();
		return true;
		
	case 0x004:
		cpu.prio_mask = data & 0xf0;
		return true;
		
	case 0x008:
		cpu.binpnt = data & 0x7;
		return true;
		
	case 0x010:
		complete(data);
		return true;
		
	case 0x1000:
		distrib.ctrl = data & 1;
		update();
		return true;

	case 0x1004:
		return true;
		
	case 0x1100 ... 0x1108:
		distrib.set_enable[(off & 0xf) >> 2] = data;
		return true;
		
	case 0x1180 ... 0x1188:
		distrib.clear_enable[(off & 0xf) >> 2] = data;
		return true;
		
	case 0x1200 ... 0x1208:
		distrib.set_pending[(off & 0xf) >> 2] = data;
		return true;
		
	case 0x1280 ... 0x1288:
		distrib.clear_pending[(off & 0xf) >> 2] = data;
		return true;

	case 0x1400 ... 0x145c:
		distrib.prio[(off & 0x7f) >> 2] = data;
		return true;

	case 0x1800 ... 0x185c:
		distrib.cpu_targets[(off & 0x7f) >> 2] = data;
		return true;
		
	case 0x1c00 ... 0x1c14:
		distrib.config[(off & 0x1f) >> 2] = data;
		return true;
	}
	
	fprintf(stderr, "gic: unhandled register write %x = %08x\n", off, data);
	return false;
}

void GIC::irq_raised(irq::IRQLine& line)
{
	std::unique_lock<std::mutex> l(lock);
	
	if (!asserted.count(line.index())) {
		asserted.insert(line.index());	
		pending.insert(line.index());
		update();
	}
}

void GIC::irq_rescinded(irq::IRQLine& line)
{
	std::unique_lock<std::mutex> l(lock);
	
	if (asserted.count(line.index())) {
		asserted.erase(line.index());
		update();
	}
}

void GIC::update()
{
	current_pending = 1023;

	if (!(distrib.ctrl & 1) || !(cpu.ctrl & 1)) {
		irq.rescind();		
		return;
	}
	
	bool raise = false;
	
	if (pending.size() > 0) {
		current_pending = *pending.begin();
		raise = true;
	}
	
	if (raise) {
		irq.raise();
	} else {
		irq.rescind();
	}
}

uint32_t GIC::acknowledge()
{
	std::unique_lock<std::mutex> l(lock);
	if (current_pending == 1023) return current_pending;
	
	current_running = current_pending;
	update();
	
	return current_running;
}

void GIC::complete(uint32_t irq)
{
	std::unique_lock<std::mutex> l(lock);
	
	current_running = 1023;
	pending.erase(irq);
}
