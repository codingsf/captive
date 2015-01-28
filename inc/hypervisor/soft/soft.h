/* 
 * File:   soft.h
 * Author: s0457958
 *
 * Created on 28 January 2015, 14:47
 */

#ifndef SOFT_H
#define	SOFT_H

#include <hypervisor/hypervisor.h>

namespace captive
{
	namespace hypervisor
	{
		namespace soft
		{
			class Soft : public Hypervisor
			{
			public:
				Soft();
				virtual ~Soft();
				
				void run_guest(Guest& guest) override;
			};
		}
	}
}

#endif	/* SOFT_H */

