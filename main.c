#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "gates.h"
#include "adder.h"
#include "cpu.h"

/*
  Testers:
  - Gates
  - Adder
  - Cpu
  - Bus
  - Memory
*/

// Sample program: R1 = 5 + 3
uint16_t test_program[] = {
    0x7105,  // LDI R1, 5
    0x7203,  // LDI R2, 3
    0x0120,  // ADD R1, R1, R2
    0xF000   // HALT
};

#define TEST_PASSED 0
#define TEST_FAILED 1
#define MAX_TESTS 100

typedef struct {
    char name[64];
    int passed;
    int total;
    int failures[20];
    char failure_msgs[20][128];
} TestSuite;


void test_suite_init(TestSuite* ts, const char* name) {
    strncpy(ts->name, name, 63);
    ts->name[63] = '\0';
    ts->passed = 0;
    ts->total = 0;
    memset(ts->failures, 0, sizeof(ts->failures));
    memset(ts->failure_msgs, 0, sizeof(ts->failure_msgs));
}

void test_assert(TestSuite* ts, int condition, const char* msg) {
    ts->total++;
    if (!condition) {
        int idx = ts->passed;
        ts->failures[idx] = ts->total;
        strncpy(ts->failure_msgs[idx], msg, 127);
        ts->failure_msgs[idx][127] = '\0';
    } else {
        ts->passed++;
    }
}

void test_suite_summary(TestSuite* ts) {
    printf("\nTesting %s\n", ts->name);
    printf("Passed: %d/%d\n", ts->passed, ts->total);
    if (ts->passed < ts->total) {
        printf("Failures:\n");
        for (int i = 0; i < (ts->total - ts->passed); i++) {
            printf("  Test #%d: %s\n", ts->failures[i], ts->failure_msgs[i]);
        }
    }
}

// GATE TESTS

void test_gate_forward_single() {
    TestSuite ts;
    test_suite_init(&ts, "gate_forward_single");

    Gate and_gate = AND_GATE;
    Gate or_gate = OR_GATE;
    Gate nand_gate = NAND_GATE;
    Gate nor_gate = NOR_GATE;

    // AND truth table
    test_assert(&ts, gate_forward_single(&and_gate, 0, 0) == 0, "AND 0,0");
    test_assert(&ts, gate_forward_single(&and_gate, 0, 1) == 0, "AND 0,1");
    test_assert(&ts, gate_forward_single(&and_gate, 1, 0) == 0, "AND 1,0");
    test_assert(&ts, gate_forward_single(&and_gate, 1, 1) == 1, "AND 1,1");

    // OR truth table
    test_assert(&ts, gate_forward_single(&or_gate, 0, 0) == 0, "OR 0,0");
    test_assert(&ts, gate_forward_single(&or_gate, 0, 1) == 1, "OR 0,1");
    test_assert(&ts, gate_forward_single(&or_gate, 1, 0) == 1, "OR 1,0");
    test_assert(&ts, gate_forward_single(&or_gate, 1, 1) == 1, "OR 1,1");

    // NAND truth table
    test_assert(&ts, gate_forward_single(&nand_gate, 0, 0) == 1, "NAND 0,0");
    test_assert(&ts, gate_forward_single(&nand_gate, 0, 1) == 1, "NAND 0,1");
    test_assert(&ts, gate_forward_single(&nand_gate, 1, 0) == 1, "NAND 1,0");
    test_assert(&ts, gate_forward_single(&nand_gate, 1, 1) == 0, "NAND 1,1");

    // NOR truth table
    test_assert(&ts, gate_forward_single(&nor_gate, 0, 0) == 1, "NOR 0,0");
    test_assert(&ts, gate_forward_single(&nor_gate, 0, 1) == 0, "NOR 0,1");
    test_assert(&ts, gate_forward_single(&nor_gate, 1, 0) == 0, "NOR 1,0");
    test_assert(&ts, gate_forward_single(&nor_gate, 1, 1) == 0, "NOR 1,1");

    test_suite_summary(&ts);
}

void test_gate_init() {
    TestSuite ts;
    test_suite_init(&ts, "gate_init");

    Gate g1, g2, g3, g4;

    and_gate_init(&g1);
    test_assert(&ts, g1.w1 == 1.0f && g1.w2 == 1.0f && g1.bias == -1.5f, "and_gate_init");

    or_gate_init(&g2);
    test_assert(&ts, g2.w1 == 1.0f && g2.w2 == 1.0f && g2.bias == -0.5f, "or_gate_init");

    nand_gate_init(&g3);
    test_assert(&ts, g3.w1 == -1.0f && g3.w2 == -1.0f && g3.bias == 1.5f, "nand_gate_init");

    nor_gate_init(&g4);
    test_assert(&ts, g4.w1 == -1.0f && g4.w2 == -1.0f && g4.bias == 0.5f, "nor_gate_init");

    test_suite_summary(&ts);
}

void test_not_gate() {
    TestSuite ts;
    test_suite_init(&ts, "not_gate");

    NotGate not_gate;
    not_gate_init(&not_gate);

    test_assert(&ts, not_gate.w1 == -1.0f && not_gate.bias == 0.5f, "not_gate_init");

    test_assert(&ts, not_gate_forward_single(&not_gate, 0) == 1, "NOT 0");
    test_assert(&ts, not_gate_forward_single(&not_gate, 1) == 0, "NOT 1");

    // Batched
    int input[4] = {0, 1, 0, 1};
    int output[4] = {0};
    int expected[4] = {1, 0, 1, 0};

    not_gate_forward(&not_gate, input, 4, output);
    test_assert(&ts, memcmp(output, expected, 4 * sizeof(int)) == 0, "NOT batched");

    test_suite_summary(&ts);
}

void test_gate_forward_batched() {
    TestSuite ts;
    test_suite_init(&ts, "gate_forward_batched");

    Gate g = AND_GATE;
    int x1[4] = {0, 0, 1, 1};
    int x2[4] = {0, 1, 0, 1};
    int out[4];
    int expected[4] = {0, 0, 0, 1};

    gate_forward(&g, x1, x2, 4, out);
    test_assert(&ts, memcmp(out, expected, 4 * sizeof(int)) == 0, "AND batched");

    test_suite_summary(&ts);
}

void test_xor_gate() {
    TestSuite ts;
    test_suite_init(&ts, "xor_gate");

    XorGate xor_gate;
    xor_gate_init(&xor_gate, 4);

    // Scalar
    test_assert(&ts, xor_gate_forward_single(&xor_gate, 0, 0) == 0, "XOR 0,0");
    test_assert(&ts, xor_gate_forward_single(&xor_gate, 0, 1) == 1, "XOR 0,1");
    test_assert(&ts, xor_gate_forward_single(&xor_gate, 1, 0) == 1, "XOR 1,0");
    test_assert(&ts, xor_gate_forward_single(&xor_gate, 1, 1) == 0, "XOR 1,1");

    // Batched
    int x1[4] = {0, 0, 1, 1};
    int x2[4] = {0, 1, 0, 1};
    int out[4];
    int expected[4] = {0, 1, 1, 0};

    xor_gate_forward(&xor_gate, x1, x2, 4, out);
    test_assert(&ts, memcmp(out, expected, 4 * sizeof(int)) == 0, "XOR batched");

    xor_gate_free(&xor_gate);
    test_suite_summary(&ts);
}

// ADDER TESTS

void test_adder_basic_operations() {
    TestSuite ts;
    test_suite_init(&ts, "adder_basic");

    Adder adder;
    adder_init(&adder, 8);

    uint64_t result;
    int carry;

    // 1 + 1 = 2
    carry = adder_forward(&adder, 1, 1, 0, &result);
    test_assert(&ts, result == 2, "1+1=2");
    test_assert(&ts, carry == 0, "1+1 carry=0");

    // 255 + 1 = 0 (overflow, 8-bit)
    carry = adder_forward(&adder, 255, 1, 0, &result);
    test_assert(&ts, result == 0, "255+1=0 (wrap)");
    test_assert(&ts, carry == 1, "255+1 carry=1");

    // 0 + 0 = 0
    carry = adder_forward(&adder, 0, 0, 0, &result);
    test_assert(&ts, result == 0, "0+0=0");
    test_assert(&ts, carry == 0, "0+0 carry=0");

    // 128 + 128 = 0 (8-bit overflow)
    carry = adder_forward(&adder, 128, 128, 0, &result);
    test_assert(&ts, result == 0, "128+128=0");
    test_assert(&ts, carry == 1, "128+128 carry=1");

    adder_free(&adder);
    test_suite_summary(&ts);
}

void test_adder_with_carry_in() {
    TestSuite ts;
    test_suite_init(&ts, "adder_carry_in");

    Adder adder;
    adder_init(&adder, 8);

    uint64_t result;
    int carry;

    // 0 + 0 + cin=1 = 1
    carry = adder_forward(&adder, 0, 0, 1, &result);
    test_assert(&ts, result == 1, "0+0+cin=1");
    test_assert(&ts, carry == 0, "0+0+cin carry=0");

    // 255 + 0 + cin=1 = 0 (wrap)
    carry = adder_forward(&adder, 255, 0, 1, &result);
    test_assert(&ts, result == 0, "255+0+cin=0");
    test_assert(&ts, carry == 1, "255+0+cin carry=1");

    // 1 + 1 + cin=1 = 3
    carry = adder_forward(&adder, 1, 1, 1, &result);
    test_assert(&ts, result == 3, "1+1+cin=3");
    test_assert(&ts, carry == 0, "1+1+cin carry=0");

    adder_free(&adder);
    test_suite_summary(&ts);
}

void test_adder_bit_widths() {
    TestSuite ts;
    test_suite_init(&ts, "adder_bit_widths");

    uint64_t result;
    int carry;

    // 4-bit
    Adder adder4;
    adder_init(&adder4, 4);
    carry = adder_forward(&adder4, 7, 1, 0, &result);
    test_assert(&ts, result == 8, "4-bit: 7+1=8");
    test_assert(&ts, carry == 0, "4-bit: carry=0");

    carry = adder_forward(&adder4, 15, 1, 0, &result);
    test_assert(&ts, result == 0, "4-bit: 15+1=0");
    test_assert(&ts, carry == 1, "4-bit: carry=1");
    adder_free(&adder4);

    // 16-bit
    Adder adder16;
    adder_init(&adder16, 16);
    carry = adder_forward(&adder16, 65535, 1, 0, &result);
    test_assert(&ts, result == 0, "16-bit: 65535+1=0");
    test_assert(&ts, carry == 1, "16-bit: carry=1");
    adder_free(&adder16);

    // 64-bit (edge case)
    Adder adder64;
    adder_init(&adder64, 64);
    carry = adder_forward(&adder64, 0xFFFFFFFFFFFFFFFFULL, 1, 0, &result);
    test_assert(&ts, result == 0, "64-bit: max+1=0");
    test_assert(&ts, carry == 1, "64-bit: carry=1");
    adder_free(&adder64);

    test_suite_summary(&ts);
}

void test_adder_kogge_stone_carries() {
    TestSuite ts;
    test_suite_init(&ts, "adder_kogge_stone");

    Adder adder;
    adder_init(&adder, 4);

    // Force computation and inspect internal carries
    uint64_t result;
    adder_forward(&adder, 5, 3, 0, &result); // 0101 + 0011 = 1000

    // Internal carries should be: [0, G0, G1, G2, G3]
    // For 5+3: G = [1, 0, 0, 0], P = [0, 1, 1, 0]
    // Carries should reflect proper prefix
    test_assert(&ts, adder.carries[0] == 0, "carry[0]=cin");
    test_assert(&ts, adder.carries[4] == 0, "carry-out=0");

    // Add with overflow: 7+9=0 (4-bit)
    adder_forward(&adder, 7, 9, 0, &result);
    test_assert(&ts, result == 0, "7+9=0 (4-bit wrap)");
    test_assert(&ts, adder.carries[4] == 1, "carry-out=1");

    adder_free(&adder);
    test_suite_summary(&ts);
}

void test_adder_debug_output() {
    Adder adder;
    adder_init(&adder, 4);

    uint64_t result;
    adder_forward(&adder, 5, 3, 0, &result);

    printf("\nDebug start\n");
    adder_debug(&adder, "5+3=8");

    adder_free(&adder);
    printf("Debug end\n");
}

void test_cpu() {
    CPU cpu;
    cpu_init(&cpu);

    printf("Loading program...\n");
    cpu_load_program(&cpu, test_program, 4);

    printf("Initial state:\n");
    cpu_dump_registers(&cpu);

    printf("\nExecuting...\n");
    cpu_run(&cpu);

    printf("\nFinal state:\n");
    cpu_dump_state(&cpu);

    cpu_free(&cpu);
}

int main() {
    printf("* * Testing: Axiom Gates...\n");

    test_gate_forward_single();
    test_gate_init();
    test_not_gate();
    test_gate_forward_batched();
    test_xor_gate();

    printf("\n* * Testing: Axiom Adders...\n");

    test_adder_basic_operations();
    test_adder_with_carry_in();
    test_adder_bit_widths();
    test_adder_kogge_stone_carries();
    test_adder_debug_output();

    printf("\n* * Testing: Axiom Cpu...\n");
    test_cpu();

    printf("\nFinished!\n");
    return 0;
}
