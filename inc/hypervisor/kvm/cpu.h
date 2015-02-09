/*
 * File:   cpu.h
 * Author: spink
 *
 * Created on 29 January 2015, 08:38
 */

#ifndef KVM_CPU_H
#define	KVM_CPU_H

#include <define.h>
#include <hypervisor/cpu.h>
#include <sys/ioctl.h>
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

				inline bool initialised() const { return _initialised; }

				inline int id() const { return _id; }

			private:
				bool _initialised;
				int _id;
				int fd;
				struct kvm_run *cpu_run_struct;
				uint32_t cpu_run_struct_size;

				inline int vmioctl(unsigned long int req) const {
					return vmioctl(req, (unsigned long int)0);
				}

				inline int vmioctl(unsigned long int req, unsigned long int arg) const {
					return ioctl(fd, req, arg);
				}

				inline int vmioctl(unsigned long int req, void *arg) const {
					return ioctl(fd, req, arg);
				}

				bool handle_hypercall(uint64_t data);

				void dump_regs();
			};
		}
	}
}

#endif	/* CPU_H */

