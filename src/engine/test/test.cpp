#include <engine/test/test.h>

using namespace captive::engine;
using namespace captive::engine::test;

uint8_t bootloader[4096] __attribute__((aligned(4096)));

TestEngine::TestEngine()
{
	bootloader[0] = 0xcc;
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
