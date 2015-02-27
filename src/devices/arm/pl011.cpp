#include <captive.h>
#include <devices/arm/pl011.h>

using namespace captive::devices::arm;

#define UARTDR		0x000
#define UARTRSR		0x004
#define UARTECR		0x004
#define UARTFR		0x018
#define UARTILPR	0x020
#define UARTIBRD	0x024
#define UARTFBRD	0x028

PL011::PL011() : Primecell(0x00141011)
{

}

PL011::~PL011()
{

}

bool PL011::read(uint64_t off, uint8_t len, uint64_t& data)
{
	if (Primecell::read(off, len, data))
		return true;

	switch (off) {
	case 0x000: // Data register
		if (fifo.empty()) {
			data = 0;
		} else {
			data = fifo.front();
			fifo.pop_front();
			irq_status &= ~(IRQ_RXINTR);
			// CheckIRQs();
		}
		break;
	case 0x004: // RX status register
		data = 0;
		break;
	case 0x018: // Flag register
		data = 0;
		if (fifo.empty()) {
			data |= (1 << 4); //RXFE
		}
		data |= (1 << 7); //TXFE (always)
		break;
	case 0x024: //integer baud rate register
		data = baud_rate;
		break;
	case 0x028: //fractional baud rate register
		data = fractional_baud;
		break;
	case 0x02c: //line control register
		data = line_control;
		break;
	case 0x030: // Control register
		data = control_word;
		break;
	case 0x34:
		data = 0;
		break;
	case 0x038:
		data = irq_mask;
		break;
	case 0x03C:
		data = irq_status;
		break;
	case 0x040:
		data = irq_status & irq_mask;
		break;
	default:
		return false;
	}

	return true;
}

bool PL011::write(uint64_t off, uint8_t len, uint64_t data)
{
	if (Primecell::write(off, len, data))
		return true;

	switch (off) {
	case 0x000: // Data register
		/*if (serial) {
			serial->WriteChar(data & 0xff);
		}*/
		fprintf(stdout, "%c", (uint8_t)(data & 0xff));
		fflush(stdout);
		//RaiseTxIRQ();
		break;
	case 0x004: // RX status register / clear error register
		control_word = data;
		break;
	case 0x018: // Flag register
		break;
	case 0x024: // Integer baud rate register
		baud_rate = data & 0xffff;
		break;
	case 0x028:
		fractional_baud = data & 0x3f;
		break;
	case 0x02C:
		line_control = data & 0xff;
		break;
	case 0x030: // Control register
		control_word = data;
		break;

	case 0x034:
		break;

	case 0x38:
		irq_mask = data & 0x7ff;
		//CheckIRQs();
		break;
	case 0x044:
		irq_status = irq_status & ~data;
		//CheckIRQs();
		break;
	case 0x048:
		WARNING << "Unimplemented DMA request";
		return false;
	default:
		return false;
	}

	return true;
}
