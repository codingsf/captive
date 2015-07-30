/* 
 * File:   ir-sorter.h
 * Author: s0457958
 *
 * Created on 30 July 2015, 09:59
 */

#ifndef IR_SORTER_H
#define	IR_SORTER_H

#include <define.h>
#include <jit/translation-context.h>

namespace captive {
	namespace arch {
		namespace jit {
			class TranslationContext;
			
			class IRSorter
			{
			public:
				IRSorter(TranslationContext& _ctx) : ctx(_ctx) { }
				virtual bool sort() = 0;
				
			protected:
				TranslationContext& ctx;
				
				inline uint32_t key(uint32_t idx) const { return ctx.at(idx)->ir_block; }
				inline void exchange(uint32_t a, uint32_t b) { ctx.swap(a, b); }
			};
			
			namespace algo {
				class GnomeSort : public IRSorter
				{
				public:
					GnomeSort(TranslationContext& ctx) : IRSorter(ctx) { }
					bool sort() override;
				};
				
				class MergeSort : public IRSorter
				{
				public:
					MergeSort(TranslationContext& ctx) : IRSorter(ctx) { }
					bool sort() override;
				};
			}
		}
	}
}

#endif	/* IR_SORTER_H */
