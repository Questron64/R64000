#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <SDL.h>
#include <SDL_image.h>
#include "CPU.h"
#include "monitor.h"

SDL_Window *debugWindow;
SDL_Renderer *debugRenderer;
SDL_Texture *debugFont;

void _Noreturn Die() {
  exit(EXIT_FAILURE);
}

#define LOG(FMT,...)\
  fprintf(stderr, "%s:%d %s "FMT"\n",\
  __FILE__, __LINE__, __func__\
  __VA_OPT__(,)__VA_ARGS__)
#define LOG_AND(L, A)\
  do {\
    LOG L;\
    A;\
  } while(0)
#define SDL_LOG() LOG("%s", SDL_GetError())
#define SDL_LOG_AND(D) LOG_AND(("%s", SDL_GetError()), D)

int CPU_Load(const char *filename, uint32_t baseAddr) {
  int result = -1;
  FILE *f = NULL;

  f = fopen(filename, "rb");
  if(!f)
    LOG_AND(("Could not open '%s'", filename), goto done);
  int maxSize = MEM_SIZE - baseAddr;
  if(maxSize <= 0)
    LOG_AND(("baseAddr is past end of RAM"), goto done);
  fread(&mem[baseAddr], 1, maxSize, f);
  if(!feof(f))
    LOG_AND(("Could not load whole file"), goto done);
  result = 0;
done:
  fclose(f);
  return result;
}

int main(int argc, char *argv[]) {
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
    SDL_LOG_AND(Die());
  {
    int fl = IMG_INIT_PNG;
    if((IMG_Init(fl) & fl) != fl)
      SDL_LOG_AND(Die());
  }

  Reset();
  if(CPU_Load(argv[1], 0x0002'0000))
    exit(EXIT_FAILURE);
  reg[0] = 0xDEAD'BEEF;
  reg[PC] = 0x0002'0000;
  RunMonitor();
}
