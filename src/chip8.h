#ifndef CHIP8_HEADER
#define CHIP8_HEADER

#include<stdint.h>

#define MEMSIZE 4096

uint16_t opcode;
uint8_t memory[MEMSIZE];

/* Registers */
uint8_t v[16];
uint16_t I;

uint16_t PC;

uint8_t delay_timer;
uint8_t sound_timer;

/* Stack */
uint16_t stack[16];
uint8_t SP;

#endif