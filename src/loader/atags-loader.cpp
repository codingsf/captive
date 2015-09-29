#include <loader/atags-loader.h>

using namespace captive::loader;

ATAGsLoader::ATAGsLoader()
{
	
}

bool ATAGsLoader::install(uint8_t* gpm)
{
	uint32_t *base = (uint32_t *)(gpm + 0x1000);
	
	// CORE
	*base++ = 5;
	*base++ = 0x54410001;
	*base++ = 1;
	*base++ = 4096;
	*base++ = 0;
	
	// MEM
	*base++ = 4;
	*base++ = 0x54410002;
	*base++ = 0x10000000;
	*base++ = 0x00000000;
	
	// NONE
	*base++ = 0;
	*base++ = 0;
	
	return true;
}
