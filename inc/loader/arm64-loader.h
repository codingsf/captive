/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   arm64-loader.h
 * Author: s0457958
 *
 * Created on 13 September 2016, 11:43
 */

#ifndef ARM64_LOADER_H
#define ARM64_LOADER_H


#include <define.h>
#include <loader/loader.h>
#include <loader/kernel-loader.h>

namespace captive {
	namespace loader {
		class ARM64Loader : public FileBasedLoader, public KernelLoader
		{
		public:
			ARM64Loader(std::string filename);

			bool install(uint8_t* gpm) override;
			gpa_t entrypoint() const override { return _entrypoint; }
			gpa_t base_address() const override { return _base_address; }

			bool requires_device_tree() const override;
			
			static bool match(const uint8_t *buffer);			
			
		private:
			uint64_t _base_address, _entrypoint;
			
#define ARM64_MAGIC 0x644d5241
			struct arm64_header {
				uint32_t code0;
				uint32_t code1;
				uint64_t text_offset;
				uint64_t image_size;
				uint64_t flags;
				uint64_t res2;
				uint64_t res3;
				uint64_t res4;
				uint32_t magic;
				uint32_t res5;

			};
		};
	}
}

#endif /* ARM64_LOADER_H */

