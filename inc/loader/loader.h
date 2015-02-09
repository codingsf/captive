/*
 * File:   loader.h
 * Author: spink
 *
 * Created on 09 February 2015, 10:30
 */

#ifndef LOADER_H
#define	LOADER_H

#include <define.h>

namespace captive {
	namespace loader {
		class Loader
		{
		public:
			virtual bool install(uint8_t *gpm) = 0;
			virtual gpa_t entrypoint() const = 0;
		};

		class FileBasedLoader : public Loader
		{
		public:
			FileBasedLoader(std::string filename);

			inline const std::string& filename() const { return _filename; }
			inline bool is_open() const { return _opened; }
			inline size_t size() const { return mmap_size; }

		protected:
			bool open();
			void close();
			uint8_t *read(off_t offset, size_t size);

		private:
			std::string _filename;
			bool _opened;

			void *mmap_base;
			size_t mmap_size;
		};
	}
}

#endif	/* LOADER_H */

