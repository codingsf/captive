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

Engine::Engine(std::string libfile) : loaded(false), libfile(libfile), _entrypoint_offset(0)
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

	// First, load the program.
	for (int i = 0; i < hdr->e_phnum; i++) {
		Elf64_Phdr *phdr = (Elf64_Phdr *)(lib + hdr->e_phoff + (i * hdr->e_phentsize));

		DEBUG << "Program Header: type=" << phdr->p_type << ", flags=" << phdr->p_flags << ", file offset=" << phdr->p_offset << ", file size=" << phdr->p_filesz << ", paddr=" << std::hex << phdr->p_paddr << ", vaddr=" << phdr->p_vaddr;

		if (phdr->p_type == PT_LOAD) {
			DEBUG << "Loading";
			memcpy(base + phdr->p_vaddr, lib + phdr->p_offset, phdr->p_filesz);
		}
	}

	// Try and find the symbol table and string table
	Elf64_Sym *symtab;
	char *strtab;
	uint32_t symtab_size, strtab_size = 0;
	for (int i = 0; i < hdr->e_shnum; i++) {
		Elf64_Shdr *shdr = (Elf64_Shdr *)(lib + hdr->e_shoff + (i * hdr->e_shentsize));

		DEBUG << "Program Section: type=" << shdr->sh_type;

		if (shdr->sh_type == SHT_SYMTAB) {
			DEBUG << "Symbol Table";
			symtab = (Elf64_Sym *)(lib + shdr->sh_offset);
			symtab_size = shdr->sh_size;
		} else if (shdr->sh_type == SHT_STRTAB) {
			DEBUG << "String Table";
			strtab = (char *)(lib + shdr->sh_offset);
			strtab_size = shdr->sh_size;
		}
	}

	// Try and find the entry-point symbol
	for (int i = 0; i < symtab_size / sizeof(Elf64_Sym); i++) {
		if (ELF64_ST_TYPE(symtab[i].st_info) == STT_FUNC) {
			DEBUG << "fn symbol: " << &strtab[symtab[i].st_name];
			if (strcmp(&strtab[symtab[i].st_name], "engine_start") == 0) {
				_entrypoint_offset = symtab[i].st_value;
				DEBUG << "Entry Point Offset: " << std::hex << _entrypoint_offset;
			}
		}
	}

	// Fill in relocations.

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
	lib = (uint8_t *)mmap(NULL, lib_size, PROT_READ, MAP_PRIVATE, fd, 0);
	::close(fd);

	if (lib == MAP_FAILED) {
		ERROR << "Unable to map libfile";
		return false;
	}

	loaded = true;
	return true;
}
