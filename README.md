# Axiom

### The MCP Neuron Foundation

The artificial neuron was first proposed by Warren McCulloch and Walter Pitts in their 1943 paper,  
the `Threshold Logic Unit` (TLU) the simplest type of artificial neuron and binary classifier,  
functioning by multiplying inputs by weights, summing them, and outputting a `1`,  
if the sum meets or exceeds a set threshold.   

In the Axiom framework, every logic gate in the system is built from MCP neurons:
- **Inputs** are multiplied by weights and summed with a bias
- **Output** is 1 if the sum meets or exceeds a threshold, otherwise 0
- Logic gates (AND, OR, NAND, NOR, NOT, XOR) are constructed using specific weight/threshold combinations

## Solving the XOR Problem

The XOR function is famously not linearly separable, meaning it cannot be solved by a single McCulloch-Pitts neuron (a single threshold unit).  
To overcome this limitation, the Axiom project implements XOR as a **two-layer MCP neural network**.  

**Workflow:**
```
Input (A, B) → Layer 1 → Layer 2 → Output
                ↓           ↓
              OR gate    NAND gate
                ↓           ↓
                └─── AND ───┘
```

**Layer 1: Compute Intermediate Functions**
- **OR gate**: Outputs 1 when A=1 or B=1 (or both)
- **NAND gate**: Outputs 1 except when both A=1 and B=1

**Both gates are standard MCP neurons with specific weight/threshold configurations:**
- OR: weights `[1.0, 1.0]`, threshold `0.5` → fires when sum ≥ 1.0
- NAND: weights `[-1.0, -1.0]`, threshold `-0.5` → fires except when sum = -2.0

**Layer 2: Combine with AND**
The final layer uses an AND gate that takes the outputs from Layer 1:
- **AND gate**: weights `[1.0, 1.0]`, threshold `1.5`

**Truth Table Verification**
| A | B | OR | NAND | OR AND NAND | XOR |
|---|---|----|------|-------------|-----|
| 0 | 0 | 0  | 1    | 0           | 0   |
| 0 | 1 | 1  | 1    | 1           | 1   |
| 1 | 0 | 1  | 1    | 1           | 1   |
| 1 | 1 | 1  | 0    | 0           | 0   |

### Memory Sub-System

Using 1950s magnetic-core memory mechanics and McCulloch-Pitts (MCP) threshold logic for memory.  

In `cell_activate`, the linear combination calculation:  
$linear = (w_{row} \cdot x_{row}) + (w_{col} \cdot x_{col}) + (w_{depth} \cdot x_{depth}) + bias$  
*(mirrors the exact mathematical model of an MCP neuron)*  

With the default weights set to $1.0f$ and a bias of $-1.5f$:  
- If only **one** signal is sent (e.g., $1 + 0 + 0 - 1.5 = -0.5$),  
  it stays below `SELECTION_THRESHOLD` ($0.0f$) $\rightarrow$ **Unselected**.  
- If **two or three** signals coincide (e.g., $1 + 1 + 0 - 1.5 = +0.5$),  
  it crosses the threshold $\rightarrow$ **Selected**.  
*(coincidence detection mapped cleanly onto code)*  

**Authentic Magnetic-Core Simulation Mechanics**  
The operations replicate the physical behavior of core memory grids:

*Coincidence Addressing:*  
Just like threading an X, Y, and Z wire through a ferrite bead,  
a core only flips or reads if all specified coordinate lines carry active signals.  

*Destructive Read:*  
The `cell_read` function captures the state and immediately resets `n->state = cell_NEUTRAL;`,  
authentically replicating the physical property of magnetic core memory where reading a core clears its magnetic polarization to zero,  
requiring an immediate write-back or refresh cycle.

*The bus architecture uses **CCEL memory** with:*
- **16-bit addressing** mapped to a 256x256 2D grid
- **16 separate CCEL planes**, one per bit, giving 65,536 words of storage
- **Coincidence detection** for memory selection (requires multiple signals to activate a cell)
- **Destructive reads** that clear the cell after reading (authentic core memory behavior)

**CCEL Memory Capacity:**  
- Each cell is addressed by a 64-bit address: `(depth << 48) | (row << 32) | col`
- In practice, the bus limits this to **16-bit addressing** (65,536 words) × **16 bits** = **128 KB total**
- Each of the 16 bus data bits maps to its own CCEL plane
- The theoretical maximum with the current addressing scheme: **65,536 unique addresses**

**CCEL Memory Minimum Granularity:**  
- Single bit storage per cell (each CCEL cell stores one trinary state: -1, 0, +1)
- But for binary logic, we treat NEGATIVE as 0 and POSITIVE as 1
- Read operations are **destructive** (cell resets to NEUTRAL), requiring immediate refresh

**Memory Mapping:**  
- Program     : `0x0000-0x1FFF` (8,192 words)
- Data        : `0x2000-0x2FFF` (4,096 words)  
- Stack       : `0x3000-0x3FFF` (4,096 words)
- Free        : `0x4000-0xFFDF` (49,152 words)
- System      : `0xFFE0-0xFFF7` (24 words)

### CPU Features  
- **16-bit ALU** with Kogge-Stone parallel prefix adder
- **16 opcodes** including arithmetic, logic, memory operations, and control flow
- **Flag register**: Z (zero), C (carry), OV (overflow), L (less), G (greater)
- **Signed/unsigned comparison mode** configurable at runtime

## Known Issues / Workarounds  

1. **Performance Testing**  
   - Test loops that modify registers drift over iterations
   - Need stable operand tests for accurate measurement

2. **3D Addressing Overlap**  
   - `depth<<48 | row<<32 | col` has row/depth collision at bit 47
   - Row values above 65,535 corrupt depth addressing
   - Consider explicit masking over simple shifting

3. **Threshold Boundary Handling**  
   - Custom weights can produce exactly `0.0` sums
   - Current behavior treats this as "unselected"
   - Need explicit design decision for equality case

4. **Signal Validation**  
   - Relies on `assert` that disappears with `NDEBUG`
   - Invalid signals (e.g., `2`) would silently break the binary model
   - Should use explicit runtime validation
  
5. **64-bit edge case weirdness**  
    - The `mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1)` is a C oddity
    - Classical adders don't have to handle shift-edge cases this way
    - Has integer/neural boundary friction

6. **Carries are stored as int arrays**  
    - Classical: carry chains are bit-level signals in hardware
    - Here: int* carries arrays with values 0 or 1, computed through neuron outputs
    - All carries must be computed before sum (no hardware propagation)

7. **CCEL memory is bizarre**  
    A. **Coincidence addressing**  
    - Classical: one address line selects one location
    - CCEL: requires 2+ of 3 signals active simultaneously
    - row_signal, col_signal, depth_signal, if only one is active, nothing happens
    - This is a sparse addressing scheme: most address combos are invalid
    
    B. **Destructive reads**  
    - Classical: reading leaves memory intact
    - CCEL: n->state = CCEL_NEUTRAL after read
    - You must call ccel_refresh() immediately after every read
    - This mirrors 1950s magnetic core memory, not modern RAM

    C.**Trinary storage, binary interface**  
    - Stores three states: NEGATIVE (-1), NEUTRAL (0), POSITIVE (+1)
    - But bus operates on binary (0/1 bits)
    - For memory, POSITIVE = 1, NEGATIVE = 0, NEUTRAL = "erased" state
    - This means cells can be "partially programmed"

    D. **Neural activation for selection**  
    - Classical: address decoder selects cell
    - CCEL: the cell computes if it should be selected using MCP math
    - `linear = (w_row×row) + (w_col×col) + (w_depth×depth) + bias`
    - The cell itself decides if you're talking to it, not a decoder

    E. **Bit-sliced planes**  
    - Classical: memory word is stored contiguously
    - CCEL: `Ccel* planes[BUS_DATA_BITS] → one plane per bit`
    - To read 16-bit word, you must read 16 separate CCEL cells across 16 planes
    - Each read is destructive, requiring 16 refresh operations
    - 16× the overhead for a word operation

## Performance Scaling Consistency

| Iterations | ADD Ops/sec | MUL Ops/sec | Observed |
|------------|-------------|-------------|-------------|
| 100 | 34,795 | 13,891 | warm-up |
| 1,000 | 36,948 | 15,000 | Stabilizing |
| 5,000 | 36,335 | 15,030 | Stable |
| 10,000 | 36,656 | 15,109 | Stable |
| 50,000 | 36,546 | 15,027 | Stable |
see performance log: [perf.txt](perf.txt)

## Files

| File | Role |
|------|------|
| **gates.c** | McCulloch-Pitts neurons: AND, OR, NAND, NOR, NOT, XOR. All logic is computed via weighted sums + threshold. |
| **adder.c** | Kogge-Stone parallel prefix adder. O(log N) carry propagation. |
| **alu.c**   | ALU operations with Flag bits: Z, C, OV, L, G. |
| **ccel.c**   | Coincidence detection, CCEL memory storage. |
| **cpu.c**   | 16-bit ALU-harnass with bus accessed CCEL memory. |
| **bus.c** | Owns CCEL memory instance, memory controller. |

## Designing Neuron Gates

Each logic gate is implemented as an MCP neuron with specific weights and threshold values:  

**AND gate:**  
- Weights: [1.0, 1.0]
- Threshold: 1.5
- Outputs 1 only when both inputs are 1 (sum = 2.0)

**OR gate:**
- Weights: [1.0, 1.0]
- Threshold: 0.5
- Outputs 1 when at least one input is 1 (sum >= 1.0)

**NAND gate:**
- Weights: [-1.0, -1.0]
- Threshold: -0.5
- Outputs 1 except when both inputs are 1 (sum = -2.0)

**NOR gate:**
- Weights: [-1.0, -1.0]
- Threshold: -1.5
- Outputs 1 only when both inputs are 0 (sum = 0.0)

**NOT gate:**
- Weights: [-1.0]
- Threshold: -0.5
- Inverts the input: 1 -> 0, 0 -> 1

**XOR gate:**
- Implemented as a two-layer network: (A OR B) AND (NAND(A,B))
- First layer computes OR and NAND, second layer ANDs them together

## Designing Adder Architecture

Step 1: Compute `G = A & B, P = A XOR B`  

Step 2: Use Kogge-Stone parallel prefix:  
        `stride=1:  G[i] = G[i] | (P[i] & G[i-1])  
                    P[i] = P[i] & P[i-1]`  
        `stride=2:  G[i] = G[i] | (P[i] & G[i-2])  
                    P[i] = P[i] & P[i-2]`  
        `stride=4:  ...`  

Step 3: `Carries = [cin, G[0], G[1], ..., G[N-1]]`  

Step 4: `Sum = P_original XOR Carries[0..N-1]`  

## Designing Opcodes

| Opcode | Value | Operation | Description | Flags Set |
|--------|-------|-----------|-------------|-----------|
| `OP_ADD` | 0x0 | `A + B` | Addition | Z, C, OV |
| `OP_SUB` | 0x1 | `A - B` | Subtraction | Z, C, OV |
| `OP_AND` | 0x2 | `A & B` | Bitwise AND | Z |
| `OP_OR` | 0x3 | `A \| B` | Bitwise OR | Z |
| `OP_XOR` | 0x4 | `A ^ B` | Bitwise XOR | Z |
| `OP_MUL` | 0x5 | `A * B` | Multiplication (low `bits` bits) | Z, OV |
| `OP_CMP` | 0x6 | Compare `A` vs `B` | Sets flags only (no result) | Z, L, G |
| `OP_LDI` | 0x7 | Load Immediate | CPU operation (not ALU) | - |
| `OP_LDI16` | 0x8 | Load 16-bit Immediate | CPU operation (not ALU) | - |
| `OP_LD` | 0x9 | Load from Memory | CPU operation (not ALU) | - |
| `OP_ST` | 0xA | Store to Memory | CPU operation (not ALU) | - |
| `OP_JMP` | 0xB | Unconditional Jump | CPU operation (not ALU) | - |
| `OP_JZ` | 0xC | Jump if Zero | CPU operation (not ALU) | - |
| `OP_JNZ` | 0xD | Jump if Not Zero | CPU operation (not ALU) | - |
| `OP_SYS` | 0xE | System Call | CPU operation (not ALU) | - |
| `OP_HALT` | 0xF | Halt Execution | CPU operation (not ALU) | - |

## Opcode Parameter Signatures

| Opcode | Value | Format | RD | RS1 | RS2/IMM | Info |
|--------|-------|--------|----|-----|---------|-------|
| `OP_ADD` | 0x0 | R-R | Used | Used | Used | RD = RS1 + RS2 |
| `OP_SUB` | 0x1 | R-R | Used | Used | Used | RD = RS1 - RS2 |
| `OP_AND` | 0x2 | R-R | Used | Used | Used | RD = RS1 & RS2 |
| `OP_OR` | 0x3 | R-R | Used | Used | Used | RD = RS1 \| RS2 |
| `OP_XOR` | 0x4 | R-R | Used | Used | Used | RD = RS1 ^ RS2 |
| `OP_MUL` | 0x5 | R-R | Used | Used | Used | RD = RS1 * RS2 |
| `OP_CMP` | 0x6 | R-R | Unused | Used | Used | Sets FLAGS, no result |
| `OP_LDI` | 0x7 | R-I | Used | - | IMM8 | RD = IMM8 |
| `OP_LDI16`| 0x8 | R-I16 | Used | - | IMM16 | 2-word, RD = IMM16 |
| `OP_LD` | 0x9 | R-M | Used | - | ADDR8 | RD = MEM[ADDR8] |
| `OP_ST` | 0xA | M-R | Unused | Used | ADDR8 | MEM[ADDR8] = RS1 |
| `OP_JMP` | 0xB | J | - | - | ADDR8 | PC = ADDR8 |
| `OP_JZ` | 0xC | J-C | - | - | ADDR8 | if Z: PC = ADDR8 |
| `OP_JNZ` | 0xD | J-C | - | - | ADDR8 | if !Z: PC = ADDR8 |
| `OP_SYS` | 0xE | SYS | Used | Used | Used | System call |
| `OP_HALT` | 0xF | SYS | - | - | - | Stop execution |

**Flags**

| Flag | Abbr | Description | Set When |
|------|------|-------------|----------|
| **Z** | Zero | Result is zero | All ops except CMP set Z based on result |
| **C** | Carry | Carry/Borrow out | ADD (carry), SUB (borrow) |
| **OV** | Overflow | Signed overflow | ADD, SUB, MUL (product doesn't fit) |
| **L** | Less | Comparison less-than | CMP only (signed/unsigned mode) |
| **G** | Greater | Comparison greater-than | CMP only (signed/unsigned mode) |

**ADD**
- `Z` = (result == 0)
- `C` = carry-out from most significant bit
- `OV` = `carries[bits] ^ carries[bits-1]` (signed overflow detection)

**SUB**
- `Z` = (result == 0)
- `C` = borrow (inverse of carry-out)
- `OV` = `carries[bits] ^ carries[bits-1]`

**MUL**
- `Z` = (result == 0)
- `OV` = product > `(1<<bits) - 1` (doesn't fit in width)

**CMP**
- `Z` = (A == B)
- `L` = (A < B) (signed or unsigned depending on mode)
- `G` = (A > B) (signed or unsigned depending on mode)
- **Result register = 0**

**AND, OR, XOR**
- `Z` = (result == 0)
- All other flags = 0

**Signed vs Unsigned Comparison**

Default: **signed** (`cmp_signed = 1`)

- **Signed mode**: L uses `sign(A-B) XOR overflow(A-B)` (standard SLT idiom)
- **Unsigned mode**: L uses borrow from subtraction

Set via `alu_set_cmp_mode(alu, is_signed)`:
```c
alu_set_cmp_mode(&alu, 1);  // signed (default)
alu_set_cmp_mode(&alu, 0);  // unsigned
```

**CPU Operations (Not ALU)***

The following opcodes are defined for the CPU but don't execute in the ALU:
- `OP_LDI`, `OP_LDI16` - Immediate loads
- `OP_LD`, `OP_ST` - Memory access
- `OP_JMP`, `OP_JZ`, `OP_JNZ` - Control flow
- `OP_SYS` - System call
- `OP_HALT` - Stop execution

These will be handled by the CPU control logic when `cpu.c` is implemented.

# Testing Sample codes

Sample program:
```asm
   // Test program
   uint16_t test_program[] = {
       // Load test values
       0x7105,  // LDI R1, 5
       0x7203,  // LDI R2, 3
       0x7307,  // LDI R3, 7
       // ADD: R0 = R1 + R2 = 5 + 3 = 8
       0x0012,  // ADD R0, R1, R2
       // SUB: R0 = R0 - R2 = 8 - 3 = 5
       0x1102,  // SUB R0, R0, R2
       // AND: R0 = R0 & R3 = 5 & 7 = 5
       0x2103,  // AND R0, R0, R3
       // OR:  R0 = R0 | R2 = 5 | 3 = 7
       0x3102,  // OR  R0, R0, R2
       // XOR: R0 = R0 ^ R3 = 7 ^ 7 = 0
       0x4103,  // XOR R0, R0, R3
       // MUL: R0 = R1 * R2 = 5 * 3 = 15
       0x5012,  // MUL R0, R1, R2
       // CMP: Compare R0 (15) vs R3 (7)
       0x6103,  // CMP R0, R3   (flags: Z=0, L=0, G=1)
       // HALT
       0xF000   // HALT
   };
```

Sample debug data:
```text
   Executing...
   
   Final state:
   Registers:
     R0 : 0x002D (   45)
     R1 : 0x000F (   15)
     R2 : 0x0003 (    3)
     R3 : 0x0007 (    7)
     R4 : 0x0000 (    0)
     R5 : 0x0000 (    0)
     R6 : 0x0000 (    0)
     R7 : 0x0000 (    0)
     R8 : 0x0000 (    0)
     R9 : 0x0000 (    0)
     R10: 0x0000 (    0)
     R11: 0x0000 (    0)
     R12: 0x0000 (    0)
     R13: 0x0000 (    0)
     R14: 0x0000 (    0)
     R15: 0x0000 (    0)
     PC:  0x000B 
     SP:  0x4000 
     IR:  0xF000
   Flags: Z=0 C=0 OV=0 L=0 G=1 
   Cycles: 22
   State: HALTED
   
   Memory Dump (at PC offset):
   Bus Dump: addr=0x0007, words=8
     Addr  |  Data (hex) | Data (bin)
     ------+-------------+-----------
     0x0007 | 0x4103     | 0100 0001 0000 0011
     0x0008 | 0x5012     | 0101 0000 0001 0010
     0x0009 | 0x6103     | 0110 0001 0000 0011
     0x000A | 0xF000     | 1111 0000 0000 0000
     0x000B | 0x0000     | 0000 0000 0000 0000
     0x000C | 0x0000     | 0000 0000 0000 0000
     0x000D | 0x0000     | 0000 0000 0000 0000
     0x000E | 0x0000     | 0000 0000 0000 0000
```
