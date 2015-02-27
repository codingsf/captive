/*
 * File:   ir-instruction.h
 * Author: spink
 *
 * Created on 27 February 2015, 16:33
 */

#ifndef IR_INSTRUCTION_H
#define	IR_INSTRUCTION_H

namespace captive {
	namespace arch {
		namespace jit {
			class IRBlock;

			class IRInstruction {
			public:
				enum IRInstructionType {
					ADD,
					SUB,
					MUL,
					DIV
				};

				inline void attach(IRBlock &parent) {
					_parent = &parent;
				}

				virtual IRInstructionType type() const = 0;

			private:
				IRBlock *_parent;
			};

			class IRAddInstruction : public IRInstruction {
			public:
				IRInstructionType type() const override { return ADD; }
			};
		}
	}
}

#endif	/* IR_INSTRUCTION_H */

