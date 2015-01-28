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
		
		class GuestConfiguration
		{
		public:
			std::string name;
		};
		
		class Guest
		{
		public:
			Guest(Hypervisor& owner, const GuestConfiguration& config);
			virtual ~Guest();
			virtual bool start() = 0;
			
			inline Hypervisor& owner() const { return _owner; }
			inline const GuestConfiguration& config() const { return _config; }
			
		private:
			Hypervisor& _owner;
			const GuestConfiguration& _config;
		};
	}
}

#endif	/* GUEST_H */

