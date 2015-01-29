/*
 * File:   test.h
 * Author: spink
 *
 * Created on 29 January 2015, 08:11
 */

#ifndef TEST_H
#define	TEST_H

#include <define.h>
#include <engine/engine.h>

namespace captive {
	namespace engine {
		namespace test {
			class TestEngine : public Engine
			{
			public:
				explicit TestEngine();
				virtual ~TestEngine();

				void* get_bootloader_addr() const override;
				uint64_t get_bootloader_size() const override;
			};
		}
	}
}

#endif	/* TEST_H */

