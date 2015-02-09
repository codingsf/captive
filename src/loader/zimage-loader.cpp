#include <captive.h>
#include <loader/zimage-loader.h>

using namespace captive::loader;

ZImageLoader::ZImageLoader(std::string filename) : FileBasedLoader(filename)
{

}

bool ZImageLoader::install(uint8_t* gpm)
{
	if (!open()) {
		ERROR << "Unable to open file";
		return false;
	}

	const zimage_header *header = (const zimage_header *)read(0, size());

	if (header->magic_number != ZIMAGE_MAGIC_NUMBER) {
		ERROR << "ZImage Header has invalid magic number";
		return false;
	}

	void *gpm_base = &gpm[ZIMAGE_BASE];
	memcpy(gpm_base, (const void *)header, size());

	return true;
}

gpa_t ZImageLoader::entrypoint() const
{
	return (gpa_t)ZIMAGE_BASE;
}
