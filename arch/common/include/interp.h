/*
 * File:   interp.h
 * Author: spink
 *
 * Created on 17 February 2015, 18:22
 */

#ifndef INTERP_H
#define	INTERP_H

namespace captive {
	namespace arch {
		template<class DecodeType>
		class Interpreter
		{
		public:
			Interpreter();
			virtual ~Interpreter();

			virtual bool step_single(DecodeType& insn) = 0;
			bool step_single_trace(DecodeType& insn);

			bool trace;
		};
	}
}

#endif	/* INTERP_H */

