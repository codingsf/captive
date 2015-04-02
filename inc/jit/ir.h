/*
 * File:   ir.h
 * Author: spink
 *
 * Created on 10 March 2015, 18:19
 */

#ifndef IR_H
#define	IR_H

#include <jit/jit.h>
#include <shared-jit.h>

#include <map>
#include <list>
#include <set>

namespace captive {
	namespace jit {
		class IRContext;

		namespace ir {
			class IRBlock;
			class IRInstruction;

			class IRRegister
			{
			public:
				IRRegister(IRContext& owner, std::string name, uint8_t size) : _owner(owner), _name(name), _size(size) { }

				uint8_t size() const { return _size; }

				std::string name() const { return _name; }

			private:
				IRContext& _owner;
				std::string _name;
				uint8_t _size;
			};

			class IROperand
			{
			public:
				enum OperandType
				{
					INVALID,
					REGISTER,
					IMMEDIATE,
					BLOCK,
					FUNCTION
				};

				IROperand(OperandType type, uint8_t size) : _owner(NULL), _type(type), _size(size) { }

				static IROperand *create_from_bytecode(IRContext& ctx, const shared::IROperand *oper);

				inline void owner(IRInstruction& insn) { _owner = &insn; }

				inline IRInstruction& owner() const { return *_owner; }

				inline OperandType type() const { return _type; }

				inline uint8_t size() const { return _size; }

				virtual std::string to_string() const = 0;

				inline bool is_immed_or_vreg() const { return _type == IMMEDIATE || _type == REGISTER; }
				inline bool is_immed() const { return _type == IMMEDIATE; }
				inline bool is_vreg() const { return _type == REGISTER; }
				inline bool is_function() const { return _type == FUNCTION; }
				inline bool is_block() const { return _type == BLOCK; }

			private:
				IRInstruction *_owner;
				OperandType _type;
				uint8_t _size;
			};

			namespace operands
			{
				class RegisterOperand : public IROperand
				{
				public:
					RegisterOperand(IRRegister& rg) : IROperand(IROperand::REGISTER, rg.size()), _rg(rg) { }

					inline IRRegister& reg() const { return _rg; }

					std::string to_string() const override;

				private:
					IRRegister& _rg;
				};

				class ImmediateOperand : public IROperand
				{
				public:
					ImmediateOperand(uint64_t value, uint8_t size) : IROperand(IROperand::IMMEDIATE, size), _value(value) { }

					inline uint64_t value() const { return _value; }

					std::string to_string() const override { return "#" + std::to_string(_value); }

				private:
					uint64_t _value;
				};

				class BlockOperand : public IROperand
				{
				public:
					BlockOperand(IRBlock& block) : IROperand(IROperand::BLOCK, 0), _block(block) { }

					inline IRBlock& block() const { return _block; }

					std::string to_string() const override;

				private:
					IRBlock& _block;
				};

				class FunctionOperand : public IROperand
				{
				public:
					FunctionOperand(void *ptr) : IROperand(IROperand::FUNCTION, 0), _ptr(ptr) { }

					inline void *ptr() const { return _ptr; }

					std::string to_string() const override { return "&" + std::to_string((uint64_t)_ptr); }

				private:
					void *_ptr;
				};

				class InvalidOperand : public IROperand
				{
				public:
					InvalidOperand() : IROperand(IROperand::INVALID, 0) { }

					std::string to_string() const override { return "(invalid)"; }
				};
			}

			class IRInstruction
			{
			public:
				typedef std::list<IROperand *> operand_list_t;
				typedef std::set<IROperand *> operand_set_t;

				enum InstructionType
				{
					INVALID,

					MOVE,

					ADD,
					SUB,
					MUL,
					DIV,
					MOD,

					CALL,
					BRANCH,
					JUMP,
					RETURN,

					STREG,
					LDREG,
				};

				IRInstruction(IRBlock& owner, InstructionType type) : _owner(owner), _type(type) { }

				static IRInstruction *create_from_instruction(IRContext& ctx, IRBlock& owner, const shared::IRInstruction *insn);

				inline IRBlock& owner() const { return _owner; }

				inline InstructionType type() const { return _type; }

				virtual std::string mnemonic() const = 0;

				operand_set_t::iterator begin() { return _all.begin(); }
				operand_set_t::iterator end() { return _all.end(); }

				operand_set_t::const_iterator begin() const { return _all.begin(); }
				operand_set_t::const_iterator end() const { return _all.end(); }

				std::string to_string() const;

			protected:
				inline void add_input_operand(IROperand& oper)
				{
					_ins.push_back(&oper);
					_all.insert(&oper);
				}

				inline void add_output_operand(IROperand& oper)
				{
					_outs.push_back(&oper);
					_all.insert(&oper);
				}

				inline void add_none_operand(IROperand& oper)
				{
					_all.insert(&oper);
				}

			private:
				IRBlock& _owner;
				InstructionType _type;

				operand_list_t _ins, _outs;
				operand_set_t _all;
			};

			class BinaryIRInstruction : public IRInstruction
			{
			public:
				BinaryIRInstruction(IRBlock& owner, IRInstruction::InstructionType type, IROperand& src, IROperand& dst)
					: IRInstruction(owner, type), _src(src), _dst(dst)
				{
					assert(src.is_immed_or_vreg() && dst.is_vreg());

					add_input_operand(src);
					add_input_operand(dst);
					add_output_operand(dst);
				}

				inline IROperand& src() const { return _src; }
				inline IROperand& dst() const { return _dst; }

			private:
				IROperand& _src;
				IROperand& _dst;
			};

			class TerminatorIRInstruction : public IRInstruction
			{
			public:
				TerminatorIRInstruction(IRBlock& owner, IRInstruction::InstructionType type)
					: IRInstruction(owner, type)
				{

				}

			protected:
				void add_destination(IRBlock& block);
			};

			namespace instructions {
				class Invalid : public IRInstruction
				{
				public:
					Invalid(IRBlock& owner) : IRInstruction(owner, IRInstruction::INVALID) { }

					std::string mnemonic() const override { return "(invalid)"; }
				};

				class Move : public BinaryIRInstruction
				{
				public:
					Move(IRBlock& owner, IROperand& src, IROperand& dst)
						: BinaryIRInstruction(owner, IRInstruction::MOVE, src, dst)
					{

					}

					std::string mnemonic() const override { return "mov"; }
				};

				class Add : public BinaryIRInstruction
				{
				public:
					Add(IRBlock& owner, IROperand& src, IROperand& dst)
						: BinaryIRInstruction(owner, IRInstruction::ADD, src, dst)
					{

					}

					std::string mnemonic() const override { return "add"; }
				};

				class Call : public IRInstruction
				{
				public:
					Call(IRBlock& owner, IROperand& fn) : IRInstruction(owner, IRInstruction::CALL), _target(fn)
					{
						assert(fn.is_function());
						add_none_operand(fn);
					}

					std::string mnemonic() const override { return "call"; }

				private:
					IROperand& _target;
				};

				class Return : public TerminatorIRInstruction
				{
				public:
					Return(IRBlock& owner)
						: TerminatorIRInstruction(owner, IRInstruction::RETURN)
					{
					}

					std::string mnemonic() const override { return "return"; }
				};

				class Jump : public TerminatorIRInstruction
				{
				public:
					Jump(IRBlock& owner, IROperand& target)
						: TerminatorIRInstruction(owner, IRInstruction::JUMP)
					{
						assert(target.is_block());
						add_none_operand(target);
						add_destination(((operands::BlockOperand &)target).block());
					}

					std::string mnemonic() const override { return "jump"; }
				};

				class Branch : public TerminatorIRInstruction
				{
				public:
					Branch(IRBlock& owner, IROperand& cond, IROperand& tt, IROperand& ft)
						: TerminatorIRInstruction(owner, IRInstruction::BRANCH)
					{
						assert(cond.is_immed_or_vreg());
						assert(tt.is_block() && ft.is_block());

						add_input_operand(cond);

						add_none_operand(tt);
						add_none_operand(ft);

						add_destination(((operands::BlockOperand &)tt).block());
						add_destination(((operands::BlockOperand &)ft).block());
					}

					std::string mnemonic() const override { return "branch"; }
				};

				class RegisterStore : public IRInstruction
				{
				public:
					RegisterStore(IRBlock& owner, IROperand& val, IROperand& loc)
						: IRInstruction(owner, IRInstruction::STREG), _val(val), _loc(loc)
					{
						assert(val.is_immed_or_vreg() && loc.is_immed_or_vreg());

						add_input_operand(val);
						add_input_operand(loc);
					}

					std::string mnemonic() const override { return "st-reg"; }

				private:
					IROperand& _val;
					IROperand& _loc;
				};

				class RegisterLoad : public IRInstruction
				{
				public:
					RegisterLoad(IRBlock& owner, IROperand& loc, IROperand& val)
						: IRInstruction(owner, IRInstruction::LDREG), _val(val), _loc(loc)
					{
						assert(val.is_vreg() && loc.is_immed_or_vreg());

						add_input_operand(val);
						add_input_operand(loc);
					}

					std::string mnemonic() const override { return "ld-reg"; }

				private:
					IROperand& _val;
					IROperand& _loc;
				};
			}

			class IRBlock
			{
			public:
				typedef std::list<ir::IRInstruction *> instruction_list_t;
				typedef std::list<ir::IRBlock *> block_list_t;

				IRBlock(IRContext& owner, std::string name) : _owner(owner), _name(name) { }

				inline void append_instruction(ir::IRInstruction *instruction)
				{
					instructions.push_back(instruction);
				}

				inline IRContext& owner() const { return _owner; }

				inline instruction_list_t::iterator begin() { return instructions.begin(); }
				inline instruction_list_t::iterator end() { return instructions.end(); }

				inline instruction_list_t::const_iterator begin() const { return instructions.begin(); }
				inline instruction_list_t::const_iterator end() const { return instructions.end(); }

				std::string name() const { return _name; }

				std::string to_string() const;

				inline void add_predecessor(IRBlock &pred)
				{
					preds.push_back(&pred);
				}

				inline void add_successor(IRBlock &succ)
				{
					succs.push_back(&succ);
				}

			private:
				IRContext& _owner;
				std::string _name;

				instruction_list_t instructions;

				block_list_t preds;
				block_list_t succs;
			};
		}

		class IRContext
		{
		public:
			typedef std::map<uint32_t, ir::IRBlock *> block_map_t;
			typedef std::map<uint32_t, ir::IRRegister *> register_map_t;

			static IRContext *build_context(const shared::TranslationBlock *tb);

			ir::IRRegister& get_or_create_vreg(uint32_t id, uint8_t size);
			ir::IRBlock& get_or_create_block(uint32_t id);

			inline block_map_t::iterator begin() { return blocks.begin(); }
			inline block_map_t::iterator end() { return blocks.end(); }

			inline block_map_t::const_iterator begin() const { return blocks.begin(); }
			inline block_map_t::const_iterator end() const { return blocks.end(); }

			std::string to_string() const;

			ir::IRBlock& entry_block() { return get_or_create_block(0); }

		private:
			IRContext();

			std::map<uint32_t, ir::IRBlock *> blocks;
			std::map<uint32_t, ir::IRRegister *> registers;
		};
	}
}

#endif	/* IR_H */

