# Axiom
Combining neural networks, vintage memory, and modern parallel prefix adders all in one coherent system.  

### The MCP Neuron Foundation

The artificial neuron was first proposed by Warren McCulloch and Walter Pitts in their 1943 paper,  
the `Threshold Logic Unit` (TLU) the simplest type of artificial neuron and binary classifier,  
functioning by multiplying inputs by weights, summing them, and outputting a `1`,  
if the sum meets or exceeds a set threshold.   

In the Axiom framework, every logic gate in the system is built from MCP neurons:
- **Inputs** are multiplied by weights and summed with a bias
- **Output** is 1 if the sum meets or exceeds a threshold, otherwise 0
- Logic gates (AND, OR, NAND, NOR, NOT, XOR) are constructed using specific weight/threshold combinations

### CPU Features  
- **16-bit ALU** with Kogge-Stone parallel prefix adder
- **16 opcodes** including arithmetic, logic, memory operations, and control flow
- **Flag register**: Z (zero), C (carry), OV (overflow), L (less), G (greater)
- **Signed/unsigned comparison mode** configurable at runtime

**Memory Mapping:**  
- Program     : `0x0000-0x1FFF` (8,192 words)
- Data        : `0x2000-0x2FFF` (4,096 words)  
- Stack       : `0x3000-0x3FFF` (4,096 words)
- Free        : `0x4000-0xFFDF` (49,152 words)
- System      : `0xFFE0-0xFFF7` (24 words)

## Files

| File | Role |
|------|------|
| **gates.c** | McCulloch-Pitts neurons: AND, OR, NAND, NOR, NOT, XOR. All logic is computed via weighted sums + threshold. |
| **adder.c** | Kogge-Stone parallel prefix adder. O(log N) carry propagation. |
| **alu.c**   | ALU operations with Flag bits: Z, C, OV, L, G. |
| **ccel.c**   | Coincidence detection, CCEL memory storage. More: [readme](io/README.md) |
| **cpu.c**   | 16-bit ALU-harnass with bus accessed CCEL memory. |
| **bus.c** | Owns CCEL memory instance, memory controller. |

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

## Designing Primitive Neurons (Gate module)

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

## Designing a Fast Adder

Performance Scaling Consistency:
| Iterations | ADD Ops/sec | MUL Ops/sec | Observed |
|------------|-------------|-------------|-------------|
| 100 | 34,795 | 13,891 | warm-up |
| 1,000 | 36,948 | 15,000 | Stabilizing |
| 5,000 | 36,335 | 15,030 | Stable |
| 10,000 | 36,656 | 15,109 | Stable |
| 50,000 | 36,546 | 15,027 | Stable |
see performance log: [perf.txt](perf.txt)

Step 1: Compute `G = A & B, P = A XOR B`  

Step 2: Use Kogge-Stone parallel prefix:  
        `stride=1:  G[i] = G[i] | (P[i] & G[i-1])  
                    P[i] = P[i] & P[i-1]`  
        `stride=2:  G[i] = G[i] | (P[i] & G[i-2])  
                    P[i] = P[i] & P[i-2]`  
        `stride=4:  ...`  

Step 3: `Carries = [cin, G[0], G[1], ..., G[N-1]]`  

Step 4: `Sum = P_original XOR Carries[0..N-1]`  
