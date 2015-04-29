/*
 * File:   ir.h
 * Author: spink
 *
 * Created on 28 April 2015, 14:50
 */

#ifndef IR_H
#define	IR_H

#include <define.h>
#include <shared-jit.h>
#include <map>
#include <list>
#include <vector>

namespace captive {
	namespace arch {
		namespace jit {
			class IRBlock;
			class IRContext;
			class IRInstruction;

			class IRRegister
			{
			public:
				IRRegister(IRContext& owner, shared::IRRegId id, uint8_t width) : _owner(owner), _id(id), _width(width) { }

				inline IRContext& owner() const { return _owner; }
				inline shared::IRRegId id() const { return _id; }
				inline uint8_t width() const { return _width; }

			private:
				IRContext& _owner;
				shared::IRRegId _id;
				uint8_t _width;
			};

			class IROperand
			{
				friend class IRInstruction;

			public:
				enum OperandType
				{
					Register,
					Constant,
					Block,
					Function,
					PC
				};

				IROperand() : _owner(NULL) { }

				virtual OperandType type() const = 0;

				virtual void dump() const;

			private:
				IRInstruction *_owner;

				inline void attach(IRInstruction& owner) { _owner = &owner; }
			};

			class IRRegisterOperand : public IROperand
			{
			public:
				IRRegisterOperand(IRRegister& rg) : _rg(rg) { }

				OperandType type() const override { return Register; }

				IRRegister& reg() const { return _rg; }

				void dump() const override;

			private:
				IRRegister& _rg;
			};

			class IRConstantOperand : public IROperand
			{
			public:
				IRConstantOperand(uint64_t value, uint8_t width) : _value(value), _width(width) { }

				OperandType type() const override { return Constant; }

				void dump() const override;

				uint64_t value() const { return _value; }
				uint8_t width() const { return _width; }

			private:
				uint64_t _value;
				uint8_t _width;
			};

			class IRBlockOperand : public IROperand
			{
			public:
				IRBlockOperand(IRBlock& block) : _block(block) { }

				OperandType type() const override { return Block; }

				void dump() const override;

				IRBlock& block() const { return _block; }

			private:
				IRBlock& _block;
			};

			class IRFunctionOperand : public IROperand
			{
			public:
				IRFunctionOperand(void *fnp) : _fnp(fnp) { }

				OperandType type() const override { return Function; }

				void *ptr() const { return _fnp; }

				void dump() const override;

			private:
				void *_fnp;
			};

			class IRPCOperand : public IROperand
			{
			public:
				IRPCOperand() { }

				OperandType type() const override { return PC; }
			};

			class IRInstruction
			{
				friend class IRBlock;

			public:
				enum InstructionTypes
				{
					Call,
					Jump,
					Return
				};

				IRInstruction() { }
				~IRInstruction();

				virtual InstructionTypes type() const = 0;

				inline const std::list<IROperand *>& operands() const { return _all_operands; }
				inline const std::vector<IRRegister *>& uses() const { return _uses; }
				inline const std::vector<IRRegister *>& defs() const { return _defs; }

				virtual void dump() const;

			protected:
				inline void add_input_operand(IROperand& operand)
				{
					operand.attach(*this);
					_all_operands.push_back(&operand);

					if (operand.type() == IROperand::Register)
						_uses.push_back(&((IRRegisterOperand&)operand).reg());
				}

				inline void add_output_operand(IRRegisterOperand& operand)
				{
					operand.attach(*this);
					_all_operands.push_back(&operand);
					_defs.push_back(&operand.reg());
				}

				inline void add_input_output_operand(IRRegisterOperand& operand)
				{
					operand.attach(*this);
					_all_operands.push_back(&operand);

					_uses.push_back(&operand.reg());
					_defs.push_back(&operand.reg());
				}

				virtual const char *mnemonic() const = 0;

			private:
				inline void attach(IRBlock& owner) { _owner = &owner; }

				IRBlock *_owner;

				std::list<IROperand *> _all_operands;
				std::vector<IRRegister *> _uses, _defs;
			};

			class IRBlock
			{
			public:
				IRBlock(IRContext& owner, shared::IRBlockId id) : _owner(owner), _id(id) { }
				~IRBlock();

				shared::IRBlockId id() const { return _id; }

				inline const std::list<IRInstruction *>& instructions() const { return _instructions; }

				void append_instruction(IRInstruction& insn)
				{
					_instructions.push_back(&insn);
					insn.attach(*this);
				}

			private:
				IRContext& _owner;
				shared::IRBlockId _id;
				std::list<IRInstruction *> _instructions;
			};

			class IRContext
			{
			public:
				IRContext();
				~IRContext();

				inline const std::vector<IRBlock *> blocks() const
				{
					std::vector<IRBlock *> ret;
					for (auto block : _blocks) {
						ret.push_back(block.second);
					}
					return ret;
				}

				IRBlock& get_block_by_id(shared::IRBlockId id);
				IRRegister& get_register_by_id(shared::IRRegId id, uint8_t width);

				void dump() const;

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
						_args.push_back(&operand);
					}

					inline const std::vector<IROperand *>& arguments() const { return _args; }

				protected:
					const char* mnemonic() const override { return "call"; }

				private:
					std::vector<IROperand *> _args;
				};

				class IRJumpInstruction : public IRInstruction
				{
				public:
					IRJumpInstruction(IRBlockOperand& target) { add_input_operand(target); }
					IRInstruction::InstructionTypes type() const override { return Jump; }

					IRBlockOperand& target() { return *((IRBlockOperand *)operands().front()); }

				protected:
					const char* mnemonic() const override { return "jump"; }
				};

				class IRRetInstruction : public IRInstruction
				{
				public:
					IRRetInstruction() { }
					IRInstruction::InstructionTypes type() const override { return Return; }

				protected:
					const char* mnemonic() const override { return "ret"; }
				};
			}
		}
	}
}

#endif	/* IR_H */
