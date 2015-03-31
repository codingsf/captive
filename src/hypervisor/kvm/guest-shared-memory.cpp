#include <hypervisor/kvm/guest.h>
#include <util/spin-lock.h>
#include <captive.h>

DECLARE_CONTEXT(SharedMemoryAllocator);

using namespace captive::hypervisor::kvm;

KVMGuest::KVMSharedMemory::KVMSharedMemory() : _arena(NULL), _arena_size(0)
{

}


void *KVMGuest::KVMSharedMemory::allocate(size_t size)
{
	if (size % 16) {
		size += 16 - (size % 16);
	}

	//DEBUG << "Locating region of size " << std::hex << size;
	region_map_t::iterator rgn = free_regions.lower_bound(size);

	if (rgn == free_regions.end())
		return NULL;

	size_t region_size = rgn->first;
	struct region_header *region_hdr = rgn->second;

	//DEBUG << "Found region of size " << std::hex << region_size << " @ " << (uint64_t)region_hdr;

	free_regions.erase(rgn);

	if (region_size > size) {
		size_t new_size = region_size - size;
		struct region_header *new_ptr = (struct region_header *)((uint64_t)region_hdr + size + 8);
		new_ptr->size = new_size - 8;

		//DEBUG << "Splitting Region, new size=" << std::hex << new_size << " @ " << (uint64_t)new_ptr;

		free_regions.emplace(new_size, new_ptr);
		region_hdr->size = size;
	} else if (region_size < size) {
		return NULL;
	}

	return &region_hdr->data[0];
}

void KVMGuest::KVMSharedMemory::free(void* p)
{
	struct region_header *header = (struct region_header *)((uint64_t)p - 8);

	//DEBUG << "Returning region of size " << std::hex << header->size;
	free_regions.emplace(header->size, header);

	if (free_regions.size() > 5)
		coalesce();
}

void KVMGuest::KVMSharedMemory::coalesce()
{
	//
}
