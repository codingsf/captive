/* 
 * File:   soft.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 14:47
 */

#ifndef SOFT_H
#define	SOFT_H

#include <hypervisor/hypervisor.h>

namespace captive {
	namespace hypervisor {
		namespace soft {
			class Soft;
			
			class SoftGuest : public Guest
			{
			public:
				SoftGuest(Hypervisor& owner, GuestConfiguration& config);
				virtual ~SoftGuest();
				virtual bool start() override;
			};
			
			class Soft : public Hypervisor {
			public:
				Soft();
				virtual ~Soft();

				Guest *create_guest() override;
			};
		}
	}
}

#endif	/* SOFT_H */

