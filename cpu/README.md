# Axiom CPU Model

- **LDI16** is a **2-word instruction** - the immediate value is fetched from the next memory word
- **All ALU operations** are computed every cycle, then mux-selected (compute-all model)
- **CMP** sets flags but doesn't write a result register
- **MUL** sets OV if product doesn't fit in 16 bits
- **Default comparison mode** is signed (can be changed via `alu_set_cmp_mode`)
- **System calls** use `rs1` for syscall ID and `rd` for data register
- **EOF sentinel** for SYS_GETC is `0xFFFF` (invalid character code)

## Memory Map (CCEL/BUS)

```
┌─────────────────────────────────────┐
│ 0x0000 - 0x1FFF  Program Memory     │ 8,192 words
├─────────────────────────────────────┤
│ 0x2000 - 0x2FFF  Data Memory        │ 4,096 words
├─────────────────────────────────────┤
│ 0x3000 - 0x3FFF  Stack Memory       │ 4,096 words
├─────────────────────────────────────┤
│ 0x4000 - 0xFFDF  Free Space         │ 49,152 words
├─────────────────────────────────────┤
│ 0xFFE0 - 0xFFF7  System Variables   │ 24 words
└─────────────────────────────────────┘
```

## Instruction Encoding

All instructions are **16-bit** with the following format:
```
┌─────────────┬─────────────┬─────────────┬─────────────┐
│  OPCODE     │  RD/RS1     │  RS1/RS2    │  RS2/IMM    │
│  Bits 15-12 │  Bits 11-8  │  Bits 7-4   │  Bits 3-0   │
└─────────────┴─────────────┴─────────────┴─────────────┘
```

| Field | Bits | Description |
|-------|------|-------------|
| **OPCODE** | 15-12 | Operation (0x0-0xF) |
| **RD** | 11-8 | Destination Register |
| **RS1** | 7-4 | Source Register 1 |
| **RS2/IMM** | 3-0 | Source Register 2 or 4-bit Immediate |

---

## CPU Cycle Diagram

```
    ┌─────────────────────────────────────────────────────────────┐
    │                    CPU Loop                                 │
    │                                                             │
    │  1. ALU computes result & flags                             │
    │     ↓                                                       │
    │  2. CPU stores result in R[n] (cached)                      │
    │     CPU.flags = alu.flags                                   │
    │     cpu->R_dirty[n] = 1                                     │
    │     cpu->flags_dirty = 1  ← Not written yet!                │
    │     ↓                                                       │
    │  3. Continue executing...                                   │
    │     ↓                                                       │
    │  4. On halt or sync request:                                │
    │     cpu_sync_all() → flags_pack() → bus_write_flags()       │
    │     ↓                                                       │
    │  5. bus_write_flags() → bus_write() → bus_tick()            │
    │     ↓                                                       │
    │  6. bus_tick() writes to CCEL planes at addr 0xFFE2         │
    │     ↓                                                       │
    │  7. CCEL memory: 16 bits written across 16 CCEL cells       │
    │     Each bit stored in a separate trinary CCEL cell         │
    └─────────────────────────────────────────────────────────────┘
```

## Opcode Table

| Opcode | Value | Format | Operation | Flags Set |
|--------|-------|--------|-----------|-----------|
| **ADD** | 0x0 | `ADD Rd, Rs1, Rs2` | `Rd = Rs1 + Rs2` | Z, C, OV |
| **SUB** | 0x1 | `SUB Rd, Rs1, Rs2` | `Rd = Rs1 - Rs2` | Z, C, OV |
| **AND** | 0x2 | `AND Rd, Rs1, Rs2` | `Rd = Rs1 & Rs2` | Z |
| **OR** | 0x3 | `OR Rd, Rs1, Rs2` | `Rd = Rs1 \| Rs2` | Z |
| **XOR** | 0x4 | `XOR Rd, Rs1, Rs2` | `Rd = Rs1 ^ Rs2` | Z |
| **MUL** | 0x5 | `MUL Rd, Rs1, Rs2` | `Rd = Rs1 * Rs2` | Z, OV |
| **CMP** | 0x6 | `CMP Rs1, Rs2` | Compare Rs1 vs Rs2 | Z, L, G |
| **LDI** | 0x7 | `LDI Rd, Imm4` | `Rd = Imm4` (zero-extended) | - |
| **LDI16** | 0x8 | `LDI16 Rd, Imm16` | `Rd = Imm16` (2-word) | - |
| **LD** | 0x9 | `LD Rd, Addr8` | `Rd = MEM[Addr8]` | - |
| **ST** | 0xA | `ST Rs1, Addr8` | `MEM[Addr8] = Rs1` | - |
| **JMP** | 0xB | `JMP Addr8` | `PC = Addr8` | - |
| **JZ** | 0xC | `JZ Addr8` | if Z: `PC = Addr8` | - |
| **JNZ** | 0xD | `JNZ Addr8` | if !Z: `PC = Addr8` | - |
| **SYS** | 0xE | `SYS Rs1, Rd` | System call (Rs1=ID, Rd=data) | - |
| **HALT** | 0xF | `HALT` | Stop execution | - |

---

## Instruction Formats

### R-R Format (ALU Operations)
```
┌──────┬──────────┬──────────┬──────────┐
│ OP   │ RD       │ RS1      │ RS2      │
│ 4b   │ 4b       │ 4b       │ 4b       │
└──────┴──────────┴──────────┴──────────┘
```
**Used by:** ADD, SUB, AND, OR, XOR, MUL, CMP

**Example:** `ADD R0, R1, R2` → `0x0012`
```
OP=0x0, RD=0, RS1=1, RS2=2
Hex: 0x0012
```

### R-I Format (Immediate)
```
┌──────┬──────────┬──────────┬──────────┐
│ OP   │ RD       │ IMM      │ IMM      │
│ 4b   │ 4b       │ 4b       │ 4b       │
└──────┴──────────┴──────────┴──────────┘
```
**Used by:** LDI

**Example:** `LDI R1, 5` → `0x7105`
```
OP=0x7, RD=1, IMM=5
Hex: 0x7105
```

### I-Format (Memory/Jump)
```
┌──────┬──────────┬──────────┬──────────┐
│ OP   │ RS1/RD   │ ADDR     │ ADDR     │
│ 4b   │ 4b       │ 4b       │ 4b       │
└──────┴──────────┴──────────┴──────────┘
```
**Used by:** LD, ST, JMP, JZ, JNZ

**Example:** `LD R0, 0x12` → `0x9012`
```
OP=0x9, RD=0, ADDR=0x12
Hex: 0x9012
```

### SYS Format
```
┌──────┬──────────┬──────────┬──────────┐
│ OP   │ RD       │ SYS_ID   │ RESERVED │
│ 4b   │ 4b       │ 4b       │ 4b       │
└──────┴──────────┴──────────┴──────────┘
```
**Used by:** SYS

**Example:** `SYS PUTC, R0` → `0xE000`
```
OP=0xE, RD=0 (data register), SYS_ID=0 (PUTC)
Hex: 0xE000
```

---

## Flags Reference

| Flag | Abbr | Description | Set When |
|------|------|-------------|----------|
| **Z** | Zero | Result is zero | All ops except CMP set Z based on result |
| **C** | Carry | Carry/Borrow out | ADD (carry), SUB (borrow) |
| **OV** | Overflow | Signed overflow | ADD, SUB, MUL (product doesn't fit) |
| **L** | Less | Comparison less-than | CMP only (signed/unsigned mode) |
| **G** | Greater | Comparison greater-than | CMP only (signed/unsigned mode) |

### ALU Operation Details

**ADD**
```
Result = A + B (mod 2^bits)
Z = (Result == 0)
C = carry-out from MSB
OV = carry[bits] ^ carry[bits-1]
```

**SUB**
```
Result = A - B (mod 2^bits)
Z = (Result == 0)
C = borrow (inverse of carry-out)
OV = carry[bits] ^ carry[bits-1]
```

**AND, OR, XOR**
```
Result = bitwise operation
Z = (Result == 0)
Other flags = 0
```

**MUL**
```
Result = A * B (low bits)
Z = (Result == 0)
OV = (Product > 2^bits - 1)
```

**CMP**
```
Compare A vs B (signed or unsigned mode)
Z = (A == B)
L = (A < B)  // Signed or unsigned depending on mode
G = (A > B)  // Signed or unsigned depending on mode
Result register = 0
```

**Signed vs Unsigned Comparison**
- **Default:** Signed mode (`cmp_signed = 1`)
- **Signed mode:** L uses `sign(A-B) XOR overflow(A-B)`
- **Unsigned mode:** L uses borrow from subtraction

Set mode via: `alu_set_cmp_mode(&alu, 1)` (signed) or `alu_set_cmp_mode(&alu, 0)` (unsigned)

---

---

## Complete Test Program

```c
// Test program demonstrating all ALU operations
uint16_t test_program[] = {
    // Load test values
    0x7105,  // LDI R1, 5       (R1 = 5)
    0x7203,  // LDI R2, 3       (R2 = 3)
    0x7307,  // LDI R3, 7       (R3 = 7)

    // ALU Operations
    0x0012,  // ADD R0, R1, R2  (R0 = R1 + R2 = 5 + 3 = 8)
    0x1102,  // SUB R1, R0, R2  (R1 = R0 - R2 = 8 - 3 = 5)
    0x2103,  // AND R1, R0, R3  (R1 = R0 & R3 = 8 & 7 = 0)
    0x3102,  // OR  R1, R0, R2  (R1 = R0 | R2 = 8 | 3 = 11)
    0x4103,  // XOR R1, R0, R3  (R1 = R0 ^ R3 = 8 ^ 7 = 15)
    0x5012,  // MUL R0, R1, R2  (R0 = R1 * R2 = 15 * 3 = 45)

    // Compare
    0x6103,  // CMP R0, R3     (Compare R0(45) vs R3(7) → G=1)

    // HALT
    0xF000   // HALT
};
```
---

**System Variables:**
| Address | Name | Description |
|---------|------|-------------|
| 0xFFE0 | PC | Program Counter |
| 0xFFE1 | SP | Stack Pointer |
| 0xFFE2 | FLAGS | Condition Flags |
| 0xFFE3 | IR | Instruction Register |
| 0xFFE4-0xFFE7 | CYCLES | Cycle Counter (64-bit) |
| 0xFFE8-0xFFF7 | RESERVED | Future use |

## Instruction Quick Reference Card

### ALU Operations
```
ADD Rd, Rs1, Rs2    # Rd = Rs1 + Rs2
SUB Rd, Rs1, Rs2    # Rd = Rs1 - Rs2
AND Rd, Rs1, Rs2    # Rd = Rs1 & Rs2
OR  Rd, Rs1, Rs2    # Rd = Rs1 | Rs2
XOR Rd, Rs1, Rs2    # Rd = Rs1 ^ Rs2
MUL Rd, Rs1, Rs2    # Rd = Rs1 * Rs2
CMP Rs1, Rs2        # Compare, set flags
```

### Immediate Loads
```
LDI Rd, Imm4        # Rd = Imm4 (4-bit zero-extended)
LDI16 Rd, Imm16     # Rd = Imm16 (2-word immediate)
```

### Memory Operations
```
LD Rd, Addr8        # Rd = MEM[Addr8]
ST Rs1, Addr8       # MEM[Addr8] = Rs1
```

### Control Flow
```
JMP Addr8           # PC = Addr8
JZ Addr8            # if Z: PC = Addr8
JNZ Addr8           # if !Z: PC = Addr8
HALT                # Stop execution
SYS SysId, Rd       # System call (SysId in Rs1)
```

### System Calls

| ID | Name | Parameters | Description |
|----|------|------------|-------------|
| 0x0 | SYS_PUTC | R[rd] | putchar(R[rd] & 0xFF) |
| 0x1 | SYS_GETC | R[rd] | R[rd] = getchar() |
| 0x2 | SYS_PUTN | R[rd] | printf("%u", R[rd]) |
| 0x3 | SYS_PUTS | R[rd] | Print string at R[rd] |
| 0x4 | SYS_EXIT | - | Halt execution with exit code (todo) |
| 0x5 | SYS_FLUSH | - | fflush(stdout) |
| **0x6** | **SYS_LD16** | **R[rd], next word** | **Load from 16-bit address** |
| **0x7** | **SYS_ST16** | **R[rd], next word** | **Store to 16-bit address** |
| **0x8** | **SYS_LDIND** | **R[rd]** | **Load indirect (address in reg)** |
| **0x9** | **SYS_STIND** | **R[rd]** | **Store indirect (addr in rd, value in rd+1)** |
| **0xA** | **SYS_JMP16** | **next word** | **Jump to 16-bit address** |
| **0xB** | **SYS_CALL** | **next word** | **Call subroutine at 16-bit address** |
| **0xC** | **SYS_RET** | **-** | **Return from subroutine** |
| **0xD** | **SYS_PEEK** | **R[rd], next word** | **Read without destructive side-effects** |
| **0xE** | **SYS_POKE** | **R[rd], next word** | **Write without side-effects** |
| **0xF** | **SYS_MEMCPY** | **R[rd]=src, R[rd+1]=dst, R[rd+2]=count** | **Block memory copy** |

**SYS Instruction Encoding:**
- `rd` (bits 11-8): Data register (source for output, destination for input)
- `rs1` (bits 7-4): Syscall ID
- `rs2` (bits 3-0): Reserved (unused)

---
