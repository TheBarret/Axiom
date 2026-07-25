#ifndef ADDER_H
#define ADDER_H

#include <stdint.h>
#include "gates.h"

typedef struct {
    int bits;

    Gate and_gate;
    Gate or_gate;
    XorGate xor_gate;

    int* A_bits;
    int* B_bits;
    int* G;
    int* P;
    int* G_temp;
    int* P_temp;
    int* carries;
    int* S;
    int* original_P;   // Pre-allocated in adder_init; removes the per-call
                        // malloc/free that used to live inside
                        // compute_carries_kogge_stone. Same pattern as
                        // XorGate's temp1/temp2.

    uint64_t mask;
} Adder;

void adder_init(Adder* adder, int bits);
void adder_free(Adder* adder);
int  adder_forward(Adder* adder, uint64_t A, uint64_t B, int cin, uint64_t* result);

// debugger utilities
void adder_debug(Adder* adder, const char* label);

#endif // ADDER_H
