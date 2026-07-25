#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Helpers
static void* xcalloc(size_t n, size_t size, const char* what) {
    void* p = calloc(n, size);
    if (!p) {
        fprintf(stderr, "CPU: allocation failed for %s\n", what);
        abort();
    }
    return p;
}

static uint16_t flags_pack(ALUFlags flags) {
    uint16_t packed = 0;
    packed |= (flags.zero << 0);
    packed |= (flags.carry << 1);
    packed |= (flags.overflow << 2);
    packed |= (flags.less << 3);
    packed |= (flags.greater << 4);
    return packed;
}

static ALUFlags flags_unpack(uint16_t packed) {
    ALUFlags flags = {0, 0, 0, 0, 0};
    flags.zero = (packed >> 0) & 1;
    flags.carry = (packed >> 1) & 1;
    flags.overflow = (packed >> 2) & 1;
    flags.less = (packed >> 3) & 1;
    flags.greater = (packed >> 4) & 1;
    return flags;
}

// Initialization
void cpu_init(CPU* cpu) {
    assert(cpu != NULL);

    // Initialize bus (bus owns the memory)
    bus_init(&cpu->bus);

    // Initialize ALU
    alu_init(&cpu->alu, BUS_DATA_BITS);
    alu_set_cmp_mode(&cpu->alu, 1);  // signed mode default

    // Clear registers
    memset(cpu->R, 0, sizeof(cpu->R));
    cpu->PC = BUS_PROGRAM_BASE;
    cpu->SP = BUS_STACK_BASE + BUS_STACK_SIZE;  // Stack grows down
    cpu->IR = 0;
    cpu->flags = (ALUFlags){0, 0, 0, 0, 0};

    // Execution state
    cpu->running = 0;
    cpu->halted = 0;
    cpu->cycles = 0;
    cpu->cmp_signed = 1;

    // Dirty flags (all clean initially)
    memset(cpu->R_dirty, 0, sizeof(cpu->R_dirty));
    cpu->PC_dirty = 0;
    cpu->SP_dirty = 0;
    cpu->flags_dirty = 0;
}

void cpu_free(CPU* cpu) {
    // Sync all registers before free
    cpu_sync_all(cpu);
    bus_free(&cpu->bus);
    alu_free(&cpu->alu);
}

// Program Loading
void cpu_load_program(CPU* cpu, const uint16_t* program, uint16_t size) {
    assert(cpu != NULL);
    assert(program != NULL);
    assert(size < BUS_PROGRAM_SIZE);

    bus_write_block(&cpu->bus, BUS_PROGRAM_BASE, program, size);
    cpu->PC = BUS_PROGRAM_BASE;
}

// Simple hex parser (space-separated hex values)
void cpu_load_hex(CPU* cpu, const char* hex_string) {
    // Parse "00 01 02 03 ..." hex format
    // TODO
    (void)cpu;
    (void)hex_string;
}

// Register Access (with dirty tracking)
uint16_t cpu_read_reg(CPU* cpu, uint8_t idx) {
    assert(idx < NUM_REGS);
    return cpu->R[idx];
}

void cpu_write_reg(CPU* cpu, uint8_t idx, uint16_t value) {
    assert(idx < NUM_REGS);
    cpu->R[idx] = value;
    cpu->R_dirty[idx] = 1;  // Mark dirty for sync
}

void cpu_sync_registers(CPU* cpu) {
    for (int i = 0; i < NUM_REGS; i++) {
        if (cpu->R_dirty[i]) {
            bus_write_sysvar(&cpu->bus, i, cpu->R[i]);
            cpu->R_dirty[i] = 0;
        }
    }
    if (cpu->PC_dirty) {
        bus_write_pc(&cpu->bus, cpu->PC);
        cpu->PC_dirty = 0;
    }
    if (cpu->SP_dirty) {
        bus_write_sp(&cpu->bus, cpu->SP);
        cpu->SP_dirty = 0;
    }
    if (cpu->flags_dirty) {
        bus_write_flags(&cpu->bus, flags_pack(cpu->flags));
        cpu->flags_dirty = 0;
    }
}

void cpu_sync_all(CPU* cpu) {
    // Mark all dirty, then sync
    for (int i = 0; i < NUM_REGS; i++) {
        cpu->R_dirty[i] = 1;
    }
    cpu->PC_dirty = 1;
    cpu->SP_dirty = 1;
    cpu->flags_dirty = 1;
    cpu_sync_registers(cpu);
}

// Instruction Fetch
static uint16_t cpu_fetch(CPU* cpu) {
    uint16_t instr = bus_read(&cpu->bus, cpu->PC);
    cpu->PC++;
    cpu->PC_dirty = 1;
    cpu->cycles++;
    return instr;
}

// Instruction Decode
static void cpu_decode(uint16_t instr, uint8_t* opcode, uint8_t* rd, uint8_t* rs1, uint8_t* rs2, uint16_t* imm) {
    *opcode = (instr >> 12) & 0xF;
    *rd = (instr >> 8) & 0xF;
    *rs1 = (instr >> 4) & 0xF;
    *rs2 = instr & 0xF;
    *imm = instr & 0xFF;
}

// Execute One Instruction
void cpu_step(CPU* cpu) {
    if (cpu->halted) return;

    cpu->running = 1;

    // 1. Fetch
    uint16_t instr = cpu_fetch(cpu);
    cpu->IR = instr;
    bus_write_ir(&cpu->bus, instr);

    // 2. Decode
    uint8_t opcode, rd, rs1, rs2;
    uint16_t imm;
    cpu_decode(instr, &opcode, &rd, &rs1, &rs2, &imm);

    // DEBUG
    //printf("[PC=0x%04X] INSTR=0x%04X OP=%d RD=%d RS1=%d RS2=%d IMM=0x%02X\n",
    //        cpu->PC-1, instr, opcode, rd, rs1, rs2, imm);

    // 3. Execute
    switch (opcode) {
        // ALU-family opcodes are routed through alu_forward().
        // flag semantics (Z/C/OV/L/G) exactly match the spec table.
        case OP_ADD:
        case OP_SUB:
        case OP_AND:
        case OP_OR:
        case OP_XOR:
        case OP_MUL: {
            uint16_t result = alu_forward(&cpu->alu, cpu->R[rs1], cpu->R[rs2], opcode);
            cpu->R[rd] = result;
            cpu->R_dirty[rd] = 1;
            cpu->flags = cpu->alu.flags;
            cpu->flags_dirty = 1;
            break;
        }

        case OP_CMP:
            alu_forward(&cpu->alu, cpu->R[rs1], cpu->R[rs2], opcode);
            cpu->flags = cpu->alu.flags;
            cpu->flags_dirty = 1;
            break;

        case OP_LDI:
            cpu->R[rd] = imm;
            cpu->R_dirty[rd] = 1;
            break;

        case OP_LDI16: {
            // 2-word instruction: fetch immediate via cpu_fetch,
            // PC advance and cycle accounting stay consistent
            uint16_t imm16 = cpu_fetch(cpu);
            cpu->R[rd] = imm16;
            cpu->R_dirty[rd] = 1;
            break;
        }

        case OP_LD:
            cpu->R[rd] = bus_read(&cpu->bus, imm);
            cpu->R_dirty[rd] = 1;
            break;

        case OP_ST:
            bus_write(&cpu->bus, imm, cpu->R[rs1]);
            break;

        case OP_JMP:
            cpu->PC = imm;
            cpu->PC_dirty = 1;
            break;

        case OP_JZ:
            if (cpu->flags.zero) {
                cpu->PC = imm;
                cpu->PC_dirty = 1;
            }
            break;

        case OP_JNZ:
            if (!cpu->flags.zero) {
                cpu->PC = imm;
                cpu->PC_dirty = 1;
            }
            break;

        case OP_SYS:
            // System call (TODO)
            break;

        case OP_HALT:
            cpu->halted = 1;
            cpu->running = 0;
            break;

        default:
            fprintf(stderr, "CPU: Unknown opcode 0x%X at PC=0x%04X\n", opcode, cpu->PC);
            cpu->halted = 1;
            cpu->running = 0;
            break;
    }

    cpu->cycles++;
}

// Until HALT
void cpu_run(CPU* cpu) {
    cpu->running = 1;
    while (cpu->running && !cpu->halted) {
        cpu_step(cpu);
    }
    cpu_sync_all(cpu);
}

// Reset
void cpu_reset(CPU* cpu) {
    cpu->PC = BUS_PROGRAM_BASE;
    cpu->PC_dirty = 1;
    cpu->halted = 0;
    cpu->running = 0;
    cpu->cycles = 0;
    memset(cpu->R, 0, sizeof(cpu->R));
    for (int i = 0; i < NUM_REGS; i++) {
        cpu->R_dirty[i] = 1;
    }
    cpu->flags = (ALUFlags){0, 0, 0, 0, 0};
    cpu->flags_dirty = 1;
    cpu->SP = BUS_STACK_BASE + BUS_STACK_SIZE;
    cpu->SP_dirty = 1;
    cpu_sync_all(cpu);
}

// Debug
void cpu_dump_registers(CPU* cpu) {
    printf("Registers:\n");
    for (int i = 0; i < NUM_REGS; i++) {
        printf("  R%-2d: 0x%04X (%5d)%s\n",
               i, cpu->R[i], cpu->R[i],
               cpu->R_dirty[i] ? " *dirty" : "");
    }
    printf("  PC:  0x%04X %s\n", cpu->PC, cpu->PC_dirty ? "*dirty" : "");
    printf("  SP:  0x%04X %s\n", cpu->SP, cpu->SP_dirty ? "*dirty" : "");
    printf("  IR:  0x%04X\n", cpu->IR);
    printf("Flags: Z=%d C=%d OV=%d L=%d G=%d %s\n",
           cpu->flags.zero, cpu->flags.carry, cpu->flags.overflow,
           cpu->flags.less, cpu->flags.greater,
           cpu->flags_dirty ? "*dirty" : "");
    printf("Cycles: %llu\n", (unsigned long long)cpu->cycles);
    printf("State: %s\n", cpu->halted ? "HALTED" : (cpu->running ? "RUNNING" : "STOPPED"));
}

void cpu_dump_state(CPU* cpu) {
    cpu_dump_registers(cpu);
    printf("\nMemory Dump (at PC offset):\n");
    bus_dump(&cpu->bus, cpu->PC - 4, 8);
}
