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
#include <set>

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
				enum RegisterAllocationClass
				{
					None,
					Register,
					Stack,
				};

				IRRegisterOperand(IRRegister& rg) : _rg(rg), _alloc_class(None), _alloc_data(0) { }

				OperandType type() const override { return IROperand::Register; }

				IRRegister& reg() const { return _rg; }

				void dump() const override;

				void allocate(RegisterAllocationClass alloc_class, uint64_t value)
				{
					_alloc_class = alloc_class;
					_alloc_data = value;
				}

				inline RegisterAllocationClass allocation_class() const { return _alloc_class; }
				inline uint64_t allocation_data() const { return _alloc_data; }

				inline bool is_allocated() const { return _alloc_class != None; }
				inline bool is_allocated_reg() const { return _alloc_class == Register; }
				inline bool is_allocated_stack() const { return _alloc_class == Stack; }

			private:
				IRRegister& _rg;

				RegisterAllocationClass _alloc_class;
				uint64_t _alloc_data;
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
				IRPCOperand(uint32_t val) : _val(val) { }

				OperandType type() const override { return PC; }

				uint32_t value() const { return _val; }

				void dump() const override;

			private:
				uint32_t _val;
			};

			class IRInstruction
			{
				friend class IRBlock;

			public:
				enum InstructionTypes
				{
					Nop,
					Trap,
					Verify,

					Call,
					Jump,
					Branch,
					Return,

					Move,
					ConditionalMove,
					LoadPC,

					SignExtend,
					ZeroExtend,
					Truncate,

					ReadRegister,
					WriteRegister,
					ReadMemory,
					WriteMemory,
					ReadUserMemory,
					WriteUserMemory,
					ReadDevice,
					WriteDevice,
					SetCPUMode,

					Add,
					Sub,
					Mul,
					Div,
					Mod,

					ShiftLeft,
					ShiftRight,
					ArithmeticShiftRight,
					CountLeadingZeroes,

					BitwiseAnd,
					BitwiseOr,
					BitwiseXor,

					CompareEqual,
					CompareNotEqual,
					CompareLessThan,
					CompareGreaterThan,
					CompareLessThanOrEqual,
					CompareGreaterThanOrEqual,
				};

				IRInstruction() : _owner(NULL), _next(NULL) { }
				~IRInstruction();

				virtual InstructionTypes type() const = 0;

				inline const std::list<IROperand *>& operands() const { return _all_operands; }
				inline const std::vector<IRRegisterOperand *>& uses() const { return _uses; }
				inline const std::vector<IRRegisterOperand *>& defs() const { return _defs; }

				const std::set<IRRegister *> live_ins() const { return _live_in; }
				const std::set<IRRegister *> live_outs() const { return _live_out; }

				virtual void dump() const;

				inline IRInstruction *next() const { return _next; }

				void remove_from_parent();

			protected:
				inline void add_operand(IROperand& operand)
				{
					operand.attach(*this);
					_all_operands.push_back(&operand);
				}

				inline void add_input_operand(IROperand& operand)
				{
					operand.attach(*this);
					_all_operands.push_back(&operand);

					if (operand.type() == IROperand::Register)
						_uses.push_back(&((IRRegisterOperand&)operand));
				}

				inline void add_output_operand(IRRegisterOperand& operand)
				{
					operand.attach(*this);
					_all_operands.push_back(&operand);
					_defs.push_back(&operand);
				}

				inline void add_input_output_operand(IRRegisterOperand& operand)
				{
					operand.attach(*this);
					_all_operands.push_back(&operand);

					_uses.push_back(&operand);
					_defs.push_back(&operand);
				}

				virtual const char *mnemonic() const = 0;

			private:
				inline void attach(IRBlock& owner) { _owner = &owner; _next = NULL; }
				inline void detach(IRBlock& owner) { _owner = NULL; _next = NULL; }

				IRBlock *_owner;
				IRInstruction *_next;

				std::list<IROperand *> _all_operands;
				std::vector<IRRegisterOperand *> _uses, _defs;

				std::set<IRRegister *> _live_in, _live_out;
			};

			namespace instructions
			{
				class IRTerminatorInstruction;
			}

			class IRBlock
			{
			public:
				IRBlock(IRContext& owner, shared::IRBlockId id) : _owner(owner), _id(id) { }
				~IRBlock();

				shared::IRBlockId id() const { return _id; }

				inline const std::list<IRInstruction *>& instructions() const { return _instructions; }

				inline instructions::IRTerminatorInstruction& terminator() const
				{
					assert(_instructions.size() > 0);

					IRInstruction& last_insn = *(_instructions.back());
					assert(last_insn.type() == IRInstruction::Jump || last_insn.type() == IRInstruction::Branch || last_insn.type() == IRInstruction::Return);

					return (instructions::IRTerminatorInstruction&)last_insn;
				}

				inline void add_successor(IRBlock& successor)
				{
					_succ.insert(&successor);
				}

				inline void remove_successors()
				{
					_succ.clear();
				}

				inline void add_predecessor(IRBlock& predecessor)
				{
					_pred.insert(&predecessor);
				}

				inline void remove_predecessors()
				{
					_pred.clear();
				}

				inline void remove_predecessor(IRBlock& predecessor)
				{
					for (auto PI = _pred.begin(), PE = _pred.end(); PI != PE; ++PI) {
						if ((*PI) == &predecessor) {
							_pred.erase(PI);
							break;
						}
					}
				}

				inline const std::set<IRBlock *> successors() const { return _succ; }
				inline const std::set<IRBlock *> predecessors() const { return _pred; }

				inline const std::set<IRRegister *>& live_ins() const { return _live_in; }
				inline const std::set<IRRegister *>& live_outs() const { return _live_out; }

				void append_instruction(IRInstruction& insn)
				{
					if (_instructions.size() > 0) {
						_instructions.back()->_next = &insn;
					}

					_instructions.push_back(&insn);

					insn.attach(*this);
				}

				void remove_instruction(IRInstruction& insn)
				{
					IRInstruction *prev = NULL;
					for (auto IS = _instructions.begin(), IE = _instructions.end(); IS != IE; ++IS) {
						if ((*IS) == &insn) {
							if (prev) {
								prev->_next = insn._next;
							}

							_instructions.erase(IS);
							break;
						}

						prev = *IS;
					}

					insn.detach(*this);
				}

				void remove_from_parent();

				void invalidate_liveness();
				void calculate_liveness();

			private:
				IRContext& _owner;
				shared::IRBlockId _id;
				std::list<IRInstruction *> _instructions;

				std::set<IRRegister *> _live_in, _live_out;
				std::set<IRBlock *> _succ, _pred;
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

				void remove_block(IRBlock& block);

			private:
				typedef std::map<shared::IRBlockId, IRBlock *> block_map_t;
				block_map_t _blocks;

				typedef std::map<shared::IRRegId, IRRegister *> reg_map_t;
				reg_map_t _registers;
			};

			namespace instructions {
				class IRNopInstruction : public IRInstruction
				{
				public:
					IRNopInstruction() { }
					IRInstruction::InstructionTypes type() const override { return Nop; }

				protected:
					const char* mnemonic() const override { return "nop"; }
				};

				class IRTrapInstruction : public IRInstruction
				{
				public:
					IRTrapInstruction() { }
					IRInstruction::InstructionTypes type() const override { return Trap; }

				protected:
					const char* mnemonic() const override { return "trap"; }
				};

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

				class IRTerminatorInstruction : public IRInstruction
				{
				public:
					IRTerminatorInstruction() { }

					inline const std::vector<IRBlock *> successors() const { return _succ; }

				protected:
					inline void add_successor(IRBlock& succ)
					{
						_succ.push_back(&succ);
					}

				private:
					std::vector<IRBlock *> _succ;
				};

				class IRJumpInstruction : public IRTerminatorInstruction
				{
				public:
					IRJumpInstruction(IRBlockOperand& target) { add_input_operand(target); add_successor(target.block()); }
					IRInstruction::InstructionTypes type() const override { return Jump; }

					IRBlockOperand& target() { return *((IRBlockOperand *)operands().front()); }

				protected:
					const char* mnemonic() const override { return "jump"; }
				};

				class IRBranchInstruction : public IRTerminatorInstruction
				{
				public:
					IRBranchInstruction(IROperand& cond, IRBlockOperand& true_target, IRBlockOperand& false_target)
						: _cond(cond), _tt(true_target), _ft(false_target)
					{
						add_input_operand(cond);
						add_input_operand(true_target);
						add_input_operand(false_target);

						add_successor(true_target.block());
						add_successor(false_target.block());
					}

					IRInstruction::InstructionTypes type() const override { return Branch; }

					IROperand& condition() const { return _cond; }
					IRBlockOperand& true_target() const { return _tt; }
					IRBlockOperand& false_target() const { return _ft; }

				protected:
					const char* mnemonic() const override { return "branch"; }

				private:
					IROperand& _cond;
					IRBlockOperand& _tt;
					IRBlockOperand& _ft;
				};

				class IRRetInstruction : public IRTerminatorInstruction
				{
				public:
					IRRetInstruction() { }
					IRInstruction::InstructionTypes type() const override { return Return; }

				protected:
					const char* mnemonic() const override { return "ret"; }
				};

				class IRMovInstruction : public IRInstruction
				{
				public:
					IRMovInstruction(IROperand& src, IRRegisterOperand& dest) : _src(src), _dst(dest)
					{
						add_input_operand(src);
						add_output_operand(dest);
					}

					IRInstruction::InstructionTypes type() const override { return Move; }

					inline IROperand& source() const { return _src; }
					inline IRRegisterOperand& destination() const { return _dst; }

				protected:
					const char* mnemonic() const override { return "mov"; }

				private:
					IROperand& _src;
					IRRegisterOperand& _dst;
				};

				class IRCondMovInstruction : public IRInstruction
				{
				public:
					IRCondMovInstruction(IROperand& cond, IROperand& src, IRRegisterOperand& dest)
					{
						add_input_operand(cond);
						add_input_operand(src);
						add_output_operand(dest);
					}

					IRInstruction::InstructionTypes type() const override { return ConditionalMove; }

				protected:
					const char* mnemonic() const override { return "cmov"; }
				};

				class IRLoadPCInstruction : public IRInstruction
				{
				public:
					IRLoadPCInstruction(IRRegisterOperand& dest) : _dst(dest)
					{
						add_output_operand(dest);
					}

					IRInstruction::InstructionTypes type() const override { return LoadPC; }

					IRRegisterOperand& destination() const { return _dst; }

				protected:
					const char* mnemonic() const override { return "ldpc"; }

				private:
					IRRegisterOperand& _dst;
				};

				class IRChangeSizeInstruction : public IRInstruction
				{
				public:
					IRChangeSizeInstruction(IROperand& src, IRRegisterOperand& dest) : _src(src), _dest(dest)
					{
						add_input_operand(src);
						add_output_operand(dest);
					}

					inline IROperand& source() const { return _src; }
					inline IRRegisterOperand& destination() const { return _dest; }

				private:
					IROperand& _src;
					IRRegisterOperand& _dest;
				};

				class IRSXInstruction : public IRChangeSizeInstruction
				{
				public:
					IRSXInstruction(IROperand& src, IRRegisterOperand& dest) : IRChangeSizeInstruction(src, dest)
					{
					}

					IRInstruction::InstructionTypes type() const override { return SignExtend; }

				protected:
					const char* mnemonic() const override { return "sx"; }
				};

				class IRZXInstruction : public IRChangeSizeInstruction
				{
				public:
					IRZXInstruction(IROperand& src, IRRegisterOperand& dest) : IRChangeSizeInstruction(src, dest)
					{
					}

					IRInstruction::InstructionTypes type() const override { return ZeroExtend; }

				protected:
					const char* mnemonic() const override { return "zx"; }
				};

				class IRTruncInstruction : public IRChangeSizeInstruction
				{
				public:
					IRTruncInstruction(IROperand& src, IRRegisterOperand& dest) : IRChangeSizeInstruction(src, dest)
					{
					}

					IRInstruction::InstructionTypes type() const override { return Truncate; }

				protected:
					const char* mnemonic() const override { return "trunc"; }
				};

				class IRReadRegisterInstruction : public IRInstruction
				{
				public:
					IRReadRegisterInstruction(IROperand& offset, IRRegisterOperand& storage) : _offset(offset), _storage(storage)
					{
						add_input_operand(offset);
						add_output_operand(storage);
					}

					IRInstruction::InstructionTypes type() const override { return ReadRegister; }

					IROperand& offset() const { return _offset; }
					IRRegisterOperand& storage() const { return _storage; }

				protected:
					const char* mnemonic() const override { return "ldreg"; }

				private:
					IROperand& _offset;
					IRRegisterOperand& _storage;
				};

				class IRWriteRegisterInstruction : public IRInstruction
				{
				public:
					IRWriteRegisterInstruction(IROperand& value, IROperand& offset) : _value(value), _offset(offset)
					{
						add_input_operand(value);
						add_input_operand(offset);
					}

					IRInstruction::InstructionTypes type() const override { return WriteRegister; }

					IROperand& value() const { return _value; }
					IROperand& offset() const { return _offset; }

				protected:
					const char* mnemonic() const override { return "streg"; }

				private:
					IROperand& _value;
					IROperand& _offset;
				};

				class IRReadMemoryInstruction : public IRInstruction
				{
				public:
					IRReadMemoryInstruction(IROperand& offset, IRRegisterOperand& storage) : _off(offset), _dst(storage)
					{
						add_input_operand(offset);
						add_output_operand(storage);
					}

					IRInstruction::InstructionTypes type() const override { return ReadMemory; }

					IROperand& offset() const { return _off; }

					IRRegisterOperand& storage() const { return _dst; }

				protected:
					const char* mnemonic() const override { return "ldmem"; }

				private:
					IROperand& _off;
					IRRegisterOperand& _dst;
				};

				class IRWriteMemoryInstruction : public IRInstruction
				{
				public:
					IRWriteMemoryInstruction(IROperand& value, IROperand& offset) : _value(value), _offset(offset)
					{
						add_input_operand(value);
						add_input_operand(offset);
					}

					IRInstruction::InstructionTypes type() const override { return WriteMemory; }

					IROperand& value() const { return _value; }
					IROperand& offset() const { return _offset; }

				protected:
					const char* mnemonic() const override { return "stmem"; }

				private:
					IROperand& _value;
					IROperand& _offset;
				};

				class IRReadUserMemoryInstruction : public IRReadMemoryInstruction
				{
				public:
					IRReadUserMemoryInstruction(IROperand& offset, IRRegisterOperand& storage) : IRReadMemoryInstruction(offset, storage) { }

					IRInstruction::InstructionTypes type() const override { return ReadUserMemory; }

				protected:
					const char* mnemonic() const override { return "ldmem-user"; }
				};

				class IRWriteUserMemoryInstruction : public IRWriteMemoryInstruction
				{
				public:
					IRWriteUserMemoryInstruction(IROperand& value, IROperand& offset) : IRWriteMemoryInstruction(value, offset) { }

					IRInstruction::InstructionTypes type() const override { return WriteUserMemory; }

				protected:
					const char* mnemonic() const override { return "stmem-user"; }
				};

				class IRSetCpuModeInstruction : public IRInstruction
				{
				public:
					IRSetCpuModeInstruction(IROperand& __new_mode) : _new_mode(__new_mode) { add_input_operand(__new_mode); }

					IRInstruction::InstructionTypes type() const override { return SetCPUMode; }

					IROperand& new_mode() const { return _new_mode; }

				protected:
					const char* mnemonic() const override { return "set-cpu-mode"; }

				private:
					IROperand& _new_mode;
				};

				class IRArithmeticInstruction : public IRInstruction
				{
				public:
					IRArithmeticInstruction(IROperand& src, IRRegisterOperand& dest) : _src(src), _dst(dest)
					{
						add_input_operand(src);
						add_input_output_operand(dest);
					}

					IROperand& source() const { return _src; }
					IRRegisterOperand& destination() const { return _dst; }

				private:
					IROperand& _src;
					IRRegisterOperand& _dst;
				};

				class IRAddInstruction : public IRArithmeticInstruction
				{
				public:
					IRAddInstruction(IROperand& src, IRRegisterOperand& dest) : IRArithmeticInstruction(src, dest) { }

					IRInstruction::InstructionTypes type() const override { return Add; }

				protected:
					const char* mnemonic() const override { return "add"; }
				};

				class IRSubInstruction : public IRArithmeticInstruction
				{
				public:
					IRSubInstruction(IROperand& src, IRRegisterOperand& dest) : IRArithmeticInstruction(src, dest) { }

					IRInstruction::InstructionTypes type() const override { return Sub; }

				protected:
					const char* mnemonic() const override { return "sub"; }
				};

				class IRMulInstruction : public IRArithmeticInstruction
				{
				public:
					IRMulInstruction(IROperand& src, IRRegisterOperand& dest) : IRArithmeticInstruction(src, dest) { }

					IRInstruction::InstructionTypes type() const override { return Mul; }

				protected:
					const char* mnemonic() const override { return "mul"; }
				};

				class IRDivInstruction : public IRArithmeticInstruction
				{
				public:
					IRDivInstruction(IROperand& src, IRRegisterOperand& dest) : IRArithmeticInstruction(src, dest) { }

					IRInstruction::InstructionTypes type() const override { return Div; }

				protected:
					const char* mnemonic() const override { return "div"; }
				};

				class IRModInstruction : public IRArithmeticInstruction
				{
				public:
					IRModInstruction(IROperand& src, IRRegisterOperand& dest) : IRArithmeticInstruction(src, dest) { }

					IRInstruction::InstructionTypes type() const override { return Mod; }

				protected:
					const char* mnemonic() const override { return "mod"; }
				};

				class IRBitwiseAndInstruction : public IRArithmeticInstruction
				{
				public:
					IRBitwiseAndInstruction(IROperand& src, IRRegisterOperand& dest) : IRArithmeticInstruction(src, dest) { }

					IRInstruction::InstructionTypes type() const override { return BitwiseAnd; }

				protected:
					const char* mnemonic() const override { return "and"; }
				};

				class IRBitwiseOrInstruction : public IRArithmeticInstruction
				{
				public:
					IRBitwiseOrInstruction(IROperand& src, IRRegisterOperand& dest) : IRArithmeticInstruction(src, dest) { }

					IRInstruction::InstructionTypes type() const override { return BitwiseOr; }

				protected:
					const char* mnemonic() const override { return "or"; }
				};

				class IRBitwiseXorInstruction : public IRArithmeticInstruction
				{
				public:
					IRBitwiseXorInstruction(IROperand& src, IRRegisterOperand& dest) : IRArithmeticInstruction(src, dest) { }

					IRInstruction::InstructionTypes type() const override { return BitwiseXor; }

				protected:
					const char* mnemonic() const override { return "xor"; }
				};

				class IRShiftInstruction : public IRInstruction
				{
				public:
					IRShiftInstruction(IROperand& amt, IRRegisterOperand& operand) : _amount(amt), _operand(operand)
					{
						add_input_operand(amt);
						add_input_operand(operand);
					}

					IROperand& amount() const { return _amount; }

					IRRegisterOperand& operand() const { return _operand; }

				private:
					IROperand& _amount;
					IRRegisterOperand& _operand;
				};

				class IRShiftLeftInstruction : public IRShiftInstruction
				{
				public:
					IRShiftLeftInstruction(IROperand& amt, IRRegisterOperand& operand)
						: IRShiftInstruction(amt, operand)
					{
					}

					IRInstruction::InstructionTypes type() const override { return ShiftLeft; }

				protected:
					const char* mnemonic() const override { return "shl"; }
				};

				class IRShiftRightInstruction : public IRShiftInstruction
				{
				public:
					IRShiftRightInstruction(IROperand& amt, IRRegisterOperand& operand)
						: IRShiftInstruction(amt, operand)
					{
					}

					IRInstruction::InstructionTypes type() const override { return ShiftRight; }

				protected:
					const char* mnemonic() const override { return "shr"; }
				};

				class IRArithmeticShiftRightInstruction : public IRShiftInstruction
				{
				public:
					IRArithmeticShiftRightInstruction(IROperand& amt, IRRegisterOperand& operand)
						: IRShiftInstruction(amt, operand)
					{
					}

					IRInstruction::InstructionTypes type() const override { return ArithmeticShiftRight; }

				protected:
					const char* mnemonic() const override { return "sar"; }
				};

				class IRDeviceInstruction : public IRInstruction
				{
				public:
					IRDeviceInstruction(IROperand& device, IROperand& offset) : _device(device), _offset(offset)
					{
						add_input_operand(device);
						add_input_operand(offset);
					}

					IROperand& device() const { return _device; }
					IROperand& offset() const { return _offset; }

				private:
					IROperand& _device;
					IROperand& _offset;
				};

				class IRWriteDeviceInstruction : public IRDeviceInstruction
				{
				public:
					IRWriteDeviceInstruction(IROperand& device, IROperand& offset, IROperand& data)
						: IRDeviceInstruction(device, offset), _data(data)
					{
						add_input_operand(data);
					}

					IRInstruction::InstructionTypes type() const override { return WriteDevice; }

					IROperand& data() const { return _data; }

				protected:
					const char* mnemonic() const override { return "wrdev"; }

				private:
					IROperand& _data;
				};

				class IRReadDeviceInstruction : public IRDeviceInstruction
				{
				public:
					IRReadDeviceInstruction(IROperand& device, IROperand& offset, IRRegisterOperand& data)
						: IRDeviceInstruction(device, offset), _data(data)
					{
						add_output_operand(data);
					}

					IRInstruction::InstructionTypes type() const override { return ReadDevice; }

					IRRegisterOperand& storage() const { return _data; }

				protected:
					const char* mnemonic() const override { return "rddev"; }

				private:
					IRRegisterOperand& _data;
				};

				class IRComparisonInstruction : public IRInstruction
				{
				public:
					IRComparisonInstruction(IROperand& lh, IROperand& rh, IRRegisterOperand& dst) : _lh(lh), _rh(rh), _dst(dst)
					{
						add_input_operand(lh);
						add_input_operand(rh);
						add_output_operand(dst);
					}

					IROperand& lhs() const { return _lh; }
					IROperand& rhs() const { return _rh; }
					IRRegisterOperand& destination() const { return _dst; }

				private:
					IROperand& _lh;
					IROperand& _rh;
					IRRegisterOperand& _dst;
				};

				class IRCompareEqualInstruction : public IRComparisonInstruction
				{
				public:
					IRCompareEqualInstruction(IROperand& lh, IROperand& rh, IRRegisterOperand& dst) : IRComparisonInstruction(lh, rh, dst)
					{

					}

					IRInstruction::InstructionTypes type() const override { return CompareEqual; }

				protected:
					const char* mnemonic() const override { return "cmpeq"; }
				};

				class IRCompareNotEqualInstruction : public IRComparisonInstruction
				{
				public:
					IRCompareNotEqualInstruction(IROperand& lh, IROperand& rh, IRRegisterOperand& dst) : IRComparisonInstruction(lh, rh, dst)
					{

					}

					IRInstruction::InstructionTypes type() const override { return CompareNotEqual; }

				protected:
					const char* mnemonic() const override { return "cmpne"; }
				};

				class IRCompareLessThanInstruction : public IRComparisonInstruction
				{
				public:
					IRCompareLessThanInstruction(IROperand& lh, IROperand& rh, IRRegisterOperand& dst) : IRComparisonInstruction(lh, rh, dst)
					{

					}

					IRInstruction::InstructionTypes type() const override { return CompareLessThan; }

				protected:
					const char* mnemonic() const override { return "cmplt"; }
				};

				class IRCompareGreaterThanInstruction : public IRComparisonInstruction
				{
				public:
					IRCompareGreaterThanInstruction(IROperand& lh, IROperand& rh, IRRegisterOperand& dst) : IRComparisonInstruction(lh, rh, dst)
					{

					}

					IRInstruction::InstructionTypes type() const override { return CompareGreaterThan; }

				protected:
					const char* mnemonic() const override { return "cmpgt"; }
				};

				class IRCompareLessThanOrEqualInstruction : public IRComparisonInstruction
				{
				public:
					IRCompareLessThanOrEqualInstruction(IROperand& lh, IROperand& rh, IRRegisterOperand& dst) : IRComparisonInstruction(lh, rh, dst)
					{

					}

					IRInstruction::InstructionTypes type() const override { return CompareLessThanOrEqual; }

				protected:
					const char* mnemonic() const override { return "cmplte"; }
				};

				class IRCompareGreaterThanOrEqualInstruction : public IRComparisonInstruction
				{
				public:
					IRCompareGreaterThanOrEqualInstruction(IROperand& lh, IROperand& rh, IRRegisterOperand& dst) : IRComparisonInstruction(lh, rh, dst)
					{

					}

					IRInstruction::InstructionTypes type() const override { return CompareGreaterThanOrEqual; }

				protected:
					const char* mnemonic() const override { return "cmpgte"; }
				};

				class IRVerifyInstruction : public IRInstruction
				{
				public:
					IRVerifyInstruction(IRPCOperand& pc) : _pc(pc) { add_operand(pc); }

					IRInstruction::InstructionTypes type() const override { return Verify; }

					IRPCOperand& pc() const { return _pc; }

				protected:
					const char* mnemonic() const override { return "verify"; }

				private:
					IRPCOperand& _pc;
				};
			}
		}
	}
}

#endif	/* IR_H */
