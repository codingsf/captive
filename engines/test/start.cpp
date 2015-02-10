/*
 * start.cpp
 */

void printf(const char *fmt, ...);

extern "C" void engine_start(unsigned int entry_point)
{
	printf("Welcome to the engine: %s entry point=%x\n", "tom", 0xbabe1234);
	for(;;);
}
