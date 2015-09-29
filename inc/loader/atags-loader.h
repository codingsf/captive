/* 
 * File:   atags-loader.h
 * Author: s0457958
 *
 * Created on 29 September 2015, 12:41
 */

#ifndef ATAGS_LOADER_H
#define ATAGS_LOADER_H


#include <loader/loader.h>

namespace captive {
	namespace loader {
		class ATAGsLoader : public Loader
		{
		public:
			ATAGsLoader();

			bool install(uint8_t* gpm) override;
		};
	}
}

#endif /* ATAGS_LOADER_H */

