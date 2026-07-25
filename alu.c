#include "alu.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>


// HELPERS


static inline void int_to_bits(uint64_t val, int bits, int* out) {
    for (int i = 0; i < bits; i++) {
        out[i] = (val >> i) & 1;
    }
}

static inline uint64_t bits_to_int(const int* bits, int n) {
    uint64_t res = 0;
    for (int i = 0; i < n; i++) {
        res |= ((uint64_t)bits[i]) << i;
    }
    return res;
}

static void* xcalloc(size_t n, size_t size, const char* what) {
    void* p = calloc(n, size);
    if (!p) {
        fprintf(stderr, "ALU: allocation failed for %s (n=%zu, size=%zu)\n",
                what, n, size);
        abort();
    }
    return p;
}

static inline uint64_t width_mask(int bits) {
    return (bits >= 64) ? ~0ULL : ((1ULL << bits) - 1);
}

// Generic OR-reduction over n bits -> single bit, log2(n)-depth pairwise
// tree. Mutates buf in place. This is the fold that replaces branchy
// scan-with-break patterns anywhere a "did any bit differ / is anything
// set" question shows up -- currently used by the comparator's equality
// check, and reusable later (e.g. a Neuristor memory parity/integrity
// check would be the same primitive one layer up).
static int gate_reduce_or(const Gate* or_gate, int* buf, int n) {
    while (n > 1) {
        int half = (n + 1) / 2;
        for (int i = 0; i < n / 2; i++) {
            buf[i] = gate_forward_single(or_gate, buf[2 * i], buf[2 * i + 1]);
        }
        if (n % 2 == 1) {
            buf[half - 1] = buf[n - 1];
        }
        n = half;
    }
    return buf[0];
}


// SUBTRACTOR


void subtractor_init(Subtractor* sub, int bits) {
    sub->bits = bits;
    sub->mask = width_mask(bits);
    not_gate_init(&sub->not_gate);
    adder_init(&sub->adder, bits);
    sub->B_bits     = (int*)xcalloc(bits, sizeof(int), "Subtractor.B_bits");
    sub->B_inv_bits = (int*)xcalloc(bits, sizeof(int), "Subtractor.B_inv_bits");
}

void subtractor_free(Subtractor* sub) {
    adder_free(&sub->adder);
    free(sub->B_bits);
    free(sub->B_inv_bits);
    sub->B_bits = NULL;
    sub->B_inv_bits = NULL;
}

int subtractor_forward(Subtractor* sub, uint64_t A, uint64_t B, uint64_t* result) {
    A &= sub->mask;
    B &= sub->mask;

    // No A==B fast path (see header note) -- always run the full
    // NOT + two-adder two's-complement pipeline, so sub->adder.carries
    // reflects this call's own math every time.
    int_to_bits(B, sub->bits, sub->B_bits);
    not_gate_forward(&sub->not_gate, sub->B_bits, sub->bits, sub->B_inv_bits);

    uint64_t B_neg = bits_to_int(sub->B_inv_bits, sub->bits);

    uint64_t B_neg_plus_1 = 0;
    adder_forward(&sub->adder, B_neg, 1, 0, &B_neg_plus_1);

    int carry = adder_forward(&sub->adder, A, B_neg_plus_1, 0, result);
    *result &= sub->mask;

    return carry ? 0 : 1; // borrow is the inverse of carry-out
}


// MULTIPLIER (Shift-and-Add, constant-time)


void multiplier_init(SAMultiplier* mul, int bits) {
    mul->bits = bits;
    mul->mask = width_mask(bits);
    mul->full_mask = (bits * 2 >= 64) ? ~0ULL : ((1ULL << (bits * 2)) - 1);

    and_gate_init(&mul->and_gate);
    adder_init(&mul->adder, bits * 2);

    int width2 = bits * 2;
    mul->b_bcast      = (int*)xcalloc(width2, sizeof(int), "Multiplier.b_bcast");
    mul->shifted_bits = (int*)xcalloc(width2, sizeof(int), "Multiplier.shifted_bits");
    mul->masked_bits  = (int*)xcalloc(width2, sizeof(int), "Multiplier.masked_bits");
}

void multiplier_free(SAMultiplier* mul) {
    adder_free(&mul->adder);
    free(mul->b_bcast);
    free(mul->shifted_bits);
    free(mul->masked_bits);
    mul->b_bcast = mul->shifted_bits = mul->masked_bits = NULL;
}

uint64_t multiplier_forward(SAMultiplier* mul, uint64_t A, uint64_t B) {
    A &= mul->mask;
    B &= mul->mask;

    int width2 = mul->bits * 2;
    uint64_t result = 0;

    for (int i = 0; i < mul->bits; i++) {
        int b_bit = (int)((B >> i) & 1);
        uint64_t shifted = (A << i) & mul->full_mask;

        int_to_bits(shifted, width2, mul->shifted_bits);

        // Fan-out, not a gate: replicating one wire's value onto N lines
        // is routing, same as a real bus tap. The logic happens next,
        // in the AND.
        for (int j = 0; j < width2; j++) {
            mul->b_bcast[j] = b_bit;
        }

        gate_forward(&mul->and_gate, mul->shifted_bits, mul->b_bcast, width2, mul->masked_bits);
        uint64_t masked_val = bits_to_int(mul->masked_bits, width2);

        // Always adds -- exactly `bits` adder_forward calls every time,
        // regardless of B's bit pattern. When b_bit is 0, masked_val is
        // 0 and this is an add-of-zero, not a skipped call.
        uint64_t next_result = 0;
        adder_forward(&mul->adder, result, masked_val, 0, &next_result);
        result = next_result;
    }

    return result & mul->full_mask;
}


// BITWISE LOGIC


void bitwise_init(BitWiseLogic* bw, int bits, int gate_type) {
    bw->bits = bits;
    bw->gate_type = gate_type;

    and_gate_init(&bw->and_gate);
    or_gate_init(&bw->or_gate);

    if (gate_type == 2) {
        xor_gate_init(&bw->xor_gate, bits);
    }

    bw->A_bits      = (int*)xcalloc(bits, sizeof(int), "BitWiseLogic.A_bits");
    bw->B_bits      = (int*)xcalloc(bits, sizeof(int), "BitWiseLogic.B_bits");
    bw->result_bits = (int*)xcalloc(bits, sizeof(int), "BitWiseLogic.result_bits");
}

void bitwise_free(BitWiseLogic* bw) {
    if (bw->gate_type == 2) {
        xor_gate_free(&bw->xor_gate);
    }
    free(bw->A_bits);
    free(bw->B_bits);
    free(bw->result_bits);
    bw->A_bits = bw->B_bits = bw->result_bits = NULL;
}

uint64_t bitwise_forward(BitWiseLogic* bw, uint64_t A, uint64_t B) {
    int_to_bits(A, bw->bits, bw->A_bits);
    int_to_bits(B, bw->bits, bw->B_bits);

    if (bw->gate_type == 0) {
        gate_forward(&bw->and_gate, bw->A_bits, bw->B_bits, bw->bits, bw->result_bits);
    } else if (bw->gate_type == 1) {
        gate_forward(&bw->or_gate, bw->A_bits, bw->B_bits, bw->bits, bw->result_bits);
    } else {
        xor_gate_forward(&bw->xor_gate, bw->A_bits, bw->B_bits, bw->bits, bw->result_bits);
    }

    return bits_to_int(bw->result_bits, bw->bits);
}


// COMPARATOR


void comparator_init(Comparator* cmp, int bits) {
    cmp->bits = bits;
    or_gate_init(&cmp->or_gate);
    xor_gate_init(&cmp->xor_gate, bits);
    cmp->A_bits      = (int*)xcalloc(bits, sizeof(int), "Comparator.A_bits");
    cmp->B_bits      = (int*)xcalloc(bits, sizeof(int), "Comparator.B_bits");
    cmp->xor_results = (int*)xcalloc(bits, sizeof(int), "Comparator.xor_results");
}

void comparator_free(Comparator* cmp) {
    xor_gate_free(&cmp->xor_gate);
    free(cmp->A_bits);
    free(cmp->B_bits);
    free(cmp->xor_results);
    cmp->A_bits = cmp->B_bits = cmp->xor_results = NULL;
}

CmpResult comparator_forward(Comparator* cmp, uint64_t A, uint64_t B) {
    int_to_bits(A, cmp->bits, cmp->A_bits);
    int_to_bits(B, cmp->bits, cmp->B_bits);

    xor_gate_forward(&cmp->xor_gate, cmp->A_bits, cmp->B_bits, cmp->bits, cmp->xor_results);

    // xor_results is scratch past this point -- reducing it in place.
    int any_diff = gate_reduce_or(&cmp->or_gate, cmp->xor_results, cmp->bits);

    CmpResult res = {0, 0, 0};
    res.equal = any_diff ? 0 : 1;
    // less_than / greater_than are intentionally left 0 here -- the ALU
    // derives them from the subtractor's flags. See alu_forward.
    return res;
}


// ALU

void alu_init(ALU* alu, int bits) {
    alu->bits = bits;
    alu->mask = width_mask(bits);
    alu->cmp_signed = 1;

    adder_init(&alu->adder, bits);
    subtractor_init(&alu->subtractor, bits);
    bitwise_init(&alu->and_logic, bits, 0);
    bitwise_init(&alu->or_logic, bits, 1);
    bitwise_init(&alu->xor_logic, bits, 2);
    multiplier_init(&alu->multiplier, bits);
    comparator_init(&alu->comparator, bits);

    alu->flags = (ALUFlags){0, 0, 0, 0, 0};
}

void alu_free(ALU* alu) {
    adder_free(&alu->adder);
    subtractor_free(&alu->subtractor);
    bitwise_free(&alu->and_logic);
    bitwise_free(&alu->or_logic);
    bitwise_free(&alu->xor_logic);
    multiplier_free(&alu->multiplier);
    comparator_free(&alu->comparator);
}

void alu_set_cmp_mode(ALU* alu, int is_signed) {
    alu->cmp_signed = is_signed ? 1 : 0;
}

uint64_t alu_forward(ALU* alu, uint64_t A, uint64_t B, Opcode op) {
    int bits = alu->bits;
    // All operations computed every call, unconditionally (compute-all mux-select)

    uint64_t add_result = 0;
    int add_carry = adder_forward(&alu->adder, A, B, 0, &add_result);
    add_result &= alu->mask;
    int add_overflow = alu->adder.carries[bits] ^ alu->adder.carries[bits - 1];

    uint64_t sub_result = 0;
    int sub_borrow = subtractor_forward(&alu->subtractor, A, B, &sub_result);
    sub_result &= alu->mask;
    int sub_overflow = alu->subtractor.adder.carries[bits] ^ alu->subtractor.adder.carries[bits - 1];

    uint64_t and_result = bitwise_forward(&alu->and_logic, A, B);
    uint64_t or_result  = bitwise_forward(&alu->or_logic, A, B);
    uint64_t xor_result = bitwise_forward(&alu->xor_logic, A, B);

    uint64_t mul_full   = multiplier_forward(&alu->multiplier, A, B);
    uint64_t mul_result = mul_full & alu->mask;
    uint64_t mul_overflow = mul_full >> bits; // nonzero => product didn't fit

    CmpResult cmp_result = comparator_forward(&alu->comparator, A, B);

    // Ordering derived from the subtraction we already computed, same
    // way real ALUs do it -- no separate magnitude-comparison logic.
    //   signed:   LT = sign(A-B) XOR overflow(A-B)   (standard slt idiom)
    //   unsigned: LT = borrow(A-B)
    int sub_sign = (int)((sub_result >> (bits - 1)) & 1);
    int cmp_less = alu->cmp_signed ? (sub_sign ^ sub_overflow) : sub_borrow;
    int cmp_greater = (!cmp_result.equal) && (!cmp_less);

    uint64_t result = 0;
    switch (op) {
        case OP_ADD: result = add_result; break;
        case OP_SUB: result = sub_result; break;
        case OP_AND: result = and_result; break;
        case OP_OR:  result = or_result;  break;
        case OP_XOR: result = xor_result; break;
        case OP_MUL: result = mul_result; break;
        case OP_CMP: result = 0; break;
        default:     result = 0; break;
    }

    alu->flags.zero = 0;
    alu->flags.carry = 0;
    alu->flags.overflow = 0;
    alu->flags.less = 0;
    alu->flags.greater = 0;

    if (op == OP_CMP) {
        alu->flags.zero = cmp_result.equal;
        alu->flags.less = cmp_result.equal ? 0 : cmp_less;
        alu->flags.greater = cmp_greater;
    } else {
        alu->flags.zero = (result == 0) ? 1 : 0;

        if (op == OP_ADD) {
            alu->flags.carry = add_carry;
            alu->flags.overflow = add_overflow;
        } else if (op == OP_SUB) {
            alu->flags.carry = sub_borrow;
            alu->flags.overflow = sub_overflow;
        } else if (op == OP_MUL) {
            alu->flags.overflow = (mul_overflow != 0) ? 1 : 0;
        }
    }

    return result;
}
