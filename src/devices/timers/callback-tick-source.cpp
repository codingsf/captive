#include <devices/timers/callback-tick-source.h>

using namespace captive::devices::timers;

CallbackTickSource::CallbackTickSource(uint32_t scale) : _scale(scale), _current_scale(0)
{

}

CallbackTickSource::~CallbackTickSource()
{

}
