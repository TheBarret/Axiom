#ifndef GATES_H
#define GATES_H

#include <stdbool.h>
#include <stdint.h>

// --- Threshold contract ---
// All gates fire on (linear > 0.0f), strictly greater-than, never >=.
// NAND/NOR correctness depends on this being exact and consistent
// across every gate below. Do not change without re-deriving all biases.

typedef struct {
    float w1;
    float w2;
    float bias;
} Gate;

typedef struct {
    float w1;
    float bias;
} NotGate;

typedef struct {
    Gate or_gate;
    Gate nand_gate;
    Gate and_gate;
    int* temp1;      // Pre-allocated buffers, sized to max_n
    int* temp2;
    int max_n;        // For bounds checking
} XorGate;

// --- Base Gate ---
// Fixed-weight singletons: these four shapes never change at runtime,
// so callers can use them directly instead of paying init cost per gate.
extern const Gate AND_GATE;
extern const Gate OR_GATE;
extern const Gate NAND_GATE;
extern const Gate NOR_GATE;

void gate_init(Gate* g, float w1, float w2, float bias);
void and_gate_init(Gate* g);
void or_gate_init(Gate* g);
void nand_gate_init(Gate* g);
void nor_gate_init(Gate* g);

// Single source of truth for the threshold rule. gate_forward loops
// over this rather than duplicating the linear-threshold formula.
int gate_forward_single(const Gate* g, int x1, int x2);
void gate_forward(const Gate* g, const int* restrict x1, const int* restrict x2,
                   int n, int* restrict out);

// --- Not Gate ---
void not_gate_init(NotGate* g);
int not_gate_forward_single(const NotGate* g, int x);
void not_gate_forward(const NotGate* g, const int* restrict x, int n, int* restrict out);

// --- Xor Gate ---
// Composed from OR, NAND, AND (3 gate evaluations per bit). Always
// call the batched form (xor_gate_forward) from vectorized callers;
// the _single form exists for scalar/debug use and will not vectorize.
void xor_gate_init(XorGate* g, int max_n);
void xor_gate_free(XorGate* g);
int xor_gate_forward_single(XorGate* g, int x1, int x2);
void xor_gate_forward(XorGate* g, const int* restrict x1, const int* restrict x2,
                       int n, int* restrict out);

#endif // GATES_H
