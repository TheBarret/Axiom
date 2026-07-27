#ifndef CCEL_H
#define CCEL_H

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

typedef enum {
    CCEL_NEGATIVE = -1,
    CCEL_NEUTRAL  =  0,
    CCEL_POSITIVE =  1
} CcelState;

typedef enum {
    CCEL_UNSELECTED = 0,
    CCEL_SELECTED   = 1
} CcelSelection;

typedef struct {
    CcelSelection status;
    CcelState state;
} CcelReadResult;

// Safe, overlap-free addressing
typedef union {
    uint64_t raw;
    struct {
        uint32_t col;   // Bits 0-31
        uint16_t row;   // Bits 32-47
        uint16_t depth; // Bits 48-63
    };
} CcelAddress;

typedef struct {
    float w_row;
    float w_col;
    float w_depth;
    float bias;

    CcelState state;
    CcelAddress address; // Replaces separate address/idx fields

    uint64_t access_count;
    uint64_t last_access_time;
    float w_feedback_reserved;
} Ccel;

#define CCEL_IS_VALID_SIGNAL(s) (((s) & ~1) == 0)

// Initialization
void ccel_init(Ccel* n, uint16_t row, uint32_t col);
void ccel_init_3d(Ccel* n, uint16_t row, uint32_t col, uint16_t depth);
void ccel_init_custom(Ccel* n, float w_row, float w_col, float w_depth, float bias);

// Core Operations
CcelSelection ccel_activate(const Ccel* n, bool row_signal, bool col_signal, bool depth_signal);
CcelReadResult ccel_read(Ccel* n, bool row_signal, bool col_signal, bool depth_signal);
void ccel_write(Ccel* n, bool row_signal, bool col_signal, bool depth_signal, CcelState new_state);
void ccel_refresh(Ccel* n, bool row_signal, bool col_signal, bool depth_signal, CcelState saved_state);

// Convenience
static inline CcelState ccel_peek(const Ccel* n) { return n->state; }
static inline uint64_t ccel_get_address(const Ccel* n) { return n->address.raw; }
static inline uint64_t ccel_get_mask(int bits) {
    if (bits >= 64) return ~0ULL;
    if (bits <= 0)  return 0ULL;
    return (1ULL << bits) - 1;
}

void ccel_erase(Ccel* n, bool row_signal, bool col_signal, bool depth_signal);
void ccel_reset_stats(Ccel* n);
const char* ccel_state_to_string(CcelState state);

static inline int ccel_state_to_binary(CcelState state) {
    return (state == CCEL_POSITIVE) ? 1 : 0;
}

static inline CcelState ccel_binary_to_state(int bit) {
    return (bit == 1) ? CCEL_POSITIVE : CCEL_NEGATIVE;
}

#endif // CCEL_H
