#include <captive.h>
#include <loader/kernel-loader.h>
#include <loader/zimage-loader.h>
#include <loader/elf-loader.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

using namespace captive::loader;

USE_CONTEXT(Loader)
DECLARE_CHILD_CONTEXT(KernelLoader, Loader)

KernelLoader *KernelLoader::create_from_file(std::string filename)
{
	uint8_t buffer[128];
	
	int fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0) {
		ERROR << CONTEXT(KernelLoader) << "Unable to open file";
		return NULL;
	}
	
	if (read(fd, buffer, 128) <= 0) {
		ERROR << CONTEXT(KernelLoader) << "Unable to read file";
		close(fd);
		return NULL;
	}
	
	close(fd);
	
	if (ZImageLoader::match(buffer)) {
		return new ZImageLoader(filename);
	} else if (ELFLoader::match(buffer)) {
		return new ELFLoader(filename);
	} else {
		ERROR << CONTEXT(KernelLoader) << "Unable to detect file type";
		return NULL;
	}
}
