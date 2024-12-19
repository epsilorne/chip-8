#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <SDL2/SDL.h>

#include "chip8.h"

/* SDL Components */
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *canvas = NULL;

// 3 bits for R, 3 bits for G, 2 bits for B
uint8_t fg_colour = 0b10010010;
uint8_t bg_colour = 0b01001001;

// Representation of the CHIP-8 display - each element is 'on' or 'off'
uint8_t display[CHIP8_HEIGHT][CHIP8_WIDTH];

// CPU frequency in Hz
uint8_t clock_freq;

// Value of keys from the previous frame so we can detect key releases
uint16_t prev_keys;

/**
  * Initialise memory and registers to their starting values.
  */
void init_chip8(uint8_t freq){
  clock_freq = freq;

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

  // Init font sprites
  uint8_t fonts[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
  };
  memcpy(memory, fonts, sizeof(fonts));

  keys = prev_keys = 0;
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
  fread(memory + 0x200, sizeof(uint8_t), size, fptr);

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
      pixels[y * pitch + x] = (display[y][x] != 0) ? fg_colour : bg_colour;
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
        uint8_t flag;
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
          v[x] = (uint8_t) val;

          v[0xF] = val > 0xFF;
          break;

        // 8xy5 - SUB Vx, Vy
        case(0x0005):
          flag = v[x] >= v[y];
          v[x] = v[x] - v[y];

          v[0xF] = flag;
          break;

        // 8xy6 - SHR Vx {, Vy}
        case(0x0006):
          flag = v[x] & 0x1;
          v[x] = v[x] >> 1;

          v[0xF] = flag;
          break;

        // 8xy7 - SUBN Vx, Vy
        case(0x0007):
          flag = v[y] >= v[x];
          v[x] = v[y] - v[x];

          v[0xF] = flag;
          break;

        // 8xyE - SHL Vx {, Vy}
        case(0x000E):
          flag = v[x] >> 7;
          v[x] = v[x] << 1;

          v[0xF] = flag;
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
      uint8_t x_pos = v[x] % CHIP8_WIDTH;
      uint8_t y_pos = v[y] % CHIP8_HEIGHT;

      // For each 'row' of the sprite...
      for(uint16_t i = 0; i < n && y_pos + i < CHIP8_HEIGHT; i++){
        sprite_row = memory[I + i];

        // Go through each of the 8 bits
        for(uint16_t j = 0; j < 8 && x_pos + j < CHIP8_WIDTH; j++){
          // The value to be 'drawn'
          uint8_t value = sprite_row & (1 << (7 - j));

          // The 'values' in the display before and after drawing
          uint8_t *display_before = &display[y_pos + i][x_pos + j];
          uint8_t display_after = *display_before ^ value;

          // Check if a collision occured (value was non-zero, and is now zero)
          if(*display_before != 0 && (display_after != *display_before)){
            v[0xF] = 1;
          }

          // Set the display to be the XORed result
          *display_before = display_after;
        }
      }

      refresh_display();
      break;

    // E000 OPCODES
    case(0xE000):
      switch(opcode & 0x00FF){
        // Ex9E - SKP Vx
        case(0x009E):
          if(keys & (1 << v[x])){
            PC += 2;
          }
          break;

        // ExA1 - SKNP Vx
        case(0x00A1):
          if(!(keys & (1 << v[x]))){
            PC += 2;
          }
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
          // Decrement the PC so we can 'wait'
          PC -= 2;

          // Store the first recorded key to be released
          for(int i = 0; i < 16; i++){
            // We check if the key in the previous frame was 1 (pressed) and
            // 0 in the current frame (released)
            if((prev_keys & (1 << i)) && !(keys & (1 << i))){
              v[x] = i;

              // This allows us to move to the next instruction
              PC += 2;
              break;
            }
          }

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
          // Sprites are 5 bytes in length
          I = 5 * (0x00FF & v[x]);
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

  prev_keys = keys;
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
}

/* Converts a key (character) into the equivalent CHIP-8 keyboard key. If the
 * key is not within range, 0x10 is returned.
 */
uint16_t key_to_idx(char key){
  switch(key){
    case '1':
      return 0x1;
    case '2':
      return 0x2;
    case '3':
      return 0x3;
    case '4':
      return 0xC;
    case 'q':
      return 0x4;
    case 'w':
      return 0x5;
    case 'e':
      return 0x6;
    case 'r':
      return 0xD;
    case 'a':
      return 0x7;
    case 's':
      return 0x8;
    case 'd':
      return 0x9;
    case 'f':
      return 0xE;
    case 'z':
      return 0xA;
    case 'x':
      return 0x0;
    case 'c':
      return 0xB;
    case 'v':
      return 0xF;
    default:
      return 0x10;
  }
}

int main(int argc, char *argv[]){
  if(argc < 2){
    printf("Usage: ./chip8 <rom_path> [clock_freq]\n");
    exit(1);
  }

  FILE *rom = NULL;
  SDL_Event event;
  uint16_t key_idx;

  uint16_t freq = 500;
  if(argc == 3){
    freq = (uint16_t) atoi(argv[2]);
  }

  init_chip8(freq);

  if((rom = open_rom(argv[1])) == NULL){
    perror("open_rom");
    exit(1);
  }

  init_SDL(argv[1]);

  uint32_t last_timer_ticks = SDL_GetTicks();
  uint32_t last_cycle_ticks = SDL_GetTicks();
  uint32_t ticks = 0;

  // Main emulation loop
  while(1){
    // We continuously poll for inputs...
    while(SDL_PollEvent(&event)){
      switch(event.type){
        // In the event of a key press, we record it in the keys map
        case SDL_KEYDOWN:
          // Check if a valid key was pressed
          if((key_idx = key_to_idx(event.key.keysym.sym)) <= 0xF){
            keys |= 1 << (uint8_t) key_idx;
          }
          break;
        case SDL_KEYUP:
          // Check if a valid key was pressed
          if((key_idx = key_to_idx(event.key.keysym.sym)) <= 0xF){
            keys &= ~(1 << (uint8_t) key_idx);
          }
          break;
        case SDL_QUIT:
          goto end;
        default:
          break;
      }
    }

    ticks = SDL_GetTicks();

    // Independently execute instructions at freq Hz
    if(ticks - last_cycle_ticks >= 1000 / freq){
      cycle();
      last_cycle_ticks = SDL_GetTicks();
    }

    // Update delay/sound timers at 60Hz
    if(ticks - last_timer_ticks >= 1000 / 60){
      // Decrement delay/sound timer registers if non-zero
      if(delay_timer > 0){
        delay_timer--;
      }
      if(sound_timer > 0){
        sound_timer--;
      }
      last_timer_ticks = SDL_GetTicks();
    }

    // print_info();
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
