/*
 * File:   shared-jit.h
 * Author: spink
 *
 * Created on 27 March 2015, 17:49
 */

#ifndef SHARED_JIT_H
#define	SHARED_JIT_H

#ifndef __packed
#ifndef packed
#define __packed __attribute__((packed))
#else
#define __packed packed
#endif
#endif

namespace captive {
	namespace shared {
		struct TranslationBlockDescriptor
		{
			uint32_t block_id;
			uint32_t heat;
			uint32_t block_addr;
		} __packed;

		struct TranslationBlocks
		{
			uint32_t block_count;
			TranslationBlockDescriptor descriptors[];
		} __packed;

		struct RegionWorkUnit
		{
			uint32_t work_unit_id;
			uint32_t region_base_address;

			TranslationBlocks *blocks;
			void *ir;
		} __packed;

		struct BlockWorkUnit
		{
			uint32_t work_unit_id;

			void *ir;
		} __packed;
	}
}

#endif	/* SHARED_JIT_H */

