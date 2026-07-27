#ifndef ALU_H
#define ALU_H

#include <stdint.h>
#include <stdbool.h>
#include "../gates.h"
#include "../adder.h"

typedef enum {
    OP_ADD   = 0x0,
    OP_SUB   = 0x1,
    OP_AND   = 0x2,
    OP_OR    = 0x3,
    OP_XOR   = 0x4,
    OP_MUL   = 0x5,
    OP_CMP   = 0x6,
    OP_LDI   = 0x7,
    OP_LDI16 = 0x8,
    OP_LD    = 0x9,
    OP_ST    = 0xA,
    OP_JMP   = 0xB,
    OP_JZ    = 0xC,
    OP_JNZ   = 0xD,
    OP_SYS   = 0xE,
    OP_HALT  = 0xF
} Opcode;

typedef struct {
    int zero;
    int carry;
    int overflow;
    int less;
    int greater;
} ALUFlags;

// --- Subtractor ---
// v2: no A==B fast-path bypass. Every subtraction runs the full
// NOT + two-adder pipeline, unconditionally. Two reasons, not just one:
//   1. Authenticity -- there's no such shortcut in gate-level hardware.
//   2. Correctness -- the ALU now reads sub->adder.carries after the
//      call to derive the overflow flag. A native-C bypass would leave
//      that array stale from some earlier call, silently producing a
//      wrong overflow flag for every A==B subtraction. This was a real
//      bug waiting to happen once flags started depending on adder
//      internals, not just a style objection.
typedef struct {
    int bits;
    NotGate not_gate;
    Adder adder;
    uint64_t mask;
    int* B_bits;      // B unpacked into bits (NOT gate's input)
    int* B_inv_bits;  // ~B (NOT gate's output) -- kept separate from
                       // B_bits so the restrict-qualified not_gate_forward
                       // call never aliases its in/out pointers
} Subtractor;

void subtractor_init(Subtractor* sub, int bits);
void subtractor_free(Subtractor* sub);
int  subtractor_forward(Subtractor* sub, uint64_t A, uint64_t B, uint64_t* result);

// --- Multiplier (Shift-and-Add) ---
// v2: constant-time w.r.t. B's bit pattern. v1 skipped the adder call
// entirely when a bit of B was 0 (`if ((B >> i) & 1)`), which made the
// loop's cost data-dependent -- a real timing side channel, and also
// inconsistent with the "compute everything, every time" model the
// rest of the ALU uses. v2 always calls adder_forward exactly `bits`
// times; the skipped terms are masked to zero via an AND gate instead
// of being skipped via a branch.
typedef struct {
    int bits;
    Gate and_gate;
    Adder adder;         // 2*bits wide, holds the full product
    uint64_t mask;
    uint64_t full_mask;
    int* b_bcast;         // width 2*bits: b_bit fanned out (wiring, not a gate)
    int* shifted_bits;    // width 2*bits: (A << i) as bits
    int* masked_bits;     // width 2*bits: shifted_bits AND b_bcast
} SAMultiplier;

void multiplier_init(SAMultiplier* mul, int bits);
void multiplier_free(SAMultiplier* mul);
uint64_t multiplier_forward(SAMultiplier* mul, uint64_t A, uint64_t B);

// --- BitWise Logic ---
typedef struct {
    int bits;
    int gate_type; // 0=AND, 1=OR, 2=XOR
    Gate and_gate;
    Gate or_gate;
    XorGate xor_gate;
    int* A_bits;
    int* B_bits;
    int* result_bits;
} BitWiseLogic;

void bitwise_init(BitWiseLogic* bw, int bits, int gate_type);
void bitwise_free(BitWiseLogic* bw);
uint64_t bitwise_forward(BitWiseLogic* bw, uint64_t A, uint64_t B);

// --- Comparator ---
// v2: equality is a fold, not a scan. v1's equality check was a
// sequential loop with an early `break` -- the number of iterations
// executed depended on where A and B first differed, a data-dependent
// timing signal. v2 XORs A and B (as before) and then OR-reduces the
// result down to a single bit via a fixed-depth pairwise tree
// (log2(bits) passes, same shape as the adder's Kogge-Stone prefix,
// but a fold instead of a scan since OR is associative and we only
// need the combined result, not per-position partials). No branch, no
// early exit, same cost for every input.
//
// Ordering (less_than/greater_than) is NOT computed here anymore --
// see the note in alu.h's alu_forward. It's derived from the
// subtractor's own sign/overflow/borrow flags instead of a second,
// separate comparison tree, since A-B already tells you the ordering
// for free once you have it. `equal` is the only field this function
// sets; less_than/greater_than are left 0 and are the ALU's job.
typedef struct {
    int bits;
    Gate or_gate;
    XorGate xor_gate;
    int* A_bits;
    int* B_bits;
    int* xor_results;
} Comparator;

typedef struct {
    int equal;
    int less_than;
    int greater_than;
} CmpResult;

void comparator_init(Comparator* cmp, int bits);
void comparator_free(Comparator* cmp);
CmpResult comparator_forward(Comparator* cmp, uint64_t A, uint64_t B);

// --- ALU ---
// v2: width-generic. v1 was parameterized at init (`alu_init(&alu bits)`)
// but alu_forward hardcoded `& 0xFFFF` on every result and `>> 16`
// for mul overflow, silently truncating anything wider than 16 bits
// regardless of what bits was. v2 computes a mask from `bits` at init
// and uses it everywhere a width-dependent mask is needed,
// so the same structure works at 8/16/32/64 bits.
typedef struct {
    int bits;
    uint64_t mask;
    int cmp_signed;        // 1 = signed compare (default), 0 = unsigned

    Adder adder;
    Subtractor subtractor;
    BitWiseLogic and_logic;
    BitWiseLogic or_logic;
    BitWiseLogic xor_logic;
    SAMultiplier multiplier;
    Comparator comparator;
    ALUFlags flags;
} ALU;

void alu_init(ALU* alu, int bits);
void alu_free(ALU* alu);
void alu_set_cmp_mode(ALU* alu, int is_signed);
uint64_t alu_forward(ALU* alu, uint64_t A, uint64_t B, Opcode op);

#endif // ALU_H
