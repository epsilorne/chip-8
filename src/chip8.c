#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "chip8.h"

/* SDL Components */
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *canvas = NULL;

// Representation of the CHIP-8 display - each element is 'on' or 'off'
uint8_t display[CHIP8_HEIGHT][CHIP8_WIDTH];

/**
  * Initialise memory and registers to their starting values.
  */
void init_chip8(void){
  opcode = 0;
  
  memset(memory, 0, sizeof(memory));
  memset(v, 0, sizeof(v));
  I = 0;
  
  delay_timer = 0;
  sound_timer = 0;
  
  // Programs start at 0x200 in CHIP-8
  PC = 0x200;
  
  SP = 0;
  memset(stack, 0, sizeof(stack));

  // Init RNG
  srand(time(NULL));
}

/**
 * Open a ROM and place it into the CHIP-8 memory. Takes the file path of the ROM,
 * and returns a pointer to the opened ROM.
 */
FILE *open_rom(char* rom){
  FILE *fptr;

  if((fptr = fopen(rom, "rb")) == NULL){
    return NULL;
  }

  // Get the size of the file
  fseek(fptr, 0, SEEK_END);
  size_t size = ftell(fptr);
  fseek(fptr, 0, SEEK_SET);

  // Store the file into memory
  fread(memory + 0x200, sizeof(uint16_t), size, fptr);

  return fptr;
}

/** Update the display. This should only be called after a DRW instruction.
 */
void refresh_display(void){
  uint8_t *pixels;
  int pitch;

  if(SDL_LockTexture(canvas, NULL, (void**) &pixels, &pitch) != 0){
    fprintf(stderr, "SDL_LockTexture: %s\n", SDL_GetError());
  }

  // Update SDL display to match display[][]
  for(int y = 0; y < CHIP8_HEIGHT; y++){
    for(int x = 0; x < CHIP8_WIDTH; x++){
      pixels[y * pitch + x] = (display[y][x] != 0) ? 0xFF : 0x00;
    }
  }

  SDL_UnlockTexture(canvas);

  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, canvas, NULL, NULL);
  SDL_RenderPresent(renderer);
}


/**
  * Simulate a single cycle. This involves fetching the current instruction,
  * decoding it and execution.
  */
void cycle(void){
  // FETCH (and increment program counter)
  opcode = memory[PC] << 8 | memory[PC + 1]; 
  PC += 2;
  
  // DECODE 
  // Extract variables from the opcode 
  uint16_t nnn = opcode & 0x0FFF;
  uint8_t n = opcode & 0x000F;
  uint8_t kk = opcode & 0x00FF;
  
  uint8_t x = (opcode & 0x0F00) >> 8;
  uint8_t y = (opcode & 0x00F0) >> 4;

  // EXECUTE
  switch(opcode & 0xF000){
    // 0000 OPCODES
    case(0x0000):
      switch(opcode & 0x00FF){
        // 00E0 - CLS
        case(0x00E0):
          memset(display, 0, sizeof(display));
          break;

        // 00EE - RET
        case(0x00EE):
          PC = stack[SP];
          SP--;
          break;

        default:
          break;
      }
      break;

    // 1nnn - JP addr
    case(0x1000):
      PC = nnn;
      break;

    // 2nnn - CALL addr
    case(0x2000):
      SP++;
      stack[SP] = PC;
      PC = nnn;
      break;

    // 3xkk - SE Vx, byte
    case(0x3000):
      if(v[x] == kk){
        PC += 2;
      }
      break;

    // 4xkk- SNE Vx, byte
    case(0x4000):
      if(v[x] != kk){
        PC += 2;
      }
      break;

    // 5xy0 - SE Vx, Vy
    case(0x5000):
      if(v[x] == v[y]){
        PC += 2;
      }
      break;

    // 6xkk - LD Vx, byte
    case(0x6000):
      v[x] = kk;
      break;

    // 7xkk - ADD Vx, byte
    case(0x7000):
      v[x] = v[x] + kk;
      break;

    // 8000 OPCODES
    case(0x8000):
      switch(opcode & 0x000F){
        // 8xy0 - LD Vx, Vy
        case(0x0000):
          v[x] = v[y];
          break;

        // 8xy1 - OR Vx, Vy
        case(0x0001):
          v[x] = v[x] | v[y];
          break;

        // 8xy2 - AND Vx, Vy
        case(0x0002):
          v[x] = v[x] & v[y];
          break;

        // 8xy3 - XOR Vx, Vy
        case(0x0003):
          v[x] = v[x] ^ v[y];
          break;

        // 8xy4 - ADD Vx, Vy
        case(0x0004):
          uint16_t val = v[x] + v[y];
          // Set v[f] = carry (if overflow occurs)
          v[0xF] = val > 0xFF;
          v[x] = (uint8_t) val;
          break;

        // 8xy5 - SUB Vx, Vy
        case(0x0005):
          // Set v[f] = NOT borrow (if negative result)
          v[0xF] = v[x] > v[y];
          v[x] = v[x] - v[y];
          break;

        // 8xy6 - SHR Vx {, Vy}
        case(0x0006):
          v[0xF] = v[x] & 0x1;
          v[x] = v[x] >> 1;
          break;

        // 8xy7 - SUBN Vx, Vy
        case(0x0007):
          v[0xF] = v[y] > v[x];
          v[x] = v[y] - v[x];
          break;

        // 8xyE - SHL Vx {, Vy}
        case(0x000E):
          v[0xF] = v[x] >> 7;
          v[x] = v[x] << 1;
          break;

        default:
          break;
      }
      break;

    // 9xy0 - SNE Vx, Vy
    case(0x9000):
      if(v[x] != v[y]){
        PC += 2;
      }
      break;

    // Annn - LD I, addr
    case(0xA000):
      I = nnn;
      break;

    // Bnnn - JP V0, addr
    case(0xB000):
      PC = nnn + v[0];
      break;

    // Cxkk - RND Vx, byte
    case(0xC000):
      v[x] = ((uint8_t) rand() % 256) & kk;
      break;

    // Dxyn - DRW Vx, Vy, nibble
    case(0xD000):
      v[0xF] = 0;
      uint8_t sprite_row;

      // For each 'row' of the sprite...
      for(uint16_t i = 0; i < n; i++){
        sprite_row = memory[I + i];

        // Go through each bit of the row and set the display bit
        for(uint16_t j = 0, k = 1; j < 8; j++, k++){

          // Obtain the corresponding display bit
          uint8_t *display_bit = &display[(v[y] + i) % CHIP8_HEIGHT][(v[x] + (7 - j)) % CHIP8_WIDTH];

          // The value to be 'drawn'
          uint8_t value = (sprite_row & (1 << k));

          // If a collision occurs, set V[F]
          if(*display_bit & value){
            v[0xF] = 1;
          }

          *display_bit = value;
        }
      }

      refresh_display();
      break;

    // E000 OPCODES
    case(0xE000):
      switch(opcode & 0x00FF){
        // Ex9E - SKP Vx
        case(0x009E):
          // TODO: Keyboard
          break;

        // ExA1 - SKNP Vx
        case(0x00A1):
          // TODO: Keyboard
          break;

        default:
          break;
      }
      break;

    // F000 OPCODES
    case(0xF000):
      switch(opcode & 0x00FF){
        // Fx07 - LD Vx, DT
        case(0x0007):
          v[x] = delay_timer;
          break;

        // Fx0A - LD Vx, K
        case(0x000A):
          // TODO: Keyboard
          break;

        // Fx15 - LD DT, Vx
        case(0x0015):
          delay_timer = v[x];
          break;

        // Fx18 - LD ST, Vx
        case(0x0018):
          sound_timer = v[x];
          break;

        // Fx1E - ADD I, Vx
        case(0x001E):
          I = I + v[x];
          break;

        // Fx29 - LD F, Vx
        case(0x0029):
          // TODO: Sprites
          break;

        // Fx33 - LD B, Vx
        case(0x0033):
          memory[I % MEMSIZE] = v[x] / 100;
          memory[(I + 1) % MEMSIZE] = (v[x] / 10) % 10;
          memory[(I + 2) % MEMSIZE] = v[x] % 10;
          break;

        // Fx55 - LD [I], Vx
        case(0x0055):
          for(uint8_t i = 0; i <= x; i++){
            memory[(I + i) % MEMSIZE] = v[i];
          }
          break;

        // Fx65 - LD Vx, [I]
        case(0x0065):
          for(uint8_t i = 0; i <= x; i++){
            v[i] = memory[(I + i) % MEMSIZE];
          }
          break;

        default:
          break;
      }
      break;

    default:
      printf("Error: Invalid OPCODE 0x%4x\n", opcode);
      break;
  }
}

/**
  * Displays the current state of the registers, stack, PC, etc.
  */
void print_info(void){
  printf("\r");
  for(int i = 0; i < 16; i++){
    printf("V%x: %u ", i, v[i]);
  }
  printf("| PC: %4x", PC);
  fflush(stdout);
}

/**
  * Initialises SDL display. Takes a char* pointer for the window title (e.g.
  * name of the ROM loaded) and a SDL_Window.
  */
void init_SDL(char* title){
  if(SDL_Init(SDL_INIT_VIDEO) != 0){
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    exit(1);
  }

  if(SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN,
    &window, &renderer) < 0){
    fprintf(stderr, "SDL_CreateWindowAndRenderer: %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }

  if((canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, CHIP8_WIDTH, CHIP8_HEIGHT)) == NULL){
    fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }

  // Remove the file extension from title
  char *dot = strchr(title, '.');
  if(dot != NULL){
    *dot = '\0';
  }

  SDL_SetWindowTitle(window, title);

  SDL_SetRenderTarget(renderer, canvas);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}

int main(int argc, char *argv[]){
  if(argc < 2){
    printf("Usage: ./chip8 [rom_path]\n");
    exit(1);
  }

  FILE *rom = NULL;
  SDL_Event event;

  init_chip8();

  if((rom = open_rom(argv[1])) == NULL){
    perror("open_rom");
    exit(1);
  }

  init_SDL(argv[1]);

  // Main emulation loop; we execute until the PC exceeds memory
  while(PC <= 0xFFF){
    // Poll for any events
    // TODO: handle keyboard events
    while(SDL_PollEvent(&event)){
      if(event.type == SDL_QUIT){
        PC =  0xFFF;
        goto end;
      }
    }

    cycle();
    print_info();
  }

  end:
    SDL_DestroyTexture(canvas);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("\nReached the end of the ROM - exiting emulator...\n");
    fclose(rom);

  return 0;
}
