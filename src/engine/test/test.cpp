#include <engine/test/test.h>

using namespace captive::engine;
using namespace captive::engine::test;

uint8_t bootloader[4096] __attribute__((aligned(4096)));

TestEngine::TestEngine()
{
	// 48 c7 c0 34 12 00 00		mov    $0x1234,%rax
        // cc				int3

	bootloader[0] = 0xfa;
	bootloader[1] = 0xfc;
	bootloader[2] = 0xb8;

	/*bootloader[0] = 0x48;
	bootloader[1] = 0xc7;
	bootloader[2] = 0xc0;
	bootloader[3] = 0x34;
	bootloader[4] = 0x12;
	bootloader[5] = 0x00;
	bootloader[6] = 0x00;
	bootloader[7] = 0xcc;*/
}

TestEngine::~TestEngine()
{

}

void* TestEngine::get_bootloader_addr() const
{
	return (void *)bootloader;
}

uint64_t TestEngine::get_bootloader_size() const
{
	return sizeof(bootloader);
}
