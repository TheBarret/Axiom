#include "bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// --- Internal Helpers ---
static void* xcalloc(size_t n, size_t size, const char* what) {
    void* p = calloc(n, size);
    if (!p) {
        fprintf(stderr, "Bus: allocation failed for %s (n=%zu, size=%zu)\n",
                what, n, size);
        abort();
    }
    return p;
}

// --- Address Decoding ---
void bus_decode_address(uint16_t addr, uint8_t* depth, uint8_t* row, uint8_t* col) {
    *depth = (addr >> 8) & 0xFF;
    *row = (addr >> 4) & 0xF;
    *col = addr & 0xF;
}

uint16_t bus_encode_address(uint8_t depth, uint8_t row, uint8_t col) {
    return ((uint16_t)depth << 8) | ((uint16_t)row << 4) | col;
}

// --- Initialization ---
void bus_init(Bus* bus, Ccel* memory, uint16_t size) {
    assert(bus != NULL);
    assert(memory != NULL);
    assert(size == BUS_MEMORY_SIZE);

    bus->memory = memory;
    bus->size = size;
    bus->decode_cache.depth = 0;
    bus->decode_cache.row = 0;
    bus->decode_cache.col = 0;
}

void bus_free(Bus* bus) {
    bus->memory = NULL;
    bus->size = 0;
}

// --- Core Operations ---
uint16_t bus_read(Bus* bus, uint16_t addr) {
    assert(bus != NULL);
    assert(bus->memory != NULL);
    assert(bus_is_valid_addr(addr));

    uint16_t value = 0;

    for (int i = 0; i < BUS_DATA_BITS; i++) {
        uint16_t core_addr = addr + i;
        uint8_t depth, row, col;
        bus_decode_address(core_addr, &depth, &row, &col);

        CcelReadResult result = ccel_read(
            &bus->memory[core_addr],
            row, col, depth
        );

        if (result.status == CCEL_SELECTED && result.state == CCEL_POSITIVE) {
            value |= (1 << i);
        }
    }

    return value;
}

void bus_write(Bus* bus, uint16_t addr, uint16_t value) {
    assert(bus != NULL);
    assert(bus->memory != NULL);
    assert(bus_is_valid_addr(addr));

    for (int i = 0; i < BUS_DATA_BITS; i++) {
        uint16_t core_addr = addr + i;
        uint8_t depth, row, col;
        bus_decode_address(core_addr, &depth, &row, &col);

        int bit = (value >> i) & 1;
        CcelState state = bit ? CCEL_POSITIVE : CCEL_NEGATIVE;

        ccel_write(
            &bus->memory[core_addr],
            row, col, depth,
            state
        );
    }
}

void bus_read_block(Bus* bus, uint16_t addr, uint16_t* buffer, uint16_t words) {
    assert(bus != NULL);
    assert(buffer != NULL);
    assert(bus_is_valid_addr(addr));
    assert(addr + words <= BUS_MEMORY_SIZE / BUS_DATA_BITS);

    for (uint16_t i = 0; i < words; i++) {
        buffer[i] = bus_read(bus, addr + i);
    }
}

void bus_write_block(Bus* bus, uint16_t addr, const uint16_t* buffer, uint16_t words) {
    assert(bus != NULL);
    assert(buffer != NULL);
    assert(bus_is_valid_addr(addr));
    assert(addr + words <= BUS_MEMORY_SIZE / BUS_DATA_BITS);

    for (uint16_t i = 0; i < words; i++) {
        bus_write(bus, addr + i, buffer[i]);
    }
}

// --- System Variable Access ---
uint16_t bus_read_sysvar(Bus* bus, uint8_t idx) {
    assert(bus != NULL);
    assert(idx < BUS_SYSVAR_COUNT);
    return bus_read(bus, BUS_SYSVAR_BASE + idx);
}

void bus_write_sysvar(Bus* bus, uint8_t idx, uint16_t value) {
    assert(bus != NULL);
    assert(idx < BUS_SYSVAR_COUNT);
    bus_write(bus, BUS_SYSVAR_BASE + idx, value);
}

uint16_t bus_read_pc(Bus* bus) {
    return bus_read(bus, BUS_SYSVAR_PC);
}

void bus_write_pc(Bus* bus, uint16_t value) {
    bus_write(bus, BUS_SYSVAR_PC, value);
}

uint16_t bus_read_sp(Bus* bus) {
    return bus_read(bus, BUS_SYSVAR_SP);
}

void bus_write_sp(Bus* bus, uint16_t value) {
    bus_write(bus, BUS_SYSVAR_SP, value);
}

uint16_t bus_read_flags(Bus* bus) {
    return bus_read(bus, BUS_SYSVAR_FLAGS);
}

void bus_write_flags(Bus* bus, uint16_t value) {
    bus_write(bus, BUS_SYSVAR_FLAGS, value);
}

uint16_t bus_read_ir(Bus* bus) {
    return bus_read(bus, BUS_SYSVAR_IR);
}

void bus_write_ir(Bus* bus, uint16_t value) {
    bus_write(bus, BUS_SYSVAR_IR, value);
}

uint64_t bus_read_cycles(Bus* bus) {
    uint64_t cycles = 0;
    for (int i = 0; i < 4; i++) {
        uint16_t word = bus_read(bus, BUS_SYSVAR_CYCLES + i);
        cycles |= ((uint64_t)word << (i * 16));
    }
    return cycles;
}

void bus_write_cycles(Bus* bus, uint64_t value) {
    for (int i = 0; i < 4; i++) {
        uint16_t word = (value >> (i * 16)) & 0xFFFF;
        bus_write(bus, BUS_SYSVAR_CYCLES + i, word);
    }
}

// --- Diagnostic ---
void bus_dump(Bus* bus, uint16_t start, uint16_t words) {
    assert(bus != NULL);

    printf("Bus Dump: addr=0x%04X, words=%u\n", start, words);
    printf("  Addr  |  Data (hex) | Data (bin)\n");
    printf("  ------+-------------+-----------\n");

    for (uint16_t i = 0; i < words; i++) {
        uint16_t addr = start + i;
        uint16_t data = bus_read(bus, addr);

        printf("  0x%04X | 0x%04X     | ", addr, data);
        for (int b = BUS_DATA_BITS - 1; b >= 0; b--) {
            printf("%d", (data >> b) & 1);
            if (b % 4 == 0 && b > 0) printf(" ");
        }
        printf("\n");
    }
    printf("\n");
}

void bus_clear(Bus* bus, uint16_t start, uint16_t words) {
    assert(bus != NULL);

    for (uint16_t i = 0; i < words; i++) {
        bus_write(bus, start + i, 0x0000);
    }
}

void bus_dump_sysvars(Bus* bus) {
    printf("System Variables:\n");
    printf("  R0:  0x%04X\n", bus_read_sysvar(bus, 0));
    printf("  R1:  0x%04X\n", bus_read_sysvar(bus, 1));
    printf("  R2:  0x%04X\n", bus_read_sysvar(bus, 2));
    printf("  R3:  0x%04X\n", bus_read_sysvar(bus, 3));
    printf("  R4:  0x%04X\n", bus_read_sysvar(bus, 4));
    printf("  R5:  0x%04X\n", bus_read_sysvar(bus, 5));
    printf("  R6:  0x%04X\n", bus_read_sysvar(bus, 6));
    printf("  R7:  0x%04X\n", bus_read_sysvar(bus, 7));
    printf("  R8:  0x%04X\n", bus_read_sysvar(bus, 8));
    printf("  R9:  0x%04X\n", bus_read_sysvar(bus, 9));
    printf("  R10: 0x%04X\n", bus_read_sysvar(bus, 10));
    printf("  R11: 0x%04X\n", bus_read_sysvar(bus, 11));
    printf("  R12: 0x%04X\n", bus_read_sysvar(bus, 12));
    printf("  R13: 0x%04X\n", bus_read_sysvar(bus, 13));
    printf("  R14: 0x%04X\n", bus_read_sysvar(bus, 14));
    printf("  R15: 0x%04X\n", bus_read_sysvar(bus, 15));
    printf("  PC:  0x%04X\n", bus_read_pc(bus));
    printf("  SP:  0x%04X\n", bus_read_sp(bus));
    printf("  IR:  0x%04X\n", bus_read_ir(bus));
    printf("  FL:  0x%04X\n", bus_read_flags(bus));
    printf("  CY:  %llu\n", (unsigned long long)bus_read_cycles(bus));
}
