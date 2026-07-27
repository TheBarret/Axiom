#ifndef SYSCALLS_H
#define SYSCALLS_H

// OP_SYS encoding
//
//   rs1  (bits 4-7)  -> syscall ID (this table)
//   rd   (bits 8-11) -> data register: source for output calls,
//                       destination for input calls
//   rs2  (bits 0-3)  -> reserved, currently unused
//

// Syscalls
typedef enum {
    // Stdio features (0x0-0x5)
    SYS_PUTC  = 0x0,  // putchar(R[rd] & 0xFF)
    SYS_GETC  = 0x1,  // R[rd] = getchar(), or SYS_EOF_SENTINEL on EOF
    SYS_PUTN  = 0x2,  // print R[rd] as unsigned decimal
    SYS_PUTS  = 0x3,  // print bus memory from addr R[rd], one char/word, until a 0 word
    SYS_EXIT  = 0x4,  // halt; scaffolding duplicate of OP_HALT for now
    SYS_FLUSH = 0x5,  // fflush(stdout)

    // Memory supplementary features (0x6-0xF)
    SYS_LD16  = 0x6,  // Load from 16-bit address
    SYS_ST16  = 0x7,  // Store to 16-bit address
    SYS_LDIND = 0x8,  // Load indirect (address in register)
    SYS_STIND = 0x9,  // Store indirect (address in register)
    SYS_JMP16 = 0xA,  // Jump to 16-bit address
    SYS_CALL  = 0xB,  // Call subroutine (push PC, jump)
    SYS_RET   = 0xC,  // Return from subroutine
    SYS_PEEK  = 0xD,  // Read memory without destructive read
    SYS_POKE  = 0xE,  // Write memory without side effects
    SYS_MEMCPY= 0xF,  // Block memory copy
} SyscallId;

// getchar() returns EOF (-1), which doesn't fit in uint16_t. 0xFFFF is
// reserved as the EOF sentinel for SYS_GETC -- not a valid character
// code from a normal stdin byte stream (max byte value is 0x00FF).
#define SYS_EOF_SENTINEL 0xFFFF

// Macro's
#define LD16(reg, addr) \
    LDI16 reg, addr;   \
    SYS SYS_LD16, reg  // this needs the address in reg

// Better: Use SYS with next word
#define LD16(reg, addr) \
    SYS SYS_LD16, reg; \
    .word addr          // Next word is the address

#define ST16(reg, addr) \
    SYS SYS_ST16, reg; \
    .word addr

#define JMP16(addr) \
    SYS SYS_JMP16, 0; \
    .word addr

#define CALL(addr) \
    SYS SYS_CALL, 0; \
    .word addr

#define RET() \
    SYS SYS_RET, 0

#define LDIND(reg) \
    SYS SYS_LDIND, reg  // Address in reg

#define STIND(reg) \
    SYS SYS_STIND, reg  // Address in reg, value in reg+1

#define PEEK(reg, addr) \
    SYS SYS_PEEK, reg; \
    .word addr

#define MEMCPY(src, dst, count) \
    SYS SYS_MEMCPY, 0; \
    /* Uses R0, R1, R2 for src, dst, count */

#endif // SYSCALLS_H
