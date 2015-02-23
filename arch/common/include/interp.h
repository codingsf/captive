/*
 * File:   interp.h
 * Author: spink
 *
 * Created on 17 February 2015, 18:22
 */

#ifndef INTERP_H
#define	INTERP_H

#include <define.h>

namespace captive {
	namespace arch {
		template<class DecodeType>
		class Interpreter
		{
		public:
			Interpreter();
			virtual ~Interpreter();

			virtual bool step_single(DecodeType& insn) = 0;
			virtual bool handle_irq(uint32_t irq_line) = 0;

			bool step_single_trace(DecodeType& insn);
		};
	}
}

#endif	/* INTERP_H */

