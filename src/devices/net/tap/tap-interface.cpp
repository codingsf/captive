#include <devices/net/tap/tap-interface.h>

using namespace captive::devices::net;
using namespace captive::devices::net::tap;

TapInterface::TapInterface(std::string tap_device)
{

}

TapInterface::~TapInterface()
{

}

bool TapInterface::start()
{
	return true;
}

void TapInterface::stop()
{

}

bool TapInterface::transmit_packet(const uint8_t* buffer, uint32_t length)
{
	fprintf(stderr, "************ TRANSMIT PACKET\n");

	for (int i = 0; i < length; i++) {
		fprintf(stderr, "%02x ", buffer[i]);
	}

	fprintf(stderr, "\n");

	return false;
}
