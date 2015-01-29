#include <engine/test/test.h>

using namespace captive::engine;
using namespace captive::engine::test;

TestEngine::TestEngine()
{

}

TestEngine::~TestEngine()
{

}

void* TestEngine::get_bootloader_addr() const
{
	return NULL;
}

uint64_t TestEngine::get_bootloader_size() const
{
	return 0;
}
