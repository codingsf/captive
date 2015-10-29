#include <devices/arm/mptimer.h>
#include <devices/timers/tick-source.h>
#include <devices/irq/irq-line.h>

using namespace captive::devices::arm;

MPTimer::MPTimer(timers::TickSource& ts, irq::IRQLine& irq) : ts(ts), irq(irq), enabled(false), auto_reload(false), irq_enabled(false), prescale(0), load(0), val(0), isr(0), start(0)
{
	ts.add_sink(*this);
}

MPTimer::~MPTimer() {

}

bool MPTimer::read(uint64_t off, uint8_t len, uint64_t& data)
{
	switch (off) {
	case 0x04:
		data = val;
		return true;
		
	case 0x08:
		data = 0;
		data |= enabled << 0;
		data |= auto_reload << 1;
		data |= irq_enabled << 2;
		data |= prescale << 8;
		
		return true;
		
	case 0x0c:
		data = isr;
		return true;
	}
	
	fprintf(stderr, "mptimer: invalid register read @ %x\n", off);
	return false;
}

bool MPTimer::write(uint64_t off, uint8_t len, uint64_t data)
{
	switch (off) {
	case 0x00:					// LOAD
		load = data;
		val = data;
		return true;
		
	case 0x04:					// COUNTER
		val = data;
		return true;
		
	case 0x08:					// CONTROL
	{
		bool was_enabled = enabled;
		
		enabled = !!(data & 1);
		auto_reload = !!(data & 2);
		irq_enabled = !!(data & 4);
		prescale = (data >> 8) & 0xff;
		
		if (!was_enabled && enabled) {
			if (val == 0 && auto_reload) {
				val = load;
			}
		}
				
		return true;
	}
	
	case 0x0c:
		isr &= ~data;
		update_irq();
		return true;
	}
	
	fprintf(stderr, "mptimer: invalid register write @ %x = %x\n", off, data);
	return false;
}

void MPTimer::tick(uint32_t period)
{
	if (enabled) {
		if (val > 10000) {
			val -= 10000;
		} else {
			if (auto_reload) {
				val = load;
			} else {
				val = 0;
				enabled = false;
			}
			
			isr |= 1;
			update_irq();
		}
	}
}

void MPTimer::update_irq()
{
	if (irq_enabled && isr) {
		irq.raise();
	} else {
		irq.rescind();
	}
}
