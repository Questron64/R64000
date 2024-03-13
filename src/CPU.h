#ifndef CPU_H
#define CPU_H
#include <stdint.h>
#include <stdbool.h>

#ifndef MEM_SIZE
#define MEM_SIZE (16 * 1024*1024)
#endif

#define NUM_BASE_REGS 32

enum {
  X0,   ZERO = X0,
  X1,   RA   = X1,   
  X2,   SP   = X2,   
  X3,   GP   = X3,   
  X4,   TP   = X4,   
  X5,   T0   = X5,   
  X6,   T1   = X6,   
  X7,   T2   = X7,   
  X8,   S0   = X8,   
  X9,   S1   = X9,   
  X10,  A0   = X10,    
  X11,  A1   = X11,    
  X12,  A2   = X12,    
  X13,  A3   = X13,    
  X14,  A4   = X14,    
  X15,  A5   = X15,    
  X16,  A6   = X16,    
  X17,  A7   = X17,    
  X18,  S2   = X18,    
  X19,  S3   = X19,    
  X20,  S4   = X20,    
  X21,  S5   = X21,    
  X22,  S6   = X22,    
  X23,  S7   = X23,    
  X24,  S8   = X24,    
  X25,  S9   = X25,    
  X26,  S10  = X26,    
  X27,  S11  = X27,    
  X28,  T3   = X28,    
  X29,  T4   = X29,    
  X30,  T5   = X30,    
  X31,  T6   = X31,
  PC,
  NUM_REGS
};

extern uint32_t    reg[NUM_REGS];
extern uint8_t     mem[MEM_SIZE];
extern bool        breakpoint[MEM_SIZE / 4];
extern const char *reg_names [NUM_REGS];
extern const char *reg_anames[NUM_REGS];

int GetRegisterIndex(const char *name);

void Reset();

void CPUWrite32(uint32_t addr, uint32_t v32);
void CPUWrite16(uint32_t addr, uint16_t v16);
void CPUWrite8 (uint32_t addr, uint8_t  v8 );

uint32_t CPURead32(uint32_t addr);
uint16_t CPURead16(uint32_t addr);
uint8_t  CPURead8 (uint32_t addr);

unsigned CPUStep(unsigned cycles);
uint32_t Assemble(const char *line);
int Unassemble(uint32_t ins, char buf[64]);

#endif
