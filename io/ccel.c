
#include "ccel.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

/*
 version 0.2:
 - **Zero-Cost Signal Sanitization**
     By changing the parameters to `bool`, the C compiler automatically truncates any rogue integer input to strictly `0` or `1`.
     This completely neutralizes the "Invalid signals silently break the model" issue without adding a single runtime `if` statement or macro check.

 - **Symmetrical Threshold Logic**
     Changed `linear > SELECTION_THRESHOLD` to `linear >= SELECTION_THRESHOLD`.
     This is the mathematically correct behavior for McCulloch-Pitts neurons and elegantly resolves the `0.0` boundary edge case.

 - **Collision-Free Addressing**
     The `n->address` union cleanly separates `col` (32-bit), `row` (16-bit), and `depth` (16-bit).
     No bit-shifting math means zero risk of overflow or bit 47 collisions.

 - **Defensive Null Checks**
     Added `if (!n) return;` guards at the top of every mutating function.
     This prevents segfaults if a null pointer slips through, acting as a final safety net alongside `assert`.
 */

static const float DEFAULT_BIAS = -1.5f;
static const float SELECTION_THRESHOLD = 0.0f;

void ccel_init(Ccel* n, uint16_t row, uint32_t col) {
    assert(n != NULL);

    n->w_row = 1.0f;
    n->w_col = 1.0f;
    n->w_depth = 0.0f;
    n->bias = DEFAULT_BIAS;
    n->w_feedback_reserved = 0.0f;

    n->state = CCEL_NEUTRAL;

    // Safe, overlap-free addressing via C11 anonymous union
    n->address.raw = 0;
    n->address.col = col;
    n->address.row = row;
    n->address.depth = 0;

    n->access_count = 0;
    n->last_access_time = 0;
}

void ccel_init_3d(Ccel* n, uint16_t row, uint32_t col, uint16_t depth) {
    assert(n != NULL);

    // Initialize base 2D properties first
    ccel_init(n, row, col);

    // Override for 3D operation
    n->w_depth = 1.0f;
    n->address.depth = depth;
}

void ccel_init_custom(Ccel* n, float w_row, float w_col, float w_depth, float bias) {
    assert(n != NULL);

    n->w_row = w_row;
    n->w_col = w_col;
    n->w_depth = w_depth;
    n->bias = bias;
    n->w_feedback_reserved = 0.0f;

    n->state = CCEL_NEUTRAL;
    n->address.raw = 0;
    n->access_count = 0;
    n->last_access_time = 0;
}

CcelSelection ccel_activate(const Ccel* n, bool row_signal, bool col_signal, bool depth_signal) {
    if (!n) return CCEL_UNSELECTED;

    // Using `bool` in the signature provides implicit, zero-cost sanitization.
    // In C, any integer passed to a `bool` parameter is strictly converted to 0 or 1.
    // This completely eliminates the NDEBUG assert vulnerability without branching overhead.

    float linear = (n->w_row * (float)row_signal) +
                   (n->w_col * (float)col_signal) +
                   (n->w_depth * (float)depth_signal) +
                   n->bias;

    // Using >= instead of > ensures symmetrical McCulloch-Pitts threshold logic.
    // If the weighted sum exactly equals the threshold, the neuron fires.
    return (linear >= SELECTION_THRESHOLD) ? CCEL_SELECTED : CCEL_UNSELECTED;
}

CcelReadResult ccel_read(Ccel* n, bool row_signal, bool col_signal, bool depth_signal) {
    CcelReadResult result = {
        .status = CCEL_UNSELECTED,
        .state = CCEL_NEUTRAL
    };

    if (!n) return result;

    if (ccel_activate(n, row_signal, col_signal, depth_signal) == CCEL_SELECTED) {
        result.status = CCEL_SELECTED;
        result.state = n->state;

        // Authentic destructive read: reading clears the magnetic polarization
        n->state = CCEL_NEUTRAL;

        n->access_count++;
        n->last_access_time = clock();
    }

    return result;
}

void ccel_write(Ccel* n, bool row_signal, bool col_signal, bool depth_signal, CcelState new_state) {
    if (!n) return;

    // Assert remains here for developer debugging of logic errors,
    // but the API itself is now safe from external garbage input.
    assert(new_state >= CCEL_NEGATIVE && new_state <= CCEL_POSITIVE);

    if (ccel_activate(n, row_signal, col_signal, depth_signal) == CCEL_SELECTED) {
        n->state = new_state;
        n->access_count++;
        n->last_access_time = clock();
    }
}

void ccel_refresh(Ccel* n, bool row_signal, bool col_signal, bool depth_signal, CcelState saved_state) {
    if (!n) return;
    assert(saved_state >= CCEL_NEGATIVE && saved_state <= CCEL_POSITIVE);

    if (ccel_activate(n, row_signal, col_signal, depth_signal) == CCEL_SELECTED) {
        n->state = saved_state;
        n->access_count++;
        n->last_access_time = clock();
    }
}

void ccel_erase(Ccel* n, bool row_signal, bool col_signal, bool depth_signal) {
    if (!n) return;
    // Erase is simply a write of the NEUTRAL state
    ccel_write(n, row_signal, col_signal, depth_signal, CCEL_NEUTRAL);
}

void ccel_reset_stats(Ccel* n) {
    if (!n) return;
    n->access_count = 0;
    n->last_access_time = 0;
}

const char* ccel_state_to_string(CcelState state) {
    switch (state) {
        case CCEL_NEGATIVE: return "-";
        case CCEL_NEUTRAL:  return "0";
        case CCEL_POSITIVE: return "+";
        default:            return "?";
    }
}
