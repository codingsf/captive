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
		class InitRDLoader;
		
		class ATAGsLoader : public Loader
		{
		public:
			ATAGsLoader();
			ATAGsLoader(InitRDLoader& initrd);

			bool install(uint8_t* gpm) override;
			uint32_t base_address() const override { return 0x100; }
			
		private:
			InitRDLoader *_initrd;
		};
	}
}

#endif /* ATAGS_LOADER_H */

