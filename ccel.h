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

typedef struct {
    // Selection weights (pure coincident detection)
    float w_row;
    float w_col;
    float w_depth;
    float bias;

    // The 3-state core
    CcelState state;

    // 64-bit address to avoid packing collisions
    uint64_t address;

    // Metadata (diagnostic only)
    uint32_t row_idx;
    uint32_t col_idx;
    uint32_t depth_idx;
    uint64_t access_count;
    uint64_t last_access_time;

    // UNTESTED: Future hysteretic mode
    // When non-zero, selection becomes content-dependent.
    // Experimental purpose
    // Default: 0.0
    float w_feedback_reserved;
} Ccel;

// --- Initialization ---
void ccel_init(Ccel* n, uint32_t row, uint32_t col);
void ccel_init_3d(Ccel* n, uint32_t row, uint32_t col, uint32_t depth);
void ccel_init_custom(Ccel* n,
                           float w_row, float w_col,
                           float w_depth,
                           float bias);

// --- Core Operations ---
CcelSelection ccel_activate(const Ccel* n,
                                       int row_signal,
                                       int col_signal,
                                       int depth_signal);

CcelReadResult ccel_read(Ccel* n,
                                    int row_signal,
                                    int col_signal,
                                    int depth_signal);

void ccel_write(Ccel* n,
                     int row_signal,
                     int col_signal,
                     int depth_signal,
                     CcelState new_state);

void ccel_refresh(Ccel* n,
                       int row_signal,
                       int col_signal,
                       int depth_signal,
                       CcelState saved_state);

// --- Debug/Diagnostic (Non-destructive) ---
static inline CcelState ccel_peek(const Ccel* n) {
    return n->state;  // Safe: no side effects
}

static inline uint64_t ccel_get_address(const Ccel* n) {
    return n->address;
}

// --- Convenience ---
void ccel_erase(Ccel* n,
                     int row_signal,
                     int col_signal,
                     int depth_signal);

void ccel_reset_stats(Ccel* n);
const char* ccel_state_to_string(CcelState state);

static inline int ccel_state_to_binary(CcelState state) {
    return (state == CCEL_POSITIVE) ? 1 : 0;
}

static inline CcelState ccel_binary_to_state(int bit) {
    assert(bit == 0 || bit == 1);
    return (bit == 1) ? CCEL_POSITIVE : CCEL_NEGATIVE;
}

#endif // CCEL_H
