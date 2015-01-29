/* 
 * File:   cpu.h
 * Author: spink
 *
 * Created on 29 January 2015, 08:38
 */

#ifndef KVM_CPU_H
#define	KVM_CPU_H

#include <hypervisor/cpu.h>

namespace captive {
	namespace hypervisor {
		class GuestCPUConfiguration;

		namespace kvm {
			class KVMGuest;

			class KVMCpu : public CPU {
			public:
				KVMCpu(KVMGuest& owner, const GuestCPUConfiguration& config);
				~KVMCpu();

				bool init();
				void run() override;
			};
		}
	}
}

#endif	/* CPU_H */

