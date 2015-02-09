#include <captive.h>
#include <engine/engine.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <elf.h>

using namespace captive;
using namespace captive::engine;

Engine::Engine(std::string libfile) : loaded(false), libfile(libfile)
{

}

Engine::~Engine()
{
	if (loaded) {
		munmap(lib, lib_size);
		lib = NULL;
	}
}

bool Engine::init()
{
	if (!load()) {
		ERROR << "Unable to load libfile";
		return false;
	}

	return true;
}

bool Engine::install(uint8_t* base)
{
	if (!loaded) {
		ERROR << "Engine has not been initialised";
		return false;
	}

	Elf64_Ehdr *hdr = (Elf64_Ehdr *)lib;

	if (hdr->e_ident[0] != 0x7f || hdr->e_ident[1] != 0x45 || hdr->e_ident[2] != 0x4c || hdr->e_ident[3] != 0x46) {
		ERROR << "Invalid ELF header";
		return false;
	}

	return true;
}

bool Engine::load()
{
	struct stat st;

	int fd = ::open(libfile.c_str(), O_RDONLY);
	if (fd < 0) {
		ERROR << "Unable to open libfile";
		return false;
	}

	if (fstat(fd, &st)) {
		::close(fd);

		ERROR << "Unable to stat libfile";
		return false;
	}

	lib_size = st.st_size;
	lib = mmap(NULL, lib_size, PROT_READ, MAP_PRIVATE, fd, 0);
	::close(fd);

	if (lib == MAP_FAILED) {
		ERROR << "Unable to map libfile";
		return false;
	}

	loaded = true;
	return true;
}
