# Axiom

version: 1.0  
A Threshold Logic Unit (TLU) runtime, A C-runtime drop-in building block for a non-Von Neumann cpu model using MCP neurons.  

## Files

| File | Role |
|------|------|
| **gates.c** | McCulloch-Pitts neurons: AND, OR, NAND, NOR, NOT, XOR. All logic is computed via weighted sums + threshold. |
| **adder.c** | Kogge-Stone parallel prefix adder. O(log N) carry propagation. |
| **alu.c**   | ALU operations with Flag bits: Z, C, OV, L, G. |
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
