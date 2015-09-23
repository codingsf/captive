#include <devices/arm/pl390.h>

using namespace captive::devices::arm;

PL390::PL390(irq::IRQLine& irq, irq::IRQLine& fiq) : Primecell(0), irq(irq), fiq(fiq)
{
	
}

PL390::~PL390()
{

}

void PL390::irq_raised(irq::IRQLine& line)
{
	fprintf(stderr, "IRQ RAISED\n");
}

void PL390::irq_rescinded(irq::IRQLine& line)
{
	fprintf(stderr, "IRQ RESCINDED\n");
}

bool PL390::read(uint64_t off, uint8_t len, uint64_t& data)
{
	fprintf(stderr, "READ\n");
	return false;
}

bool PL390::write(uint64_t off, uint8_t len, uint64_t data)
{
	fprintf(stderr, "WRITE\n");
	return false;
}
