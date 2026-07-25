#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include <stdbool.h>
#include "ccel.h"

// --- Bus Configuration ---
#define BUS_ADDR_BITS   16
#define BUS_DATA_BITS   16
#define BUS_MEMORY_SIZE (1 << BUS_ADDR_BITS)  // 65536 cores

// --- Memory Layout ---
#define BUS_PROGRAM_BASE   0x0000
#define BUS_PROGRAM_SIZE   0x2000   // 8KB (8192 words)
#define BUS_DATA_BASE      0x2000
#define BUS_DATA_SIZE      0x1000   // 4KB (4096 words)
#define BUS_STACK_BASE     0x3000
#define BUS_STACK_SIZE     0x1000   // 4KB (4096 words)
#define BUS_FREE_BASE      0x4000
#define BUS_FREE_SIZE      0xC000   // 48KB

// --- System Variable Space (Fixed Slots) ---
#define BUS_SYSVAR_BASE    0xFFF0
#define BUS_SYSVAR_COUNT   16       // R0-R15
#define BUS_SYSVAR_PC      0xFFE0   // Program Counter
#define BUS_SYSVAR_SP      0xFFE1   // Stack Pointer
#define BUS_SYSVAR_FLAGS   0xFFE2   // ALU Flags (packed)
#define BUS_SYSVAR_IR      0xFFE3   // Instruction Register
#define BUS_SYSVAR_CYCLES  0xFFE4   // Cycle counter (64-bit, 4 words)

// --- Bus Structure ---
typedef struct {
    Ccel* memory;
    uint16_t size;
    struct {
        uint8_t depth;
        uint8_t row;
        uint8_t col;
    } decode_cache;
} Bus;

// --- Initialization ---
void bus_init(Bus* bus, Ccel* memory, uint16_t size);
void bus_free(Bus* bus);

// --- Core Operations ---
uint16_t bus_read(Bus* bus, uint16_t addr);
void bus_write(Bus* bus, uint16_t addr, uint16_t value);
void bus_read_block(Bus* bus, uint16_t addr, uint16_t* buffer, uint16_t words);
void bus_write_block(Bus* bus, uint16_t addr, const uint16_t* buffer, uint16_t words);

// --- Address Decoding ---
void bus_decode_address(uint16_t addr, uint8_t* depth, uint8_t* row, uint8_t* col);
uint16_t bus_encode_address(uint8_t depth, uint8_t row, uint8_t col);

// --- System Variable Access (Bus-level) ---
uint16_t bus_read_sysvar(Bus* bus, uint8_t idx);
void bus_write_sysvar(Bus* bus, uint8_t idx, uint16_t value);

// Named system variable access
uint16_t bus_read_pc(Bus* bus);
void bus_write_pc(Bus* bus, uint16_t value);
uint16_t bus_read_sp(Bus* bus);
void bus_write_sp(Bus* bus, uint16_t value);
uint16_t bus_read_flags(Bus* bus);
void bus_write_flags(Bus* bus, uint16_t value);
uint16_t bus_read_ir(Bus* bus);
void bus_write_ir(Bus* bus, uint16_t value);
uint64_t bus_read_cycles(Bus* bus);
void bus_write_cycles(Bus* bus, uint64_t value);

// --- Memory Layout Helpers ---
static inline bool bus_is_program_space(uint16_t addr) {
    return (addr >= BUS_PROGRAM_BASE && addr < BUS_PROGRAM_BASE + BUS_PROGRAM_SIZE);
}

static inline bool bus_is_data_space(uint16_t addr) {
    return (addr >= BUS_DATA_BASE && addr < BUS_DATA_BASE + BUS_DATA_SIZE);
}

static inline bool bus_is_stack_space(uint16_t addr) {
    return (addr >= BUS_STACK_BASE && addr < BUS_STACK_BASE + BUS_STACK_SIZE);
}

static inline bool bus_is_sysvar_space(uint16_t addr) {
    return (addr >= BUS_SYSVAR_BASE && addr < BUS_SYSVAR_BASE + BUS_SYSVAR_COUNT);
}

static inline bool bus_is_valid_addr(uint16_t addr) {
    return addr < BUS_MEMORY_SIZE;
}

// --- Diagnostic ---
void bus_dump(Bus* bus, uint16_t start, uint16_t words);
void bus_clear(Bus* bus, uint16_t start, uint16_t words);
void bus_dump_sysvars(Bus* bus);

#endif // BUS_H
