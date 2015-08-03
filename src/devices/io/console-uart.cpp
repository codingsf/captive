#include <devices/io/console-uart.h>

#include <termios.h>
#include <unistd.h>

using namespace captive::devices::io;

ConsoleUART::ConsoleUART()
{
	
}

ConsoleUART::~ConsoleUART() {

}

bool ConsoleUART::open()
{
	struct termios settings;
	tcgetattr(fileno(stdout), &settings);
	//orig_settings = settings;
	
	cfmakeraw(&settings);
	settings.c_cc[VTIME] = 0;
	settings.c_cc[VMIN] = 1;

	tcsetattr(0, TCSANOW, &settings);
	
	return true;
}

bool ConsoleUART::close()
{
	return true;
}

bool ConsoleUART::read_char(uint8_t& ch)
{
	ch = fgetc(stdin);
	return true;
}

bool ConsoleUART::write_char(uint8_t ch)
{
	fprintf(stdout, "%c", ch);
	fflush(stdout);
	
	return true;
}
