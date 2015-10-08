/*
 * File:   devtree-loader.h
 * Author: spink
 *
 * Created on 13 February 2015, 10:35
 */

#ifndef DEVTREE_LOADER_H
#define	DEVTREE_LOADER_H

#include <loader/loader.h>

namespace captive {
	namespace loader {
		class DeviceTreeLoader : public FileBasedLoader
		{
		public:
			DeviceTreeLoader(std::string filename, uint32_t base_address);

			virtual bool install(uint8_t* gpm) override;
			uint32_t base_address() const override { return _base_address; }

		private:
			uint32_t _base_address;
		};
	}
}

#endif	/* DEVTREE_LOADER_H */
