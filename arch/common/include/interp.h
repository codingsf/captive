/*
 * File:   interp.h
 * Author: spink
 *
 * Created on 17 February 2015, 18:22
 */

#ifndef INTERP_H
#define	INTERP_H

#include <define.h>
#include <mmu.h>

namespace captive {
	namespace arch {
		class Decode;

		class Interpreter
		{
		public:
			Interpreter();
			virtual ~Interpreter();

			virtual bool step_single(Decode& insn) = 0;

			virtual bool handle_irq(uint32_t irq_line) = 0;
			virtual bool handle_memory_fault(MMU::resolution_fault fault) = 0;
		};
	}
}

#endif	/* INTERP_H */
