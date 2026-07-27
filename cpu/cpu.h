#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include "alu.h"
#include "bus.h"

#define NUM_REGS 16

typedef struct {
    // Registers (cached)
    uint16_t R[NUM_REGS];
    uint16_t PC;
    uint16_t SP;
    uint16_t IR;
    ALUFlags flags;

    // Memory (bus owns the CCEL planes)
    Bus bus;

    // ALU
    ALU alu;

    // Execution state
    uint8_t running;
    uint8_t halted;
    uint64_t cycles;
    uint8_t cmp_signed;

    // Dirty flags (for lazy sync)
    uint8_t R_dirty[NUM_REGS];
    uint8_t PC_dirty;
    uint8_t SP_dirty;
    uint8_t flags_dirty;
} CPU;

// Initialization
void cpu_init(CPU* cpu);
void cpu_free(CPU* cpu);

// Program Loading
void cpu_load_program(CPU* cpu, const uint16_t* program, uint16_t size);
void cpu_load_hex(CPU* cpu, const char* hex_string);

// Execution
void cpu_step(CPU* cpu);
void cpu_run(CPU* cpu);
void cpu_reset(CPU* cpu);

// Register Access
void cpu_sync_registers(CPU* cpu);
void cpu_sync_all(CPU* cpu);

// Debug
void cpu_dump_state(CPU* cpu);
void cpu_dump_registers(CPU* cpu);

#endif // CPU_H
