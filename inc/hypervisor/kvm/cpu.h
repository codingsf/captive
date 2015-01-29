/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 29 January 2015, 08:38
 */

#ifndef KVM_CPU_H
#define	KVM_CPU_H

#include <hypervisor/cpu.h>
#include <linux/kvm.h>

namespace captive {
	namespace hypervisor {
		class GuestCPUConfiguration;

		namespace kvm {
			class KVMGuest;

			class KVMCpu : public CPU {
			public:
				KVMCpu(KVMGuest& owner, const GuestCPUConfiguration& config, int id, int fd);
				~KVMCpu();

				bool init();
				bool run() override;

				inline int id() const { return _id; }

			private:
				int _id;
				int fd;
				struct kvm_run *cpu_run_struct;
			};
		}
	}
}

#endif	/* CPU_H */

