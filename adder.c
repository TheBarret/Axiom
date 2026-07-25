#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "adder.h"

static void* xcalloc(size_t n, size_t size, const char* what) {
    void* p = calloc(n, size);
    if (!p) {
        fprintf(stderr, "Adder: allocation failed for %s (n=%zu, size=%zu)\n", what, n, size);
        abort();
    }
    return p;
}

void adder_init(Adder* adder, int bits) {
    assert(bits > 0 && bits <= 64);
    adder->bits = bits;

    // Handle 64-bit shift edge case. (1ULL << 64) is undefined in C.
    adder->mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1);

    and_gate_init(&adder->and_gate);
    or_gate_init(&adder->or_gate);
    xor_gate_init(&adder->xor_gate, bits);

    adder->A_bits     = (int*)xcalloc(bits, sizeof(int), "A_bits");
    adder->B_bits     = (int*)xcalloc(bits, sizeof(int), "B_bits");
    adder->G          = (int*)xcalloc(bits, sizeof(int), "G");
    adder->P          = (int*)xcalloc(bits, sizeof(int), "P");
    adder->G_temp     = (int*)xcalloc(bits, sizeof(int), "G_temp");
    adder->P_temp     = (int*)xcalloc(bits, sizeof(int), "P_temp");
    adder->carries    = (int*)xcalloc(bits + 1, sizeof(int), "carries");
    adder->S          = (int*)xcalloc(bits, sizeof(int), "S");
    adder->original_P = (int*)xcalloc(bits, sizeof(int), "original_P");
}

void adder_free(Adder* adder) {
    xor_gate_free(&adder->xor_gate);
    free(adder->A_bits);
    free(adder->B_bits);
    free(adder->G);
    free(adder->P);
    free(adder->G_temp);
    free(adder->P_temp);
    free(adder->carries);
    free(adder->S);
    free(adder->original_P);

    adder->A_bits = adder->B_bits = adder->G = adder->P = NULL;
    adder->G_temp = adder->P_temp = adder->carries = adder->S = NULL;
    adder->original_P = NULL;
}

static inline void int_to_bits(uint64_t val, int bits, int* out) {
    for (int i = 0; i < bits; i++) {
        out[i] = (val >> i) & 1;
    }
}

static inline void compute_generate_propagate(Adder* adder) {
    gate_forward(&adder->and_gate, adder->A_bits, adder->B_bits, adder->bits, adder->G);
    xor_gate_forward(&adder->xor_gate, adder->A_bits, adder->B_bits, adder->bits, adder->P);
}

static void compute_carries_kogge_stone(Adder* adder, int cin) {
    int N = adder->bits;

    // Save original P before Kogge-Stone modifies it in place.
    // Uses the pre-allocated buffer from adder_init -- no malloc/free
    // on the hot path anymore.
    memcpy(adder->original_P, adder->P, N * sizeof(int));

    int* G_curr = adder->G;
    int* P_curr = adder->P;
    int* G_next = adder->G_temp;
    int* P_next = adder->P_temp;
    int* carries = adder->carries;

    // Inject carry-in
    G_curr[0] = G_curr[0] | (P_curr[0] & cin);

    int stride = 1;
    while (stride < N) {
        // Copy unchanged prefix
        for (int i = 0; i < stride; i++) {
            G_next[i] = G_curr[i];
            P_next[i] = P_curr[i];
        }
        // Compute new values using the OLD state (G_curr, P_curr)
        for (int i = stride; i < N; i++) {
            // P_next: Propagate AND Propagate(shifted)
            // G_next step 1: Propagate AND Generate(shifted)
            // G_next step 2: Generate OR (the result of step 1)
            P_next[i] = gate_forward_single(&adder->and_gate, P_curr[i], P_curr[i - stride]);
            int p_and_g = gate_forward_single(&adder->and_gate, P_curr[i], G_curr[i - stride]);
            G_next[i] = gate_forward_single(&adder->or_gate, G_curr[i], p_and_g);
        }

        // Pointer swapping (O(1)) instead of memcpy (O(N)).
        int* temp_G = G_curr; G_curr = G_next; G_next = temp_G;
        int* temp_P = P_curr; P_curr = P_next; P_next = temp_P;

        stride <<= 1;
    }

    // Ensure final results are in the primary adder->G and adder->P buffers
    if (G_curr != adder->G) {
        memcpy(adder->G, G_curr, N * sizeof(int));
        memcpy(adder->P, P_curr, N * sizeof(int));
    }

    // Restore original P for sum computation (P XOR carries)
    memcpy(adder->P, adder->original_P, N * sizeof(int));

    // Construct carries array
    carries[0] = cin;
    for (int i = 0; i < N; i++) {
        carries[i + 1] = adder->G[i];
    }
}

int adder_forward(Adder* adder, uint64_t A, uint64_t B, int cin, uint64_t* result) {
    // Mask inputs to bit width
    int_to_bits(A & adder->mask, adder->bits, adder->A_bits);
    int_to_bits(B & adder->mask, adder->bits, adder->B_bits);

    compute_generate_propagate(adder);
    compute_carries_kogge_stone(adder, cin);

    // Sum = P ^ C (where C = carries[0..N-1])
    xor_gate_forward(&adder->xor_gate, adder->P, adder->carries, adder->bits, adder->S);

    // Reconstruct integer result
    uint64_t res = 0;
    for (int i = 0; i < adder->bits; i++) {
        res |= ((uint64_t)adder->S[i]) << i;
    }

    *result = res & adder->mask;
    return adder->carries[adder->bits];
}

void adder_debug(Adder* adder, const char* label) {
    fprintf(stderr, "-- Adder debug: %s (bits=%d) --\n", label ? label : "", adder->bits);

    fprintf(stderr, "  A_bits : ");
    for (int i = adder->bits - 1; i >= 0; i--) fprintf(stderr, "%d", adder->A_bits[i]);
    fprintf(stderr, "\n  B_bits : ");
    for (int i = adder->bits - 1; i >= 0; i--) fprintf(stderr, "%d", adder->B_bits[i]);
    fprintf(stderr, "\n  G      : ");
    for (int i = adder->bits - 1; i >= 0; i--) fprintf(stderr, "%d", adder->G[i]);
    fprintf(stderr, "\n  P      : ");
    for (int i = adder->bits - 1; i >= 0; i--) fprintf(stderr, "%d", adder->P[i]);
    fprintf(stderr, "\n  carries: ");
    for (int i = adder->bits; i >= 0; i--) fprintf(stderr, "%d", adder->carries[i]);
    fprintf(stderr, "\n  S      : ");
    for (int i = adder->bits - 1; i >= 0; i--) fprintf(stderr, "%d", adder->S[i]);
    fprintf(stderr, "\n");
}
