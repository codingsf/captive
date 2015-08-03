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
#include <shared-jit.h>

namespace captive {
	namespace arch {
		namespace jit {
			class TranslationContext;
						
			class IRSorter
			{
			public:
				IRSorter(TranslationContext& _ctx) : ctx(_ctx) { }
				shared::IRInstruction *perform_sort();
				
			protected:
				virtual bool do_sort() = 0;
				
				inline shared::IRBlockId key(uint32_t idx) const { return ctx.at(insn_idxs[idx])->ir_block; }
				inline void exchange(uint32_t a, uint32_t b) { std::swap(insn_idxs[a], insn_idxs[b]); }
				inline int32_t compare(uint32_t a, uint32_t b) { return key(a) - key(b); }
				inline uint32_t count() { return ctx.count(); }
				
			private:
				uint32_t *insn_idxs;
				TranslationContext& ctx;
			};
			
			namespace algo {
				class GnomeSort : public IRSorter
				{
				public:
					GnomeSort(TranslationContext& ctx) : IRSorter(ctx) { }
				protected:
					 bool do_sort() override;
				};
				
				class MergeSort : public IRSorter
				{
				public:
					MergeSort(TranslationContext& ctx) : IRSorter(ctx) { }
				protected:
					 bool do_sort() override;
					
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
