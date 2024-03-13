#include "CPU.h"
#include "linenoise.h"
#include "monitor.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char *vlinenoise(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  char *line = linenoise("");
  for(char *c = line; c && *c; c++)
    if(isspace(*c))
      *c = ' ';
  return line;
}


#define PSCAN(STR, FMT, ...) (scann = -1, sscanf(STR, " "FMT" %n" __VA_OPT__(,)__VA_ARGS__, &scann), scann > 0)
#define WSCAN(STR, FMT, ...) (PSCAN(STR, FMT __VA_OPT__(,)__VA_ARGS__) && !STR[scann])


int FormatRegisterValue(int idx, char str[10]) {
  if(idx >= 0 && idx < NUM_REGS) {
    snprintf(str, 10, "%04X:%04X", reg[idx] >> 16, reg[idx] & 0xFFFF);
    return 0;
  }
  return -1;
}


const char *FormatRegisterByIndex(int idx, char str[64]) {
  if(idx >= 0 && idx < NUM_REGS) {
    char val[10];
    FormatRegisterValue(idx, val);
    snprintf(str, 64, "%5s/%-5s %9s", reg_names[idx], reg_anames[idx], val);
    return str;
  }
  return "invalid register";
}


const char *FormatRegisterByName(const char *name, char str[64]) {
  return FormatRegisterByIndex(GetRegisterIndex(name), str);
}


void AssembleCommand(uint32_t s1) {
  char *line = NULL;
  while(s1 + 3 < MEM_SIZE) {
    free(line);
    line = vlinenoise("a %04X:%04X ", s1 >> 16, s1 & 0xFFFF);
    if(!line)
      break;

    int scann;
    if(WSCAN(line, "."))
      return;
    uint32_t ins = Assemble(line);
    if(ins) {
      CPUWrite32(s1, ins);
      s1 += 4;
    } else
      printf("syntax error\n");
  }
  free(line);
}


void BreakpointCommand(uint32_t s1, bool set) {
  if(s1 & 0x0000'0003 || s1 >= MEM_SIZE) {
    printf("b invalid instruction address\n");
    return;
  }
  breakpoint[s1 >> 2] = set;
}


void CompareCommand(uint32_t s1, uint32_t s2, uint32_t size) {
  for(; size && s1 < MEM_SIZE && s2 < MEM_SIZE; size--, s1++, s2++) {
    if(mem[s1] != mem[s2])
      printf("c %04X:%04X %02X    %04X:%04X %02X\n",
          s1 >> 16, s1 & 0xFFFF, mem[s1],
          s2 >> 16, s2 & 0xFFFF, mem[s2]);
  }
  if(size != 0)
    printf("out of range");
}


void DumpCommand(uint32_t s1, uint32_t size) {
  for(uint32_t a = s1 & 0xFFFF'FFF0; a < MEM_SIZE && a < s1 + size; a += 16) {
    printf("d %04X:%04X  ", a >> 16, a & 0xFFFF);
    for(uint32_t b = a, count = 0; count < 16; b++, count++) {
      if(b < s1 || b >= s1 + size || b >= MEM_SIZE)
        printf("   ");
      else
        printf("%02X ", mem[b]);
      if(count == 7)
        printf("  ");
    }
    printf("  |");
    for(uint32_t b = a, count = 0; count < 16; b++, count++) {
      if(b < s1 || b >= s1 + size || b >= MEM_SIZE || !isprint(mem[b]))
        printf(" ");
      else
        printf("%c", mem[b]);
    }
    printf("|\n");
  }
}


void EnterCommand(uint32_t s1, const char *line) {
  int n = -1, n2;
  sscanf(line, " e %*i %n", &n);
  if(n < 0)
    printf("syntax error\n");
  uint32_t byte;
  while(n2 = -1, sscanf(line + n, "%2X %n", &byte, &n2), n2 > 0) {
    if(s1 >= MEM_SIZE) {
      printf("out of range");
      return;
    }
    mem[s1] = byte;
    n += n2;
    s1++;
  }
}


void FillCommand(uint32_t s1, uint32_t size, uint32_t byte) {
  for(; s1 < MEM_SIZE && size > 0; s1++, size--)
    mem[s1] = byte;
  if(size != 0)
    printf("out of range");
}


void GoCommand(uint32_t s1) {
  if(s1 & 0x0000'0003 || s1 + 3 >= MEM_SIZE) {
    printf("out of range");
    return;
  }
  reg[PC] = s1;
}


void LoadCommand(uint32_t s1, const char *rest) {
  char filename[512];
  int n;

  if(s1 >= MEM_SIZE) {
    printf("out of range\n");
    return;
  }

  if((n = -1, sscanf(rest, " \"%511[^\"] %n", filename, &n), n > 0 && !rest[n]) ||
     (n = -1, sscanf(rest, " '%511[^'] %n", filename, &n), n > 0 && !rest[n]) ||
     (n = -1, sscanf(rest, " %511s %n", filename, &n), n > 0 && !rest[n])) {
    FILE *f = fopen(filename, "rb");
    if(!f) {
      printf("can't open file '%s'\n", filename);
      return;
    }
    size_t bytes = fread(&mem[s1], 1, MEM_SIZE - s1, f);
    printf("%zu bytes read\n", bytes);
    if(!feof(f))
      printf("out of range\n");
    fclose(f);
  } else
    printf("syntax error\n");
}


void MoveCommand(uint32_t s1, uint32_t s2, uint32_t size) {
  if(s1 + size >= MEM_SIZE || s2 + size >= MEM_SIZE) {
    printf("out of range\n");
    return;
  }
  memmove(&mem[s1], &mem[s2], size);
}


void RegisterCommand1() {
  for(int i = 0, j = NUM_REGS / 2; i < NUM_REGS && j < NUM_REGS; i++, j++) {
    char buf[64];
    if(i < NUM_REGS / 2)
      printf("%s    ", FormatRegisterByIndex(i, buf));
    if(j < NUM_REGS)
      printf("%s", FormatRegisterByIndex(j, buf));
    printf("\n");
  }
}


void RegisterCommand2(const char *str1) {
  char buf[64];
  printf("%s\n", FormatRegisterByName(str1, buf));
}


void RegisterCommand3(const char *str1, uint32_t s1) {
  int idx = GetRegisterIndex(str1);
  if(idx < 0) {
    printf("invalid register '%s'\n", str1);
    return;
  }
  reg[idx] = s1;
  char buf[64];
  printf("%s\n", FormatRegisterByIndex(idx, buf));
}


void StepCommand(uint32_t s1) {
  unsigned remaining = CPUStep(s1);
  if(remaining != 0)
    printf("break\n");
}


void UnassembleCommand(uint32_t s1, uint32_t size) {
  int64_t low = s1 - size * 4;
  if(low < 0)
    low = 0;
  int64_t high = s1 + size * 4;
  if(high >= MEM_SIZE)
    high = MEM_SIZE - 1;

  for(uint32_t a = low; a <= high; a += 4) {
    uint32_t ins = CPURead32(a);
    char buf[64];
    printf("%04X:%04X ", a >> 16, a & 0xFFFF);
    printf("%08X", ins);
    if(a == reg[PC])
      printf(" > ");
    else
      printf("   ");
    if(!Unassemble(ins, buf))
      printf("%s\n", buf);
    else
      printf("invalid instruction\n");
  }
}


void RunMonitor() {
  char *line = NULL;
  while(1) {
    free(line);
    line = linenoise("> ");
    if(line == NULL)
      break;
    linenoiseHistoryAdd(line);

    [[maybe_unused]] char     str[3][64];
    [[maybe_unused]] int64_t  i64[3];
    [[maybe_unused]] uint64_t u64[3];
    [[maybe_unused]] int32_t  i32[3];
    [[maybe_unused]] uint32_t u32[3];

    // ? help
    // a assemble   s1
    // b break      s1
    // c compare    s1 s2 size
    // d dump       s1 size
    // e enter      start
    // f fill       s1 size value
    // g go         start
    // l load       address file
    // m move       s1 s2 size
    // q quit
    // r register   reg [=value]
    // s step       [instructions]
    // u unassemble s1 size
    // w write      range file

    int scann;
         if(WSCAN(line, "a 0x%X",         &u32[0]))                    AssembleCommand   (u32[0]);
    else if(WSCAN(line, "b +0x%X",        &u32[0]))                    BreakpointCommand (u32[0], true);
    else if(WSCAN(line, "b -0x%X",        &u32[0]))                    BreakpointCommand (u32[0], false);
    else if(WSCAN(line, "c 0x%X 0x%X %i", &u32[0], &u32[1], &u32[2]))  CompareCommand    (u32[0], u32[1], u32[2]);
    else if(WSCAN(line, "d 0x%X %i",      &u32[0], &u32[1]))           DumpCommand       (u32[0], u32[1]);
    else if(PSCAN(line, "e 0x%X",         &u32[0]))                    EnterCommand      (u32[0], line);
    else if(WSCAN(line, "f 0x%X %i %i",   &u32[0], &u32[1], &u32[2]))  FillCommand       (u32[0], u32[1], u32[2]);
    else if(WSCAN(line, "g"))                                          GoCommand         (reg[PC]);
    else if(WSCAN(line, "g 0x%X",         &u32[0]))                    GoCommand         (u32[0]);
    else if(PSCAN(line, "l 0x%X",         &u32[0]))                    LoadCommand       (u32[0], line + scann);
    else if(WSCAN(line, "m 0x%X 0x%X %i", &u32[0], &u32[1], &u32[1]))  MoveCommand       (u32[0], u32[1], u32[2]);
    else if(WSCAN(line, "q"))                                          return;
    else if(WSCAN(line, "r"))                                          RegisterCommand1  ();
    else if(WSCAN(line, "r %[^ =]",       str[0]))                     RegisterCommand2  (str[0]);
    else if(WSCAN(line, "r %[^ =] = %i",  str[0], &u32[0]))            RegisterCommand3  (str[0], u32[0]);
    else if(WSCAN(line, "s"))                                          StepCommand       (1);
    else if(WSCAN(line, "s %i",           &u32[0]))                    StepCommand       (u32[0]);
    else if(WSCAN(line, "u pc %u",        &u32[0]))                    UnassembleCommand (reg[PC], u32[0]);
    else if(WSCAN(line, "u %i %u",        &u32[0], &u32[1]))           UnassembleCommand (u32[0], u32[1]);
    else                                                               printf("invalid command\n");
    #undef S
  }
  free(line);
}
