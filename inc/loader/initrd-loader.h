/* 
 * File:   initrd-loader.h
 * Author: s0457958
 *
 * Created on 05 October 2015, 11:33
 */

#ifndef INITRD_LOADER_H
#define INITRD_LOADER_H

#include <loader/loader.h>

namespace captive {
	namespace loader {
		class InitRDLoader : public FileBasedLoader
		{
		public:
			InitRDLoader(std::string filename, uint32_t base_address);
			
			bool install(uint8_t* gpm) override;
			gpa_t base_address() const override { return _base_address; }
			
		private:
			gpa_t _base_address;
		};
	}
}

#endif /* INITRD_LOADER_H */

