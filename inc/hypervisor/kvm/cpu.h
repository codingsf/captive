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

#include <vector>
#include <chrono>

namespace captive {
	namespace devices {
		class Device;
	}

	namespace hypervisor {
		class GuestCPUConfiguration;

		namespace kvm {
			class KVMGuest;
			class KVMCpu;
			
			class IRQFD {
			public:
				IRQFD(KVMGuest& owner);
				~IRQFD();
				
				bool attach(int gsi);
				void raise();
				
			private:
				KVMGuest& owner;
				int fd, gsi;
			};
			
			class KVMCpu : public CPU {
			public:
				KVMCpu(int id, KVMGuest& owner, const GuestCPUConfiguration& config, int fd, PerCPUData *per_cpu_data);
				~KVMCpu();

				bool init();
				bool run() override;
				void stop() override;
				void interrupt(uint32_t code) override;
				
				void raise_guest_interrupt(uint8_t irq) override;
				void rescind_guest_interrupt(uint8_t irq) override;
				void acknowledge_guest_interrupt(uint8_t irq) override;

				inline bool initialised() const { return _initialised; }

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
				int fd;
				
				struct kvm_run *cpu_run_struct;
				uint32_t cpu_run_struct_size;
				
				IRQFD irq_signal;
				IRQFD irq_raise;
				
				std::chrono::high_resolution_clock::time_point cpu_start_time;
				
				bool setup_interrupts();

				bool handle_hypercall(uint64_t data, uint64_t arg1, uint64_t arg2);
				bool handle_device_access(devices::Device *device, uint64_t pa, struct kvm_run& rs);

				void dump_regs();
			};
		}
	}
}

#endif	/* CPU_H */

