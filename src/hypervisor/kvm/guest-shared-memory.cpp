#include <hypervisor/kvm/guest.h>
#include <util/spin-lock.h>
#include <captive.h>

DECLARE_CONTEXT(SharedMemoryAllocator);

extern "C" void *dlmalloc(size_t s);
extern "C" void *dlrealloc(void *p, size_t s);
extern "C" void *dlfree(void *p);

using namespace captive::hypervisor::kvm;

static void *gsmbrk_base, *gsmbrk;

KVMGuest::KVMSharedMemory::KVMSharedMemory() : _arena(NULL), _arena_size(0)
{

}

void KVMGuest::KVMSharedMemory::set_arena(void* arena, size_t arena_size)
{
	_arena = arena;
	gsmbrk_base = gsmbrk = arena;

	_arena_size = arena_size;
}

void *KVMGuest::KVMSharedMemory::allocate(size_t size)
{
	return dlmalloc(size);
}

void *KVMGuest::KVMSharedMemory::reallocate(void *p, size_t size)
{
	return dlrealloc(p, size);
}

void KVMGuest::KVMSharedMemory::free(void* p)
{
	dlfree(p);
}

extern "C" void *gsmsbrk(intptr_t incr)
{
	if (incr == 0)
		return gsmbrk_base;

	gsmbrk = (void *)((intptr_t)gsmbrk + incr);
	return gsmbrk;
}
