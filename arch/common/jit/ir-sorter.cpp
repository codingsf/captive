#include <jit/ir-sorter.h>

using namespace captive::arch::jit;
using namespace captive::arch::jit::algo;

bool GnomeSort::sort()
{
	uint32_t pos = 1;

	while (pos < ctx.count()) {
		if (key(pos) >= key(pos - 1)) {
			pos += 1;
		} else {
			exchange(pos, pos - 1);

			if (pos > 1) {
				pos--;
			}
		}
	}	
	
	return true;
}
