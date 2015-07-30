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
				
				inline shared::IRBlockId key(uint32_t idx) const { return ctx.at(idx)->ir_block; }
				inline void exchange(uint32_t a, uint32_t b) { ctx.swap(a, b); }
				inline int32_t compare(uint32_t a, uint32_t b) { return key(a) - key(b); }
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
					
				private:
					bool sort(int from, int to);
					void merge(int from, int pivot, int to, int len1, int len2);
					void rotate(int from, int mid, int to);
					void reverse(int from, int to);
					bool insert_sort(int from, int to);
					int upper(int from, int to, int val);
					int lower(int from, int to, int val);
					inline int gcd(int m, int n) { while (n != 0) { int t = m % n; m = n; n = t; } return m; }
				};
			}
		}
	}
}

#endif	/* IR_SORTER_H */
