#ifndef REGALLOC_PC_H
#define REGALLOC_PC_H

register unsigned long long int __PC asm("r15");

#define read_pc() (__PC)
#define pc_mem_ptr() ((uintptr_t)__PC)
#define write_pc(__value) do { __PC = (__value); } while(0)

#endif
