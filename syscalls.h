#ifndef SYSCALLS_H
#define SYSCALLS_H

// OP_SYS encoding
//
//   rs1  (bits 4-7)  -> syscall ID (this table)
//   rd   (bits 8-11) -> data register: source for output calls,
//                       destination for input calls
//   rs2  (bits 0-3)  -> reserved, currently unused
//

typedef enum {
    SYS_PUTC  = 0x0,  // putchar(R[rd] & 0xFF)
    SYS_GETC  = 0x1,  // R[rd] = getchar(), or SYS_EOF_SENTINEL on EOF
    SYS_PUTN  = 0x2,  // print R[rd] as unsigned decimal
    SYS_PUTS  = 0x3,  // print bus memory from addr R[rd], one char/word, until a 0 word
    SYS_EXIT  = 0x4,  // halt; scaffolding duplicate of OP_HALT for now
    SYS_FLUSH = 0x5,  // fflush(stdout)
} SyscallId;

// getchar() returns EOF (-1), which doesn't fit in uint16_t. 0xFFFF is
// reserved as the EOF sentinel for SYS_GETC -- not a valid character
// code from a normal stdin byte stream (max byte value is 0x00FF).
#define SYS_EOF_SENTINEL 0xFFFF

#endif // SYSCALLS_H
