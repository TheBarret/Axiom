#include "ccel.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

static const float DEFAULT_BIAS = -1.5f;
static const float SELECTION_THRESHOLD = 0.0f;

void ccel_init(Ccel* n, uint32_t row, uint32_t col) {
    assert(n != NULL);

    n->w_row = 1.0f;
    n->w_col = 1.0f;
    n->w_depth = 0.0f;
    n->bias = DEFAULT_BIAS;
    n->w_feedback_reserved = 0.0f;

    n->state = CCEL_NEUTRAL;
    n->address = ((uint64_t)row << 32) | col;
    n->row_idx = row;
    n->col_idx = col;
    n->depth_idx = 0;
    n->access_count = 0;
    n->last_access_time = 0;
}

void ccel_init_3d(Ccel* n, uint32_t row, uint32_t col, uint32_t depth) {
    assert(n != NULL);
    ccel_init(n, row, col);
    n->w_depth = 1.0f;
    n->depth_idx = depth;
    n->address = ((uint64_t)depth << 48) | ((uint64_t)row << 32) | col;
}

void ccel_init_custom(Ccel* n, float w_row, float w_col, float w_depth, float bias) {
    assert(n != NULL);
    n->w_row = w_row;
    n->w_col = w_col;
    n->w_depth = w_depth;
    n->bias = bias;
    n->w_feedback_reserved = 0.0f;
    n->state = CCEL_NEUTRAL;
    n->address = 0;
    n->row_idx = 0;
    n->col_idx = 0;
    n->depth_idx = 0;
    n->access_count = 0;
    n->last_access_time = 0;
}

CcelSelection ccel_activate(const Ccel* n, int row_signal, int col_signal, int depth_signal) {
    assert(n != NULL);
    assert(row_signal == 0 || row_signal == 1);
    assert(col_signal == 0 || col_signal == 1);
    assert(depth_signal == 0 || depth_signal == 1);

    float linear = (n->w_row * (float)row_signal) +
                   (n->w_col * (float)col_signal) +
                   (n->w_depth * (float)depth_signal) +
                   n->bias;

    return (linear > SELECTION_THRESHOLD) ? CCEL_SELECTED : CCEL_UNSELECTED;
}

CcelReadResult ccel_read(Ccel* n, int row_signal, int col_signal, int depth_signal) {
    assert(n != NULL);

    CcelReadResult result = {
        .status = CCEL_UNSELECTED,
        .state = CCEL_NEUTRAL
    };

    if (ccel_activate(n, row_signal, col_signal, depth_signal) == CCEL_SELECTED) {
        result.status = CCEL_SELECTED;
        result.state = n->state;
        n->state = CCEL_NEUTRAL;
        n->access_count++;
        n->last_access_time = clock();
    }

    return result;
}

void ccel_write(Ccel* n, int row_signal, int col_signal, int depth_signal, CcelState new_state) {
    assert(n != NULL);
    assert(new_state >= CCEL_NEGATIVE && new_state <= CCEL_POSITIVE);

    if (ccel_activate(n, row_signal, col_signal, depth_signal) == CCEL_SELECTED) {
        n->state = new_state;
        n->access_count++;
        n->last_access_time = clock();
    }
}

void ccel_refresh(Ccel* n, int row_signal, int col_signal, int depth_signal, CcelState saved_state) {
    assert(n != NULL);
    assert(saved_state >= CCEL_NEGATIVE && saved_state <= CCEL_POSITIVE);

    if (ccel_activate(n, row_signal, col_signal, depth_signal) == CCEL_SELECTED) {
        n->state = saved_state;
        n->access_count++;
        n->last_access_time = clock();
    }
}

void ccel_erase(Ccel* n, int row_signal, int col_signal, int depth_signal) {
    ccel_write(n, row_signal, col_signal, depth_signal, CCEL_NEUTRAL);
}

void ccel_reset_stats(Ccel* n) {
    assert(n != NULL);
    n->access_count = 0;
    n->last_access_time = 0;
}

const char* ccel_state_to_string(CcelState state) {
    switch (state) {
        case CCEL_NEGATIVE: return "-";
        case CCEL_NEUTRAL:  return "0";
        case CCEL_POSITIVE: return "+";
        default: return "?";
    }
}
