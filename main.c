#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "cpu.h"

// --- Timing Helpers ---
static uint64_t get_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

// --- Performance Test: Single Operation Repeated ---
typedef struct {
    const char* name;
    uint16_t instr;
    uint16_t init_instr[4];
    int init_count;
} PerfTest;

static PerfTest tests[] = {
    {.name = "ADD", .instr = 0x0012, .init_instr = {0x7105, 0x7203}, .init_count = 2},
    {.name = "SUB", .instr = 0x1012, .init_instr = {0x7108, 0x7203}, .init_count = 2},
    {.name = "AND", .instr = 0x2012, .init_instr = {0x7107, 0x7203}, .init_count = 2},
    {.name = "OR",  .instr = 0x3012, .init_instr = {0x7104, 0x7202}, .init_count = 2},
    {.name = "XOR", .instr = 0x4012, .init_instr = {0x7107, 0x7203}, .init_count = 2},
    {.name = "MUL", .instr = 0x5012, .init_instr = {0x7105, 0x7203}, .init_count = 2},
    {.name = "CMP", .instr = 0x6102, .init_instr = {0x7105, 0x7203}, .init_count = 2},
    {.name = "LDI", .instr = 0x7F42, .init_instr = {0}, .init_count = 0},
};

#define NUM_TESTS (sizeof(tests) / sizeof(PerfTest))

// --- Build unrolled program ---
// Setup once, then N repetitions of the op, then HALT
static uint16_t* build_unrolled_program(const PerfTest* test, int iterations, int* out_size) {
    int size = test->init_count + iterations + 1;
    uint16_t* prog = (uint16_t*)malloc(size * sizeof(uint16_t));
    if (!prog) return NULL;

    int idx = 0;

    // Setup: load operands into registers (never modified)
    for (int i = 0; i < test->init_count; i++) {
        prog[idx++] = test->init_instr[i];
    }

    // Unrolled operations: R0 gets overwritten each time, R1/R2 stay fixed
    for (int i = 0; i < iterations; i++) {
        prog[idx++] = test->instr;
    }

    // HALT
    prog[idx++] = 0xF000;

    *out_size = size;
    return prog;
}

// --- Run a single performance test ---
static void run_perf_test(const PerfTest* test, int iterations) {
    CPU cpu;
    cpu_init(&cpu);

    int prog_size;
    uint16_t* prog = build_unrolled_program(test, iterations, &prog_size);
    if (!prog) {
        printf("  Failed to allocate program\n");
        return;
    }

    cpu_load_program(&cpu, prog, prog_size);

    uint64_t start = get_usec();
    cpu_run(&cpu);
    uint64_t end = get_usec();

    uint64_t elapsed_us = end - start;
    double elapsed_sec = elapsed_us / 1000000.0;

    double ops_per_sec = iterations / elapsed_sec;

    // Only print ops/sec — no result checking
    printf("  %-8s | %8d ops | %8.3f sec | %10.0f ops/sec\n",
           test->name, iterations, elapsed_sec, ops_per_sec);

    cpu_free(&cpu);
    free(prog);
}

// --- Main ---
int main() {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  AXIOM DRY ALU PERFORMANCE TEST (Ops/sec per instruction)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");

    // Warm-up
    printf("Warming up...\n");
    run_perf_test(&tests[0], 100);
    printf("\n");

    int iteration_counts[] = {100, 1000, 5000, 10000, 50000};
    int num_counts = sizeof(iteration_counts) / sizeof(int);

    for (int t = 0; t < NUM_TESTS; t++) {
        printf("─── %s ───\n", tests[t].name);

        for (int c = 0; c < num_counts; c++) {
            int iters = iteration_counts[c];
            run_perf_test(&tests[t], iters);
        }
        printf("\n");
    }
    return 0;
}
