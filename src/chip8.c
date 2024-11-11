#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "chip8.h"

/**
  * Initialise memory and registers to their starting values.
  *
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
 * Open a ROM and place it into the CHIP-8 memory. Takes
 * the file path of the ROM.
 */
void open_rom(char* rom){
  FILE *fptr;

  if((fptr = fopen(rom, "rb")) == NULL){
    perror("fopen");
    exit(1);
  }

  // Get the size of the file
  fseek(fptr, 0, SEEK_END);
  size_t size = ftell(fptr);
  fseek(fptr, 0, SEEK_SET);

  // Store the file into memory
  fread(memory + 0x200, sizeof(uint16_t), size, fptr);
}

/**
  * Simulate a single cycle. This involves fetching the current
  * instruction, decoding it and execution.
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
          // TODO: clear display
          break;

        // 00EE - RET
        case(0x00EE):
          PC = stack[SP];
          SP--;
          break;

        default:
          break;
      }

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
      // TODO: Drawing
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

    default:
      printf("Error: Invalid OPCODE 0x%4x\n", opcode);
      break;
  }
}

/**
  * Displays the current state of the registers, stack, PC, etc.
  */
void print_info(){
  printf("\r");
  for(int i = 0; i < 16; i++){
    printf("V%x: %u ", i, v[i]);
  }
  printf("| PC: %4x", PC);
  fflush(stdout);
}

int main(int argc, char *argv[]){
  if(argc < 2){
    printf("Usage: ./chip8 [rom_path]\n");
    exit(1);
  }

  init_chip8();
  open_rom(argv[1]);
  
  // Main emulation loop; we execute until the PC exceeds memory
  while(PC <= 0xFFF){
    print_info();
    cycle();
    sleep(1);
  }

  printf("Program has finished execution.\n");
  return 0;
}
