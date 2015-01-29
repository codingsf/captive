/* 
 * File:   kvm.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 15:44
 */

#ifndef KVM_H
#define	KVM_H

#include <define.h>

#include <vector>

#include <captive.h>
#include <hypervisor/hypervisor.h>
#include <hypervisor/guest.h>

namespace captive {
	namespace hypervisor {
		namespace kvm {
			class KVM;
			
			class KVMGuest : public Guest {
			public:
				KVMGuest(KVM& owner, const GuestConfiguration& config, int fd);
				virtual ~KVMGuest();
				
				bool start(engine::Engine& engine) override;
				
			private:
				int fd;
			};

			class KVM : public Hypervisor {
				friend class KVMGuest;
				
			public:
				explicit KVM();
				virtual ~KVM();

				bool init() override;
				Guest *create_guest(const GuestConfiguration& config) override;
				
				int version() const;

				static bool supported();

			private:
				int kvm_fd;
				std::vector<Guest *> known_guests;
				
				inline bool initialised() const { return kvm_fd >= 0; }
				
				bool validate_configuration(const GuestConfiguration& config) const;
				
				KVM(const KVM&) = delete;
				KVM& operator=(const KVM&) = delete;
			};
		}
	}
}

#endif	/* KVM_H */

