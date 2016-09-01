#include <util/cl/parsing-context.h>

using namespace captive::util::cl;

bool ParsingContext::value(std::string& v)
{
	if (peek_token().compare(0, 2, "--") == 0)
		return false;
	
	v = consume_token();
	return true;
}

bool ParsingContext::value(maybe<std::string>& v)
{
	if (peek_token().compare(0, 2, "--") == 0) {
		v = maybe<std::string>();
		return false;
	}
	
	v = maybe<std::string>(consume_token());
	return true;
}
