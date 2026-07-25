#include "gates.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// Fixed-weight singletons
const Gate AND_GATE  = { 1.0f,  1.0f, -1.5f };
const Gate OR_GATE    = { 1.0f,  1.0f, -0.5f };
const Gate NAND_GATE = { -1.0f, -1.0f, 1.5f };
const Gate NOR_GATE   = { -1.0f, -1.0f, 0.5f };

// Base Gate
void gate_init(Gate* g, float w1, float w2, float bias) {
    g->w1 = w1;
    g->w2 = w2;
    g->bias = bias;
}

void and_gate_init(Gate* g)  { *g = AND_GATE; }
void or_gate_init(Gate* g)   { *g = OR_GATE; }
void nand_gate_init(Gate* g) { *g = NAND_GATE; }
void nor_gate_init(Gate* g)  { *g = NOR_GATE; }

// Single source of truth for the threshold rule.
int gate_forward_single(const Gate* g, int x1, int x2) {
    assert((x1 == 0 || x1 == 1) && (x2 == 0 || x2 == 1) &&
           "gate input must be strictly 0 or 1");
    float linear = (g->w1 * (float)x1) + (g->w2 * (float)x2) + g->bias;
    return (linear > 0.0f) ? 1 : 0;
}

void gate_forward(const Gate* g, const int* restrict x1, const int* restrict x2,
                   int n, int* restrict out) {
    for (int i = 0; i < n; i++) {
        out[i] = gate_forward_single(g, x1[i], x2[i]);
    }
}

// Not Gate
void not_gate_init(NotGate* g) {
    g->w1 = -1.0f;
    g->bias = 0.5f;
}

int not_gate_forward_single(const NotGate* g, int x) {
    assert((x == 0 || x == 1) && "gate input must be strictly 0 or 1");
    float linear = (g->w1 * (float)x) + g->bias;
    return (linear > 0.0f) ? 1 : 0;
}

void not_gate_forward(const NotGate* g, const int* restrict x, int n, int* restrict out) {
    for (int i = 0; i < n; i++) {
        out[i] = not_gate_forward_single(g, x[i]);
    }
}

// Xor Gate
void xor_gate_init(XorGate* g, int max_n) {
    and_gate_init(&g->and_gate);
    or_gate_init(&g->or_gate);
    nand_gate_init(&g->nand_gate);
    g->max_n = max_n;
    g->temp1 = (int*)calloc(max_n, sizeof(int));
    g->temp2 = (int*)calloc(max_n, sizeof(int));
    assert(g->temp1 != NULL && g->temp2 != NULL);
}

void xor_gate_free(XorGate* g) {
    free(g->temp1);
    free(g->temp2);
    g->temp1 = NULL;
    g->temp2 = NULL;
}

int xor_gate_forward_single(XorGate* g, int x1, int x2) {
    int or_out = gate_forward_single(&g->or_gate, x1, x2);
    int nand_out = gate_forward_single(&g->nand_gate, x1, x2);
    return gate_forward_single(&g->and_gate, or_out, nand_out);
}

// Uses the pre-allocated temp1/temp2 buffers from init instead of
// stack or heap allocation per call. Caller must respect max_n.
void xor_gate_forward(XorGate* g, const int* restrict x1, const int* restrict x2,
                       int n, int* restrict out) {
    assert(n <= g->max_n && "n exceeds XorGate max_n; re-init with a larger max_n");

    gate_forward(&g->or_gate, x1, x2, n, g->temp1);
    gate_forward(&g->nand_gate, x1, x2, n, g->temp2);
    gate_forward(&g->and_gate, g->temp1, g->temp2, n, out);
}
