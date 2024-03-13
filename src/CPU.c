#include "CPU.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

uint32_t reg[NUM_REGS];
uint8_t  mem[MEM_SIZE];
bool     breakpoint[MEM_SIZE / 4];

const char *reg_names[NUM_REGS] = {
  "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
  "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
  "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
  "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31",
  "pc"
};

const char *reg_anames[NUM_REGS] = {
  "zero", "ra",   "sp",   "gp",   "tp",   "t0",   "t1",   "t2",
  "s0",   "s1",   "a0",   "a1",   "a2",   "a3",   "a4",   "a5",
  "a6",   "a7",   "s2",   "s3",   "s4",   "s5",   "s6",   "s7",
  "s8",   "s9",   "s10",  "s11",  "t3",   "t4",   "t5",   "t6",
  "pc"
};

int GetRegisterIndex(const char *name) {
  for(int i = 0; i < NUM_REGS; i++)
    if(!strcmp(reg_names[i], name) || !strcmp(reg_anames[i], name))
      return i;
  return -1;
}


void Reset() {
  for(int i = 0; i < NUM_REGS; i++)
    reg[i] = 0xDEAD'BEEF;
}

void CPUWrite32(uint32_t a, uint32_t v) {
  if(a + 3 < MEM_SIZE) {
    mem[a+0] = v;
    mem[a+1] = v >> 8;
    mem[a+2] = v >> 16;
    mem[a+3] = v >> 24;
  } else {
    // TODO: Exception
  }
}

void CPUWrite16(uint32_t a, uint16_t v) {
  if(a + 1 < MEM_SIZE) {
    mem[a+0] = v;
    mem[a+1] = v >> 8;
  } else {
    // TODO: Exception
  }
}

void CPUWrite8(uint32_t a, uint8_t v) {
  if(a < MEM_SIZE) {
    mem[a] = v;
  } else {
    // TODO: Exception
  }
}

static uint32_t CPURead32_Unchecked(uint32_t a) {
  return mem[a+0] | mem[a+1] << 8 | mem[a+2] << 16 | mem[a+3] << 24;
}

static uint16_t CPURead16_Unchecked(uint16_t a) {
  return mem[a+0] | mem[a+1] << 8;
}

uint32_t CPURead32(uint32_t a) {
  if(a + 3 < MEM_SIZE) {
    return CPURead32_Unchecked(a);
  } else {
    // TODO: Exception
    return 0;
  }
}

uint16_t CPURead16(uint32_t a) {
  if(a + 1 < MEM_SIZE) {
    return CPURead16_Unchecked(a);
  } else {
    // TODO: Exception
    return 0;
  }
}

uint32_t CPURead16SE32(uint32_t a) {
  if(a + 1 < MEM_SIZE) {
    uint32_t h = CPURead16_Unchecked(a);
    if(h & 0x0000'8000)
      h |= 0xFFFF'0000;
    return h;
  } else {
    // TODO: Exception
    return 0;
  }
}

uint8_t CPURead8(uint32_t a) {
  if(a < MEM_SIZE) {
    return mem[a];
  } else {
    // TODO: Exception
    return 0;
  }
}

uint32_t CPURead8SE32(uint32_t a) {
  if(a < MEM_SIZE) {
    uint32_t b = mem[a];
    if(b & 0x0000'0080)
      b |= 0xFFFF'FF00;
    return b;
  } else {
    // TODO: Exception
    return 0;
  }
}

static int32_t DecodeIMMU(uint32_t ins) {
  return ins & 0xFFFF'F000;
}

static int32_t DecodeIMMJ(uint32_t ins) {
  int32_t imm = 0;
  if(ins & 0x8000'0000)
    imm |= 0xFFF0'0000;
  imm |=  ins & 0x000F'F000;
  imm |= (ins & 0x0001'0000) >> 9;
  imm |= (ins & 0x07FE'0000) >> 20;
  return imm;
}

static int32_t DecodeIMMI(uint32_t ins) {
  int32_t imm = 0;
  if(ins & 0x8000'0000)
    imm |= 0xFFFF'F800;
  imm |= (ins & 0x7FF0'0000) >> 20;
  return imm;
}

static int32_t DecodeIMMS(uint32_t ins) {
  int32_t imm = 0;
  if(ins & 0x8000'0000)
    imm |= 0xFFFF'F800;
  imm |= (ins & 0x7E00'0000) >> 20;
  imm |= (ins & 0x0000'0F80) >> 7;
  return imm;
}

static int32_t DecodeIMMB(uint32_t ins) {
  int32_t imm = 0;
  if(ins & 0x8000'0000)
    imm |= 0xFFFF'F000;
  imm |= (ins & 0x7E00'0000) >> 20;
  imm |= (ins & 0x0000'0F00) >> 6;
  imm |= (ins & 0x0000'0080) << 4;
  return imm;
}

#define RD    uint32_t rd  = (ins & 0x0000'0F80) >> 7;
#define F3    uint32_t f3  = (ins & 0x0000'7000) >> 12;
#define RS1   uint32_t rs1 = (ins & 0x000F'8000) >> 15;
#define RS2   uint32_t rs2 = (ins & 0x01F0'0000) >> 20;
#define F7    uint32_t f7  = (ins & 0xFE00'0000) >> 25;
#define IMM_U int32_t  imm_u = DecodeIMMU(ins);
#define IMM_J int32_t  imm_j = DecodeIMMJ(ins);
#define IMM_I int32_t  imm_i = DecodeIMMI(ins);
#define IMM_S int32_t  imm_s = DecodeIMMS(ins);
#define IMM_B int32_t  imm_b = DecodeIMMB(ins);

#define sreg (int32_t)reg
#define uimm (uint32_t)imm

unsigned CPUStep(unsigned cycles) {
  if(cycles == 0)
    return 0;
  if(reg[PC] & 3) // TODO: Misaligned
    goto invalid;
  uint32_t ins = CPURead32(reg[PC]);
  uint32_t opc = ins & 0x7F;

  if((opc & 0b11) != 0b11)
    goto invalid;
  RD RS1 RS2
  switch(opc >> 2) { 
  case 0b01101: {IMM_U reg[rd] = imm_u; reg[PC] += 4; }         break; // lui
  case 0b00101: {IMM_U reg[rd] = reg[PC] + imm_u; reg[PC] += 4;}break; // auipc
  case 0b11011: {IMM_J reg[rd] = reg[PC] + 4; reg[PC] += imm_j;}break; // jal
  case 0b11001: {IMM_I reg[rd] = reg[PC] + 4; reg[PC] = (reg[rs1] + imm_i) & ~1; } break; // jalr
  case 0b11000: {IMM_B F3 
    switch(f3) {
    case 0b000: reg[PC] += reg [rs1] == reg [rs2] ? imm_b : 4;  break; // beq
    case 0b001: reg[PC] += reg [rs1] != reg [rs2] ? imm_b : 4;  break; // bne
    case 0b100: reg[PC] += sreg[rs1] <  sreg[rs2] ? imm_b : 4;  break; // blt
    case 0b101: reg[PC] += sreg[rs1] >= sreg[rs2] ? imm_b : 4;  break; // bge
    case 0b110: reg[PC] += reg [rs1] <  reg [rs2] ? imm_b : 4;  break; // bltu
    case 0b111: reg[PC] += reg [rs1] >= reg [rs2] ? imm_b : 4;  break; // bgeu
    default: goto invalid;
    }
  } break;
  case 0b00000: {IMM_I F3
    switch(f3) {
    case 0b000: reg[rd] = CPURead8SE32 (reg[rs1] + imm_i);      break; // lb
    case 0b001: reg[rd] = CPURead16SE32(reg[rs1] + imm_i);      break; // lh
    case 0b010: reg[rd] = CPURead32    (reg[rs1] + imm_i);      break; // lw
    case 0b100: reg[rd] = CPURead8     (reg[rs1] + imm_i);      break; // lbu
    case 0b101: reg[rd] = CPURead16    (reg[rs1] + imm_i);      break; // lhu
    default: goto invalid;
    } reg[PC] += 4;
  } break;
  case 0b01000: {IMM_S F3
    switch(f3) {
    case 0b000: CPUWrite8 (reg[rs1] + imm_s, reg[rs2]);         break; // sb
    case 0b001: CPUWrite16(reg[rs1] + imm_s, reg[rs2]);         break; // sh
    case 0b010: CPUWrite32(reg[rs1] + imm_s, reg[rs2]);         break; // sw
    default: goto invalid;
    } reg[PC] += 4;
  } break;
  case 0b00100: {IMM_I F3
    switch(f3) {
    case 0b000: reg[rd] = reg [rs1] +  imm_i;                   break; // addi
    case 0b001: reg[rd] = reg [rs1] << rs2;                     break; // slli
    case 0b010: reg[rd] = sreg[rs1] <  imm_i;                   break; // slti
    case 0b011: reg[rd] = reg [rs1] <  (uint32_t)imm_i;         break; // sltiu
    case 0b100: reg[rd] = reg [rs1] &  imm_i;                   break; // xori
    case 0b101: { F7
      if(f7 & 0x40) reg[rd] = reg [rs1] >> rs2;                        // srli
      else          reg[rd] = sreg[rs1] >> rs2;                        // srai
    } break;
    case 0b110: reg[rd] = reg[rs1] | imm_i;                     break; // ori
    case 0b111: reg[rd] = reg[rs1] & imm_i;                     break; // andi
    } reg[PC] += 4;
  } break;
  case 0b01100: {F3
    switch(f3) {
    case 0b000: { F7
      if(f7 & 0x40) reg[rd] = reg[rs1] - reg[rs2];                     // sub
      else          reg[rd] = reg[rs1] + reg[rs2];                     // add
    } break;
    case 0b001: reg[rd] = reg [rs1]  << (reg [rs2] & 0x1F);     break; // sll
    case 0b010: reg[rd] = sreg[rs1]  <   sreg[rs2];             break; // slt
    case 0b011: reg[rd] = reg [rs1]  <   reg [rs2];             break; // sltu
    case 0b100: reg[rd] = reg [rs1]  ^   reg [rs2];             break; // xor
    case 0b101: { F7
      if(f7 & 0x40) reg[rd] =  reg[rs1] >> (reg[rs2] & 0x1f);          // srl
      else          reg[rd] = sreg[rs1] >> (reg[rs2] & 0x1f);          // sra
    } break;
    case 0b110: reg[rd] = reg[rs1] | reg[rs2];                  break; // or
    case 0b111: reg[rd] = reg[rs1] & reg[rs2];                  break; // and
    } reg[PC] += 4;
  }
  default:
  invalid:
    return cycles;
  }
  reg[0] = 0;

  return CPUStep(cycles - 1);
}

int AssembleScan(const char *str, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  const char *f = fmt;
  const char *s = str;
  while(*f) {
    // Any number of spaces
    if(*f == ' ') {
      while(*s && isspace(*s))
        s++;
      f++;
    }

    // Format specifier
    else if(*f == '%') {
      if(!*s)
        return -1;
      if(f[1] == 'r') { // Register name
        const char *end = s;
        while(*end && isalnum(*end))
          end++;
        size_t len = end - s;
        if(len == 0 || len >= 16)
          return -1;
        char tmp[16];
        strncpy(tmp, s, len);
        tmp[len] = 0;
        int idx = 0;
        for(; idx < NUM_BASE_REGS; idx++)
          if(!strcmp(tmp, reg_names[idx]) || !strcmp(tmp, reg_anames[idx]))
            break;
        if(idx == NUM_BASE_REGS)
          return -1;
        *va_arg(args, uint32_t*) = idx;
        s = end;
      }

      else if(f[1] == 'i') { // Immediate value
        int32_t imm = 0;
        if(!strncmp(s, "0x", 2)) {
          s += 2;
          int i = 0;
          for(; i < 8; i++) {
            const char *hex = "0123456789ABCDEF";
            const char *idx = strchr(hex, toupper(*s));
            if(idx) {
              s++;
              imm <<= 4;
              imm |= (idx - hex) & 0xF;
            } else
              break;
          }
          if(i == 0)
            return -1;
          *va_arg(args, int32_t*) = imm;
        } else {
          bool neg = false;
          if(*s == '-') {
            neg = true;
            s++;
          }
          int64_t val = 0;
          while(isdigit(*s)) {
            val *= 10;
            val += *s - '0';
            s++;
          }
          if(neg)
            val = -val;
          if(val > UINT32_MAX || val < INT32_MIN)
            return -1;
          *va_arg(args, int32_t*) = val;
        } 
      }

      else
        return -1;

      f += 2;
    }

    // Literal character
    else {
      if(!*s || *f != *s)
        goto done;
      else
        s++;
      f++;
    }
  }

done:
  va_end(args);
  if(!*f && !*s)
    return 0;
  return -1;
}

uint32_t Assemble(const char *line) {
  enum { U, J, I, S, B, R } type;
  uint32_t opc = 0;
  uint32_t rd = 0, f3 = 0, rs1 = 0, rs2 = 0, f7 = 0;
  int32_t imm = 0;

  #define S(FMT, ...) if(AssembleScan(line, " "FMT" " __VA_OPT__(,)__VA_ARGS__) == 0)
  #define UTYPE(MNE, OPC) S(#MNE" %r , %i", &rd, &imm) { type = U; opc = OPC; }
  #define JTYPE(MNE, OPC) S(#MNE" %r , %i", &rd, &imm) { type = J; opc = OPC; }
  #define ITYPE(MNE, OPC) S(#MNE" %r , %i ( %r )", &rd, &imm, &rs1) { type = I; opc = OPC; }
  #define BTYPE(MNE, OPC, F3) S(#MNE"%r , %r , %i", &rs1, &rs2, &imm) { type = B; opc = OPC; f3 = F3; }
  #define LTYPE(MNE, OPC, F3) S(#MNE"%r , %r , %i", &rd, &rs1, &imm) { type = I; opc = OPC; f3 = F3; }
  #define STYPE(MNE, OPC, F3) S(#MNE"%r , %i(%r)", &rs2, &imm, &rs1) { type = S; opc = OPC; f3 = F3; }
  #define KTYPE(MNE, OPC, F3, F7) S(#MNE"%r , %r , %i", &rd, &rs1, &imm) { type = I; opc = OPC; f3 = F3; f7 = F7; }
  #define RTYPE(MNE, OPC, F3, F7) S(#MNE"%r , %r , %r", &rd, &rs1, &rs2) { type = R; opc = OPC; f3 = F3; f7 = F7; }
  #define ETYPE(MNE, OPC, IMM) S(#MNE) { type = I; opc = OPC, rd = rs1 = 0; imm = IMM; }
       UTYPE(lui,   0b0110111)
  else UTYPE(auipc, 0b0010111)
  else JTYPE(jal,   0b1101111)
  else ITYPE(jalr,  0b1100111)
  else BTYPE(beq,   0b1100011, 0b000)
  else BTYPE(bne,   0b1100011, 0b001)
  else BTYPE(blt,   0b1100011, 0b100)
  else BTYPE(bge,   0b1100011, 0b101)
  else BTYPE(bltu,  0b1100011, 0b110)
  else BTYPE(bgeu,  0b1100011, 0b111)
  else LTYPE(lb,    0b0000011, 0b000)
  else LTYPE(lh,    0b0000011, 0b001)
  else LTYPE(lw,    0b0000011, 0b010)
  else LTYPE(lbu,   0b0000011, 0b100)
  else LTYPE(lhu,   0b0000011, 0b101)
  else STYPE(sb,    0b0100011, 0b000)
  else STYPE(sh,    0b0100011, 0b001)
  else STYPE(sw,    0b0100011, 0b010)
  else LTYPE(addi,  0b0010011, 0b000)
  else LTYPE(slti,  0b0010011, 0b010)
  else LTYPE(sltiu, 0b0010011, 0b011)
  else LTYPE(xori,  0b0010011, 0b100)
  else LTYPE(ori,   0b0010011, 0b110)
  else LTYPE(andi,  0b0010011, 0b111)
  else KTYPE(slli,  0b0010011, 0b001, 0b0000000)
  else KTYPE(srli,  0b0010011, 0b101, 0b0000000)
  else KTYPE(srai,  0b0010011, 0b101, 0b0100000)
  else RTYPE(add,   0b0110011, 0b000, 0b0000000)
  else RTYPE(sub,   0b0110011, 0b000, 0b0100000)
  else RTYPE(sll,   0b0110011, 0b001, 0b0000000)
  else RTYPE(slt,   0b0110011, 0b010, 0b0000000)
  else RTYPE(sltu,  0b0110011, 0b011, 0b0000000)
  else RTYPE(xor,   0b0110011, 0b100, 0b0000000)
  else RTYPE(srl,   0b0110011, 0b101, 0b0000000)
  else RTYPE(sra,   0b0110011, 0b101, 0b0100000)
  else RTYPE(or,    0b0110011, 0b110, 0b0000000)
  else RTYPE(and,   0b0110011, 0b111, 0b0000000)
  else ETYPE(ecall, 0b1110011, 0)
  else ETYPE(ebreak,0b1110011, 1)
  else return 0;
  #undef ETYPE
  #undef RTYPE
  #undef KTYPE
  #undef STYPE
  #undef LTYPE
  #undef BTYPE
  #undef ITYPE
  #undef JTYPE
  #undef UTYPE
  #undef S

  uint32_t imm2 = 0;
  switch(type) {
  case U:
    imm2 = imm << 12;
    break;
  case J:
    imm2 = 0;
    if(imm & 0x8000'0000)
      imm2 |= 0xFFF0'0000;
    imm2 |= imm & 0x000F'F000;
    imm2 |= (imm & 0x0000'0800) << 8;
    imm2 |= (imm & 0x7FFE'0000) >> 20;
    break;
  case I:
    imm2 = 0;
    if(imm & 0x8000'0000)
      imm2 |= 0xFFFF'F000;
    imm2 |= (imm & 0x7FF0'0000) >> 20;
    break;
  default:
    break;
  }

  return opc | imm2 |
    (rd  & 0x1F) << 7 |
    (f3  & 0x03) << 12 |
    (rs1 & 0x1F) << 15 |
    (rs2 & 0x1F) << 20 |
    (f7  & 0x3F) << 25;
}

int Unassemble(uint32_t ins, char buf[64]) {
  const char *(*r)[NUM_REGS] = &reg_anames;
  uint32_t opc = ins & 0x7F;
  if((opc & 0b11) != 0b11)
    return -1;
  RD F3 RS1 RS2 F7 IMM_U IMM_J IMM_I IMM_S IMM_B
  switch(opc >> 2) {
  #define P(...) do { snprintf(buf, 64, __VA_ARGS__); return 0; } while(0);
  #define U(MNE) P("%-5s %s,%d", #MNE, (*r)[rd], imm_u)
  #define J(MNE) P("%-5s %s,%d", #MNE, (*r)[rd], imm_j)
  #define I(MNE) P("%-5s %s,%d(%s)", #MNE, (*r)[rd], imm_i, (*r)[rs1])
  #define L(MNE) P("%-5s %s,%s,%d", #MNE, (*r)[rd], (*r)[rs1], imm_i)
  #define S(MNE) P("%-5s %s,%d(%s)", #MNE, (*r)[rs2], imm_s, (*r)[rs1])
  #define B(MNE) P("%-5s %s,%s,%d", #MNE, (*r)[rs1], (*r)[rs2], imm_b)
  case 0b01101: U(lui)
  case 0b00101: U(auipc)
  case 0b11011: J(jal)
  case 0b11001: I(jalr)
  case 0b11000:
    switch(f3) {
    case 0b000: B(beq)
    case 0b001: B(bne)
    case 0b100: B(blt)
    case 0b101: B(bge)
    case 0b110: B(bltu)
    case 0b111: B(bgeu)
    default: return -1;
    }
  case 0b00000:
    switch(f3) {
    case 0b000: I(lb)
    case 0b001: I(lh)
    case 0b010: I(lw)
    case 0b100: I(lbu)
    case 0b101: I(lhu)
    default: return -1;
    }
  case 0b01000:
    switch(f3) {
    case 0b000: S(sb)
    case 0b001: S(sh)
    case 0b010: S(sw)
    default: return -1;
    }
  case 0b00100:
    switch(f3) {
    case 0b000: L(addi)
    case 0b001: L(slli)
    case 0b010: L(slti)
    case 0b011: L(sltiu)
    case 0b100: L(xori)
    case 0b101:
      if(f7 & 0x40) L(srli)
      else          L(srai)
    case 0b110: L(ori)
    case 0b111: L(andi)
    }
  #undef B
  #undef S
  #undef L
  #undef I
  #undef J
  #undef U
  #undef P
  }
  return -1;
}
