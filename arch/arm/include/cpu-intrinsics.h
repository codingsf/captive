/*
 * File:   cpu-intrinsics.h
 * Author: spink
 *
 * Created on 11 February 2015, 14:21
 */

#ifndef CPU_INTRINSICS_H
#define	CPU_INTRINSICS_H

//#define TRACE

#include <printf.h>
#include <env.h>

#define TRACE_REG_READ(_id) printf("(R[%s] => %08x)", #_id, cpu.state.regs._id)
#define TRACE_REG_WRITE(_id) printf("(R[%s] <= %08x)", #_id, cpu.state.regs._id)
#define TRACE_RB_READ(_bank, _id) printf("(RB[%s][%d] => %08x)", #_bank, _id, cpu.state.regs._bank[_id])
#define TRACE_RB_WRITE(_bank, _id) printf("(RB[%s][%d] <= %08x)", #_bank, _id, cpu.state.regs._bank[_id])

#ifdef TRACE

#define read_register(_id) (TRACE_REG_READ(_id), cpu.state.regs._id)
#define read_register_nt(_id) read_register(_id)
#define read_register_bank(_bank, _id) (TRACE_RB_READ(_bank, _id), cpu.state.regs._bank[_id])
#define read_register_bank_nt(_bank, _id) read_register_bank(_bank, _id)

#define write_register(_id, _value) (cpu.state.regs._id = _value, TRACE_REG_WRITE(_id))
#define write_register_nt(_id, _value) write_register(_id, _value)
#define write_register_bank(_bank, _id, _value) (cpu.state.regs._bank[_id] = _value, TRACE_RB_WRITE(_bank, _id))
#define write_register_bank_nt(_bank, _id, _value) write_register_bank(_bank, _id, _value)

#else

#define read_register(_id) cpu.state.regs._id
#define read_register_nt(_id) read_register(_id)
#define read_register_bank(_bank, _id) cpu.state.regs._bank[_id]
#define read_register_bank_nt(_bank, _id) read_register_bank(_bank, _id)

#define write_register(_id, _value) cpu.state.regs._id = _value
#define write_register_nt(_id, _value) write_register(_id, _value)
#define write_register_bank(_bank, _id, _value) cpu.state.regs._bank[_id] = _value
#define write_register_bank_nt(_bank, _id, _value) write_register_bank(_bank, _id, _value)

#endif

#define enter_user_mode()
#define enter_kernel_mode()

#define push_interrupt(_l)
#define pop_interrupt()
#define pend_interrupt()

#define set_cpu_mode(_v) cpu.state.isa_mode = _v
#define get_cpu_mode() cpu.state.isa_mode

#define trap() asm volatile ("out %0, $0xff\n" :: "a"(2))
#define halt_cpu() asm volatile ("out %0, $0xff\n" :: "a"(2))

#define write_device(a, b, c) cpu.env().write_core_device(cpu, a, b, c)
#define read_device(a, b, c) cpu.env().read_core_device(cpu, a, b, c)

#define flush_dtlb_entry(v)
#define flush_itlb_entry(v)

#define mem_read_8(_addr, _data) (_data = *((uint8_t*)((uint64_t)_addr)), 0)
#define mem_read_16(_addr, _data) (_data = *((uint16_t*)((uint64_t)_addr)), 0)
#define mem_read_32(_addr, _data) (_data = *((uint32_t*)((uint64_t)_addr)), 0)
#define mem_read_64(_addr, _data) (_data = *((uint64_t*)((uint64_t)_addr)), 0)

#define mem_write_8(_addr, _data) (*((uint8_t*)((uint64_t)_addr)) = ((uint8_t)_data), 0)
#define mem_write_16(_addr, _data) (*((uint16_t*)((uint64_t)_addr)) = ((uint16_t)_data), 0)
#define mem_write_32(_addr, _data) (*((uint32_t*)((uint64_t)_addr)) = ((uint32_t)_data), 0)
#define mem_write_64(_addr, _data) (*((uint64_t*)((uint64_t)_addr)) = ((uint64_t)_data), 0)

#define mem_read_8_user(_addr, _data) mem_read_8(_addr, _data)
#define mem_read_16_user(_addr, _data) mem_read_16(_addr, _data)
#define mem_read_32_user(_addr, _data) mem_read_32(_addr, _data)
#define mem_read_64_user(_addr, _data) mem_read_64(_addr, _data)

#define mem_write_8_user(_addr, _data) mem_write_8(_addr, _data)
#define mem_write_16_user(_addr, _data) mem_write_16(_addr, _data)
#define mem_write_32_user(_addr, _data) mem_write_32(_addr, _data)
#define mem_write_64_user(_addr, _data) mem_write_64(_addr, _data)

#endif	/* CPU_INTRINSICS_H */

