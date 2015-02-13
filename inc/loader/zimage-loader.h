/*
 * File:   ZImageLoader.h
 * Author: spink
 *
 * Created on 09 February 2015, 10:31
 */

#ifndef ZIMAGELOADER_H
#define	ZIMAGELOADER_H

#define ZIMAGE_MAGIC_NUMBER	0x016F2818
#define ZIMAGE_BASE		(64 * 1024)

#include <define.h>
#include <loader/loader.h>

namespace captive {
	namespace loader {
		class ZImageLoader : public FileBasedLoader
		{
		public:
			ZImageLoader(std::string filename);

			virtual bool install(uint8_t* gpm) override;
			virtual gpa_t entrypoint() const;
			
		private:
			struct zimage_header {
				uint32_t code[9];
				uint32_t magic_number;
				uint32_t image_start;
				uint32_t image_end;
			};
		};
	}
}

#endif	/* ZIMAGELOADER_H */

