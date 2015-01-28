/* 
 * File:   guest.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 14:52
 */

#ifndef GUEST_H
#define	GUEST_H

namespace captive {
	namespace hypervisor {
		class Hypervisor;
		
		class Guest
		{
		public:
			Guest(Hypervisor& owner);
			virtual ~Guest();
			virtual void start() = 0;
			
			inline Hypervisor& owner() const { return _owner; }
			
		private:
			Hypervisor& _owner;
		};
	}
}

#endif	/* GUEST_H */

