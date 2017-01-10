/*
 * File:   elf-loader.h
 * Author: spink
 *
 * Created on 13 March 2015, 11:35
 */

#ifndef ELF_LOADER_H
#define	ELF_LOADER_H

#include <define.h>
#include <loader/loader.h>
#include <loader/kernel-loader.h>

namespace captive {
	namespace loader {
		class ELFLoader : public FileBasedLoader, public KernelLoader
		{
		public:
			ELFLoader(std::string filename);

			bool install(uint8_t* gpm) override;
			gpa_t entrypoint() const override;
			bool requires_device_tree() const override;
			
			gpa_t base_address() const override { return 0; }
			
			static bool match(const uint8_t *buffer);

		private:
			gpa_t _entrypoint;
		};
	}
}

#endif	/* ELF_LOADER_H */

