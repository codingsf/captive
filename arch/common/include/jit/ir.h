/*
 * File:   ir.h
 * Author: spink
 *
 * Created on 28 April 2015, 14:50
 */

#ifndef IR_H
#define	IR_H

#include <shared-jit.h>
#include <map>

namespace captive {
	namespace arch {
		namespace jit {
			class IRBlock;
			class IRContext;
			class IRInstruction;

			class IRRegister
			{
			public:
				IRRegister(IRContext& owner, uint8_t width) : _owner(owner), _width(width) { }

				IRContext& owner() const { return _owner; }
				uint8_t width() const { return _width; }

			private:
				IRContext& _owner;
				uint8_t _width;
			};

			class IROperand
			{
			public:
				enum OperandType
				{
					Register,
					Constant,
					Block,
					Function
				};

				IROperand() { }

				virtual OperandType type() const = 0;
			};

			class IRRegisterOperand : public IROperand
			{
			public:
				IRRegisterOperand(IRRegister& rg) : _rg(rg) { }

				OperandType type() const override { return Register; }

				IRRegister& reg() const { return _rg; }

			private:
				IRRegister& _rg;
			};

			class IRConstantOperand : public IROperand
			{
			public:
				IRConstantOperand(uint64_t value, uint8_t width) : _value(value), _width(width) { }

				OperandType type() const override { return Constant; }

			private:
				uint64_t _value;
				uint8_t _width;
			};

			class IRBlockOperand : public IROperand
			{
			public:
				IRBlockOperand(IRBlock& block) : _block(block) { }

				OperandType type() const override { return Block; }

			private:
				IRBlock& _block;
			};

			class IRFunctionOperand : public IROperand
			{
			public:
				IRFunctionOperand(void *fnp) : _fnp(fnp) { }

				OperandType type() const override { return Function; }

			private:
				void *_fnp;
			};

			class IRInstruction
			{
				friend class IRBlock;

			public:
				enum InstructionTypes
				{
					Call
				};

				IRInstruction() : _owner(NULL) { }

				virtual InstructionTypes type() const = 0;

				inline const std::list<IROperand *>& operands() const { return _all_operands; }
				inline const std::vector<IRRegister *>& uses() const { return _uses; }
				inline const std::vector<IRRegister *>& defs() const { return _defs; }

			protected:
				inline void add_input_operand(IROperand& operand)
				{
					_all_operands.push_back(&operand);

					if (operand.type() == IROperand::Register)
						_uses.push_back(&((IRRegisterOperand&)operand).reg());
				}

				inline void add_output_operand(IRRegisterOperand& operand)
				{
					_all_operands.push_back(&operand);
					_defs.push_back(&operand.reg());
				}

				inline void add_input_output_operand(IRRegisterOperand& operand)
				{
					_all_operands.push_back(&operand);

					_uses.push_back(&operand.reg());
					_defs.push_back(&operand.reg());
				}

			private:
				inline void attach(IRBlock& owner) { _owner = &owner; }

				IRBlock *_owner;

				std::list<IROperand *> _all_operands;
				std::vector<IRRegister *> _uses, _defs;
			};

			class IRBlock
			{
			public:
				IRBlock(IRContext& owner) : _owner(owner) { }
				~IRBlock();

				inline const std::list<IRInstruction *>& instructions() const { return _instructions; }

				void append_instruction(IRInstruction& insn)
				{
					_instructions.push_back(&insn);
					insn.attach(*this);
				}

			private:
				IRContext& _owner;
				std::list<IRInstruction *> _instructions;
			};

			class IRContext
			{
			public:
				IRContext();
				~IRContext();

				IRBlock& get_block_by_id(shared::IRBlockId id);
				IRRegister& get_register_by_id(shared::IRRegId id, uint8_t width);

			private:
				typedef std::map<shared::IRBlockId, IRBlock *> block_map_t;
				block_map_t _blocks;

				typedef std::map<shared::IRRegId, IRRegister *> reg_map_t;
				reg_map_t _registers;
			};

			namespace instructions {
				class IRCallInstruction : public IRInstruction
				{
				public:
					IRCallInstruction(IRFunctionOperand& callee)
					{
						add_input_operand(callee);
					}

					IRInstruction::InstructionTypes type() const override { return Call; }

					inline void add_argument(IROperand& operand)
					{
						add_input_operand(operand);
					}
				};
			}
		}
	}
}

#endif	/* IR_H */
