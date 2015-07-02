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
	namespace devices {
		class Device;
	}

	namespace hypervisor {
		class GuestCPUConfiguration;

		namespace kvm {
			class KVMGuest;

			class KVMCpu : public CPU {
			public:
				KVMCpu(KVMGuest& owner, const GuestCPUConfiguration& config, int id, int fd, int irqfd, PerCPUData *per_cpu_data);
				~KVMCpu();

				bool init();
				bool run() override;
				void stop() override;
				void interrupt(uint32_t code) override;

				inline bool initialised() const { return _initialised; }
				inline int id() const { return _id; }

				inline int vmioctl(unsigned long int req) const {
					return vmioctl(req, (unsigned long int)0);
				}

				inline int vmioctl(unsigned long int req, unsigned long int arg) const {
					return ioctl(fd, req, arg);
				}

				inline int vmioctl(unsigned long int req, void *arg) const {
					return ioctl(fd, req, arg);
				}

			private:
				bool _initialised;
				int _id;
				int fd, irqfd;
				
				struct kvm_run *cpu_run_struct;
				uint32_t cpu_run_struct_size;

				bool setup_interrupts();

				bool handle_hypercall(uint64_t data, uint64_t arg1, uint64_t arg2);
				bool handle_device_access(devices::Device *device, uint64_t pa, struct kvm_run& rs);

				void dump_regs();
			};
		}
	}
}

#endif	/* CPU_H */

