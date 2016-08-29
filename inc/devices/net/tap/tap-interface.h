#ifndef TAP_INTERFACE_H
#define TAP_INTERFACE_H

#include <devices/net/network-interface.h>

namespace captive {
	namespace devices {
		namespace net {
			namespace tap {

				class TapInterface : public NetworkInterface {
				public:
					TapInterface(std::string tap_device);
					virtual ~TapInterface();

					bool start() override;
					void stop() override;

				private:
					bool transmit_packet(const uint8_t* buffer, uint32_t length) override;
				};
			}
		}
	}
}

#endif /* TAP_INTERFACE_H */

