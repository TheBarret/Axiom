// hello.c - Hello World using macro-based "assembler"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu/marcos.h"
#include "cpu/cpu.h"

// Hello World
uint16_t hello_program[] = {
    // Print 'Hello, World!'
    LDI(R1, 'H'),
    SYS_PUTC(R1),
    LDI(R1, 'e'),
    SYS_PUTC(R1),
    LDI(R1, 'l'),
    SYS_PUTC(R1),
    LDI(R1, 'l'),
    SYS_PUTC(R1),
    LDI(R1, 'o'),
    SYS_PUTC(R1),
    LDI(R1, ' '),
    SYS_PUTC(R1),
    LDI(R1, 'W'),
    SYS_PUTC(R1),
    LDI(R1, 'o'),
    SYS_PUTC(R1),
    LDI(R1, 'r'),
    SYS_PUTC(R1),
    LDI(R1, 'l'),
    SYS_PUTC(R1),
    LDI(R1, 'd'),
    SYS_PUTC(R1),
    LDI(R1, '!'),
    SYS_PUTC(R1),
    LDI(R1, '\n'),
    SYS_PUTC(R1),

    // Flush & Exit
    SYS_FLUSH(),
    HALT()
};

int main() {
    CPU cpu;
    cpu_init(&cpu);
    cpu_load_program(&cpu, hello_program, sizeof(hello_program) / sizeof(uint16_t));
    cpu_run(&cpu);
    //cpu_dump_state(&cpu);
    cpu_free(&cpu);
    return 0;
}
