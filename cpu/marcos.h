// marcos.h
#ifndef AXIOMATIC_H
#define AXIOMATIC_H

#include <stdint.h>

// Register Aliases
#define R0  0
#define R1  1
#define R2  2
#define R3  3
#define R4  4
#define R5  5
#define R6  6
#define R7  7
#define R8  8
#define R9  9
#define R10 10
#define R11 11
#define R12 12
#define R13 13
#define R14 14
#define R15 15

// ALU Operations (R-R Format)
#define ADD(rd, rs1, rs2)   ((0x0 << 12) | ((rd) << 8) | ((rs1) << 4) | (rs2))
#define SUB(rd, rs1, rs2)   ((0x1 << 12) | ((rd) << 8) | ((rs1) << 4) | (rs2))
#define AND(rd, rs1, rs2)   ((0x2 << 12) | ((rd) << 8) | ((rs1) << 4) | (rs2))
#define OR(rd, rs1, rs2)    ((0x3 << 12) | ((rd) << 8) | ((rs1) << 4) | (rs2))
#define XOR(rd, rs1, rs2)   ((0x4 << 12) | ((rd) << 8) | ((rs1) << 4) | (rs2))
#define MUL(rd, rs1, rs2)   ((0x5 << 12) | ((rd) << 8) | ((rs1) << 4) | (rs2))
#define CMP(rs1, rs2)       ((0x6 << 12) | (0 << 8) | ((rs1) << 4) | (rs2))

// Immediate Loads
#define LDI(rd, imm)        ((0x7 << 12) | ((rd) << 8) | ((imm) & 0xFF))
#define LDI16(rd, imm)      ((0x8 << 12) | ((rd) << 8)), ((imm) & 0xFFFF)

// Memory Operations (8-bit)
#define LD(rd, addr)        ((0x9 << 12) | ((rd) << 8) | ((addr) & 0xFF))
#define ST(rs, addr)        ((0xA << 12) | ((rs) << 8) | ((addr) & 0xFF))

// Control Flow (8-bit)
#define JMP(addr)           ((0xB << 12) | ((addr) & 0xFF))
#define JZ(addr)            ((0xC << 12) | ((addr) & 0xFF))
#define JNZ(addr)           ((0xD << 12) | ((addr) & 0xFF))

// System Calls (16-bit Extended)
#define SYS(sys_id, rd)     ((0xE << 12) | ((rd) << 8) | ((sys_id) << 4))

// 16-bit Memory Operations via SYS
#define LD16(rd, addr)      SYS(0x6, rd), ((addr) & 0xFFFF)
#define ST16(rs, addr)      SYS(0x7, rs), ((addr) & 0xFFFF)
#define LDIND(rd)           SYS(0x8, rd)
#define STIND(rs)           SYS(0x9, rs)
#define JMP16(addr)         SYS(0xA, 0), ((addr) & 0xFFFF)
#define CALL(addr)          SYS(0xB, 0), ((addr) & 0xFFFF)
#define RET()               SYS(0xC, 0)
#define PEEK(rd, addr)      SYS(0xD, rd), ((addr) & 0xFFFF)
#define MEMCPY()            SYS(0xF, 0)

// Existing Syscalls
#define SYS_PUTC(rd)        SYS(0x0, rd)
#define SYS_GETC(rd)        SYS(0x1, rd)
#define SYS_PUTN(rd)        SYS(0x2, rd)
#define SYS_PUTS(rd)        SYS(0x3, rd)
#define SYS_EXIT()          SYS(0x4, 0)
#define SYS_FLUSH()         SYS(0x5, 0)

// Stack Macros
#define PUSH(reg)           SUB(R15, R15, 1), ST(reg, 0)  // Needs indirect!
#define POP(reg)            LD(reg, 0), ADD(R15, R15, 1)   // Needs indirect!

// Data Directives
#define WORD(val)           ((val) & 0xFFFF)
#define STRING(str)         STRING_HELPER(str, 0)
#define STRING_HELPER(str, n) \
    WORD((str)[n] ? (str)[n] : 0), \
    ((str)[n+1] ? STRING_HELPER(str, n+1) : WORD(0))

// Program Markers
#define HALT()              ((0xF << 12))
#define ORG(addr)           // Not implemented, just for readability

#endif // AXIOMATIC_H
