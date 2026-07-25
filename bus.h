#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include <stdbool.h>
#include "ccel.h"

// --- Bus Configuration ---
#define BUS_DATA_BITS   16
#define BUS_ADDR_BITS   16
#define BUS_ROW_BITS    8
#define BUS_COL_BITS    8
#define BUS_ROWS        (1 << BUS_ROW_BITS)   // 256
#define BUS_COLS        (1 << BUS_COL_BITS)   // 256
#define BUS_ADDR_MASK   0xFFFF

// --- Memory Layout (Word Addresses) ---
#define BUS_PROGRAM_BASE   0x0000
#define BUS_PROGRAM_SIZE   0x2000   // 8192 words
#define BUS_DATA_BASE      0x2000
#define BUS_DATA_SIZE      0x1000   // 4096 words
#define BUS_STACK_BASE     0x3000
#define BUS_STACK_SIZE     0x1000   // 4096 words
#define BUS_FREE_BASE      0x4000
#define BUS_FREE_SIZE      0xC000   // 49152 words

// --- System Variables ---
#define BUS_SYSVAR_BASE    0xFFE0
#define BUS_SYSVAR_COUNT   24
#define BUS_SYSVAR_PC      0xFFE0
#define BUS_SYSVAR_SP      0xFFE1
#define BUS_SYSVAR_FLAGS   0xFFE2
#define BUS_SYSVAR_IR      0xFFE3
#define BUS_SYSVAR_CYCLES  0xFFE4

// --- Bus Structure ---
typedef struct {
    Ccel* planes[BUS_DATA_BITS];  // One plane per bit
    uint32_t addr;
    uint16_t data_in;
    uint16_t data_out;
    uint8_t rd;
    uint8_t wr;
} Bus;

// --- Initialization ---
void bus_init(Bus* bus);
void bus_free(Bus* bus);

// --- Core Operations ---
uint16_t bus_read(Bus* bus, uint16_t addr);
void bus_write(Bus* bus, uint16_t addr, uint16_t value);
void bus_read_block(Bus* bus, uint16_t addr, uint16_t* buffer, uint16_t words);
void bus_write_block(Bus* bus, uint16_t addr, const uint16_t* buffer, uint16_t words);

// --- System Variables ---
uint16_t bus_read_sysvar(Bus* bus, uint8_t idx);
void bus_write_sysvar(Bus* bus, uint8_t idx, uint16_t value);
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

// --- Helpers ---
static inline void bus_addr_to_rowcol(uint16_t addr, uint32_t* row, uint32_t* col) {
    *row = (addr >> BUS_COL_BITS) & (BUS_ROWS - 1);
    *col = addr & (BUS_COLS - 1);
}

static inline bool bus_is_valid_addr(uint16_t addr) {
    return addr < (BUS_ROWS * BUS_COLS);
}

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

// --- Diagnostic ---
void bus_dump(Bus* bus, uint16_t start, uint16_t words);
void bus_clear(Bus* bus, uint16_t start, uint16_t words);
void bus_dump_sysvars(Bus* bus);

#endif // BUS_H
