# Axiom

version: 1.0  
A Threshold Logic Unit C-runtime drop-in building block for a non-Von Neumann cpu model using MCP neurons.  

## Downsides learned:

**Performance Testing**  
Any test that modifies its source registers will drift over multiple iterations.   
We need a method that keeps operands stable while still measuring real execution speed.  

## Files

| File | Role |
|------|------|
| **gates.c** | McCulloch-Pitts neurons: AND, OR, NAND, NOR, NOT, XOR. All logic is computed via weighted sums + threshold. |
| **adder.c** | Kogge-Stone parallel prefix adder. O(log N) carry propagation. |
| **alu.c**   | ALU operations with Flag bits: Z, C, OV, L, G. |
| **ccel.c**   | Coincidence detection, CCEL memory storage. |
| **cpu.c**   | (TODO) 16-bit ALU-harnass with bus accessed CCEL memory. |

## McCulloch–Pitts (MCP) neurons

The artificial neuron was first proposed by Warren McCulloch and Walter Pitts in their 1943 paper,  
the `Threshold Logic Unit` (TLU) the simplest type of artificial neuron and binary classifier,  
functioning by multiplying inputs by weights, summing them, and outputting a `1`,  
if the sum meets or exceeds a set threshold.   

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
| `OP_SYS` | 0xE | SYS | - | - | - | System call |
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

## CCEL Memory

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

### Authentic Magnetic-Core Simulation Mechanics

The operations replicate the physical behavior of core memory grids:

**Coincidence Addressing:**  
Just like threading an X, Y, and Z wire through a ferrite bead,  
a core only flips or reads if all specified coordinate lines carry active signals.  

**Destructive Read:**  
The `cell_read` function captures the state and immediately resets `n->state = cell_NEUTRAL;`,  
authentically replicating the physical property of magnetic core memory where reading a core clears its magnetic polarization to zero,  
requiring an immediate write-back or refresh cycle.
