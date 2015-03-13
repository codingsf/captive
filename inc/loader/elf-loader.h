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

namespace captive {
	namespace loader {
		class ELFLoader : public FileBasedLoader
		{
		public:
			ELFLoader(std::string filename);

			virtual bool install(uint8_t* gpm) override;
			virtual gpa_t entrypoint() const;

		private:
			gpa_t _entrypoint;
		};
	}
}

#endif	/* ELF_LOADER_H */

