#include "bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// --- Internal Helpers ---
static void* xcalloc(size_t n, size_t size, const char* what) {
    void* p = calloc(n, size);
    if (!p) {
        fprintf(stderr, "Bus: allocation failed for %s\n", what);
        abort();
    }
    return p;
}

static void bus_tick(Bus* bus) {
    uint32_t row, col;
    bus_addr_to_rowcol(bus->addr, &row, &col);
    uint32_t idx = row * BUS_COLS + col;

    if (bus->rd) {
        uint16_t word = 0;
        for (int i = 0; i < BUS_DATA_BITS; i++) {
            Ccel* cell = &bus->planes[i][idx];
            CcelReadResult r = ccel_read(cell, 1, 1, 1);
            if (r.state == CCEL_POSITIVE) {
                word |= (1u << i);
            }
            // Restore after destructive read
            ccel_refresh(cell, 1, 1, 1, r.state);
        }
        bus->data_in = word;
    } else if (bus->wr) {
        for (int i = 0; i < BUS_DATA_BITS; i++) {
            Ccel* cell = &bus->planes[i][idx];
            CcelState bit = (bus->data_out & (1u << i)) ? CCEL_POSITIVE : CCEL_NEGATIVE;
            ccel_write(cell, 1, 1, 1, bit);
        }
    }
}

// --- Initialization ---
void bus_init(Bus* bus) {
    assert(bus != NULL);

    memset(bus, 0, sizeof(Bus));

    // Allocate one 256x256 grid per bit plane
    uint32_t cells_per_plane = BUS_ROWS * BUS_COLS;
    for (int i = 0; i < BUS_DATA_BITS; i++) {
        bus->planes[i] = (Ccel*)xcalloc(cells_per_plane, sizeof(Ccel), "bus plane");
        for (uint32_t r = 0; r < BUS_ROWS; r++) {
            for (uint32_t c = 0; c < BUS_COLS; c++) {
                ccel_init(&bus->planes[i][r * BUS_COLS + c], r, c);
            }
        }
    }

    bus->addr = 0;
    bus->data_in = 0;
    bus->data_out = 0;
    bus->rd = 0;
    bus->wr = 0;
}

void bus_free(Bus* bus) {
    assert(bus != NULL);
    for (int i = 0; i < BUS_DATA_BITS; i++) {
        free(bus->planes[i]);
        bus->planes[i] = NULL;
    }
}

// --- Core Operations ---
uint16_t bus_read(Bus* bus, uint16_t addr) {
    assert(bus != NULL);
    assert(bus_is_valid_addr(addr));
    bus->addr = addr;
    bus->rd = 1;
    bus->wr = 0;
    bus_tick(bus);
    bus->rd = 0;
    return bus->data_in;
}

void bus_write(Bus* bus, uint16_t addr, uint16_t value) {
    assert(bus != NULL);
    assert(bus_is_valid_addr(addr));
    bus->addr = addr;
    bus->data_out = value;
    bus->rd = 0;
    bus->wr = 1;
    bus_tick(bus);
    bus->wr = 0;
}

void bus_read_block(Bus* bus, uint16_t addr, uint16_t* buffer, uint16_t words) {
    assert(bus != NULL);
    assert(buffer != NULL);
    assert(bus_is_valid_addr(addr));
    assert(addr + words <= BUS_ROWS * BUS_COLS);
    for (uint16_t i = 0; i < words; i++) {
        buffer[i] = bus_read(bus, addr + i);
    }
}

void bus_write_block(Bus* bus, uint16_t addr, const uint16_t* buffer, uint16_t words) {
    assert(bus != NULL);
    assert(buffer != NULL);
    assert(bus_is_valid_addr(addr));
    assert(addr + words <= BUS_ROWS * BUS_COLS);

    for (uint16_t i = 0; i < words; i++) {
        bus_write(bus, addr + i, buffer[i]);
    }
}

// --- System Variables ---
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
    for (int i = 0; i < 16; i++) {
        printf("  R%-2d: 0x%04X\n", i, bus_read_sysvar(bus, i));
    }
    printf("  PC:  0x%04X\n", bus_read_pc(bus));
    printf("  SP:  0x%04X\n", bus_read_sp(bus));
    printf("  IR:  0x%04X\n", bus_read_ir(bus));
    printf("  FL:  0x%04X\n", bus_read_flags(bus));
    printf("  CY:  %llu\n", (unsigned long long)bus_read_cycles(bus));
}
