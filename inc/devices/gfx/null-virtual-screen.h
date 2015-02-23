/*
 * File:   null-virtual-screen.h
 * Author: spink
 *
 * Created on 23 February 2015, 08:19
 */

#ifndef NULL_VIRTUAL_SCREEN_H
#define	NULL_VIRTUAL_SCREEN_H

#include <devices/gfx/virtual-screen.h>

namespace captive {
	namespace devices {
		namespace gfx {
			class NullVirtualScreen : public VirtualScreen
			{
			public:
				NullVirtualScreen();
				virtual ~NullVirtualScreen();

				virtual bool initialise();
				virtual bool activate_configuration(const VirtualScreenConfiguration& cfg);
				virtual bool reset_configuration();
			};
		}
	}
}

#endif	/* NULL_VIRTUAL_SCREEN_H */

