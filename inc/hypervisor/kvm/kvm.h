/* 
 * File:   kvm.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 15:44
 */

#ifndef KVM_H
#define	KVM_H

#include <hypervisor/hypervisor.h>

namespace captive
{
	namespace hypervisor
	{
		namespace kvm
		{
			class KVM : public Hypervisor
			{
			public:
				KVM();
				virtual ~KVM();
				
				void run_guest(Guest& guest) override;
				
				static bool supported();
				
			private:
				int kvm_fd;
			};
		}
	}
}

#endif	/* KVM_H */

