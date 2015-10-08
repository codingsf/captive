/* 
 * File:   sysctrl.h
 * Author: s0457958
 *
 * Created on 29 September 2015, 16:55
 */

#ifndef SYSCTRL_H
#define SYSCTRL_H

#include <devices/arm/primecell.h>
#include <chrono>

namespace captive {
	namespace devices {
		namespace timers {
			class TickSource;
		}
		
		namespace arm {
			namespace realview {
				class SystemController : public Primecell
				{
				public:
					enum ControllerIndex
					{
						SYS_CTRL0,
						SYS_CTRL1
					};
					
					SystemController(ControllerIndex index);

					std::string name() const override { return "system-controller"; }
					
					bool read(uint64_t off, uint8_t len, uint64_t& data) override;
					bool write(uint64_t off, uint8_t len, uint64_t data) override;
					
				private:
					ControllerIndex index;
					uint32_t cr;
				};
			}
		}
	}
}

#endif /* SYSCTRL_H */
