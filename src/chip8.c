#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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
  // TODO:
}

int main(int argc, char *argv[]){
  init_chip8(); 
  cycle(); 
  return 0;
}
