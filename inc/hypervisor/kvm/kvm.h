/* 
 * File:   kvm.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 15:44
 */

#ifndef KVM_H
#define	KVM_H

#include <hypervisor/hypervisor.h>
#include <hypervisor/guest.h>

namespace captive {
	namespace hypervisor {
		namespace kvm {
			class KVM;
			
			class KVMGuest : public Guest {
			public:
				KVMGuest(Hypervisor& owner);
				virtual ~KVMGuest();
				
				void start() override;
			};

			class KVM : public Hypervisor {
			public:
				explicit KVM();
				virtual ~KVM();

				Guest *create_guest() override;

				static bool supported();

			private:
				int kvm_fd;
			};
		}
	}
}

#endif	/* KVM_H */

