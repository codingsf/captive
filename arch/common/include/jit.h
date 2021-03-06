/*
 * File:   jit.h
 * Author: spink
 *
 * Created on 27 February 2015, 12:11
 */

#ifndef JIT_H
#define	JIT_H

#include <define.h>

namespace captive {
	namespace arch {
		namespace jit {
			class TranslationContext;
		}

		class Decode;

		class JIT
		{
		public:
			JIT();
			virtual ~JIT();

			void trace(bool enable) { _trace = enable; }
			bool trace() const { return _trace; }
			
			virtual bool translate(const Decode *decode_obj, jit::TranslationContext& ctx) = 0;
			
		protected:
			bool _trace;
		};
		
		static inline uint32_t __ror(uint32_t value, uint8_t amt) {
			return (value >> amt) | (value << (32 - amt));
		}
		
		template<typename t, int e>
		class vector
		{
			
		};
	}
}

#endif	/* JIT_H */

