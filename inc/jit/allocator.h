/*
 * File:   allocator.h
 * Author: spink
 *
 * Created on 10 March 2015, 15:33
 */

#ifndef ALLOCATOR_H
#define	ALLOCATOR_H

#include <define.h>

namespace captive {
	namespace jit {
		class AllocationRegion;

		class Allocator
		{
			friend class AllocationRegion;

		public:
			Allocator(void *arena, uint64_t arena_size);
			~Allocator();

			AllocationRegion *allocate(size_t size);

		private:
			void *_arena, *_arena_next;
			uint64_t _arena_size;

			bool resize(AllocationRegion& region, size_t new_size);
			void free(AllocationRegion& region);
		};

		class AllocationRegion
		{
			friend class Allocator;

		public:
			AllocationRegion(Allocator& owner, void *base_address, uint64_t requested_size, uint64_t actual_size)
			: _owner(owner),
				_base_address(base_address),
				_size(requested_size),
				_actual_size(actual_size) { }

			inline bool resize(size_t new_size) { return _owner.resize(*this, new_size); }
			inline void free() { _owner.free(*this); }

			inline Allocator& owner() const { return _owner; }

			inline void *base_address() const { return _base_address; }
			inline uint64_t size() const { return _size; }
			inline uint64_t actual_size() const { return _actual_size; }

		private:
			Allocator& _owner;
			void *_base_address;
			uint64_t _size, _actual_size;
		};
	}
}

#endif	/* ALLOCATOR_H */

