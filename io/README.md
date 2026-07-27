## Memory Sub-System

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
authentically replicating the physical property of magnetic core memory where reading a core  
clears its magnetic polarization to zero, requiring an immediate write-back or refresh cycle.  

## CCEL (Coincidence-Cell Memory)

### Core Design
- **MCP Neuron Foundation**: Each memory cell is a McCulloch-Pitts neuron
- **Coincidence Detection**: Cell activates only when 2+ signals coincide
- **Destructive Reads**: Authentic 1950s magnetic core memory behavior
- **Trinary States**: NEGATIVE (-1), NEUTRAL (0), POSITIVE (+1)

### Safe API Design
- **Type-safe signals**: All signal parameters use `bool` for automatic sanitization
- **Symmetrical threshold**: Uses `>=` for mathematically correct MCP behavior
- **Collision-free addressing**: Union-based 3D addressing with no bit overlap
- **Defensive null checks**: Graceful handling of invalid pointers

### 3D Addressing
```
┌─────────────────────────────────────────────┐
│              CcelAddress (64-bit)           │
├─────────────────────────────────────────────┤
│  col (32 bits) | row (16 bits) | depth (16) │
│  Bits 0-31     | Bits 32-47    | Bits 48-63 │
└─────────────────────────────────────────────┘
```

### Known Design Tradeoffs
- **Bit-sliced storage**: 16 planes = 16× overhead per word operation
- **Trinary states**: Extra state for erasure, potential ternary future
- **Neural selection**: Compute cost per cell, no traditional decoder

*The bus architecture uses **CCEL memory** with:*
- **16-bit addressing** mapped to a 256x256 2D grid
- **16 separate CCEL planes**, one per bit, giving `65,536` words of storage
- **Coincidence detection** for memory selection (requires multiple signals to activate a cell)
- **Destructive reads** that clear the cell after reading (authentic core memory behavior)

**CCEL Memory Capacity:**  
- Each cell uses a **collision-free union address** with 32-bit col, 16-bit row, 16-bit depth
- In practice, the bus limits this to **16-bit addressing** (65,536 words) × **16 bits** = **128 KB total**
- Each of the 16 bus data bits maps to its own CCEL plane
- The theoretical maximum with the current addressing scheme: **65,536 unique addresses**

**CCEL Memory Minimum Granularity:**  
- Single bit storage per cell (each CCEL cell stores one trinary state: -1, 0, +1)
- But for binary logic, we treat NEGATIVE as 0 and POSITIVE as 1
- Read operations are **destructive** (cell resets to NEUTRAL), requiring immediate refresh

---

## Known Issues / Workarounds

### **1. Performance Testing** *(Open)*
- Test loops that modify registers drift over iterations
- Need stable operand tests for accurate measurement

---

### **2. 3D Addressing Overlap** *(RESOLVED - CCEL union addressing)*
- `depth<<48 | row<<32 | col` had row/depth collision at bit 47
- Row values above 65,535 corrupt depth addressing

*Replaced bit-shifting with a `CcelAddress` union that cleanly separates col (32-bit), row (16-bit), and depth (16-bit).
No bit-shifting math means zero risk of overflow or bit 47 collisions.*

**Example:** 
```c
typedef union {
    uint64_t raw;
    struct {
        uint32_t col;   // Bits 0-31
        uint16_t row;   // Bits 32-47
        uint16_t depth; // Bits 48-63
    };
} CcelAddress;
```

---

### **3. Threshold Boundary Handling** *(RESOLVED - Symmetrical MCP logic)*
- Custom weights could produce exactly `0.0` sums
- Treated as "unselected" with `>` operator
- **Fix:** Changed `linear > SELECTION_THRESHOLD` to `linear >= SELECTION_THRESHOLD`.
  This is the mathematically correct behavior for McCulloch-Pitts neurons and elegantly resolves the `0.0` boundary edge case.

*(The neuron now fires when `linear >= 0.0` instead of `linear > 0.0`, making the decision boundary closed and symmetrical.)*

---

### **4. Signal Validation** *(RESOLVED - Type-safe bool API)*
- Relied on `assert` that disappears with `NDEBUG`
- Invalid signals (e.g., `2`) would silently break the binary model
- **Fix:** Changed all signal parameters to `bool`. The C compiler automatically truncates any rogue integer input to strictly `0` or `1`.
  This completely neutralizes the "Invalid signals silently break the model" issue without adding a single runtime `if` statement or macro check.

```c
// Before
CcelSelection ccel_activate(const Ccel* n, int row_signal, int col_signal, int depth_signal);

// After  
CcelSelection ccel_activate(const Ccel* n, bool row_signal, bool col_signal, bool depth_signal);
```
*(Any integer passed to a `bool` parameter is strictly converted to 0 or 1 at compile-time.)*

---

### **5. 64-bit edge case weirdness**
- `mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1)` was a C oddity
- Shifting by 64 bits is undefined behavior in C
- **Fix:** Added a guarded mask function:
```c
static inline uint64_t ccel_get_mask(int bits) {
    if (bits >= 64) return ~0ULL;
    if (bits <= 0)  return 0ULL;
    return (1ULL << bits) - 1;
}
```

*(The function now explicitly handles the edge cases before attempting any shift, eliminating UB.)*

---

### **6. Carries are stored as int arrays**
- Classical: carry chains are bit-level signals in hardware
- Here: `int*` carries arrays with values 0 or 1, computed through neuron outputs
- All carries must be computed before sum (no hardware propagation)
- **Status:** This is now a documented design decision rather than an issue. The `int` arrays are simple, portable,
  and allow the Kogge-Stone adder to work correctly with the MCP neuron model.  

**Alternative considered:**  
Bit-packing or using MCP neurons directly for carry generation, but the current approach balances clarity with performance.  

---

## CCEL Memory: Design Notes

### 7. **CCEL memory is bizarre**

**A. Coincidence addressing**
- Classical: one address line selects one location
- CCEL: requires 2+ of 3 signals active simultaneously
- row_signal, col_signal, depth_signal, if only one is active, nothing happens
- This is a sparse addressing scheme: most address combos are invalid
- **Status:** This is intentional MCP neuron behavior, implementing coincidence detection per the magnetic core memory model

**B. Destructive reads**
- Classical: reading leaves memory intact
- CCEL: `n->state = CCEL_NEUTRAL` after read
- You must call `ccel_refresh()` immediately after every read
- This mirrors 1950s magnetic core memory, not modern RAM
- **Status:** Authentic core memory simulation - documented as intended behavior

**C. Trinary storage, binary interface**
- Stores three states: NEGATIVE (-1), NEUTRAL (0), POSITIVE (+1)
- But bus operates on binary (0/1 bits)
- For memory, POSITIVE = 1, NEGATIVE = 0, NEUTRAL = "erased" state
- This means cells can be "partially programmed"
- **Status:** Documented behavior. The trinary state allows for erasure (NEUTRAL) and has potential for future ternary logic experiments.

**D. Neural activation for selection**
- Classical: address decoder selects cell
- CCEL: the cell computes if it should be selected using MCP math
- `linear = (w_row×row) + (w_col×col) + (w_depth×depth) + bias`
- The cell itself decides if you're talking to it, not a decoder
- **Status:** This is the fundamental innovation of the CCEL memory - neuron-based selection

**E. Bit-sliced planes**
- Classical: memory word is stored contiguously
- CCEL: `Ccel* planes[BUS_DATA_BITS] → one plane per bit`
- To read 16-bit word, you must read 16 separate CCEL cells across 16 planes
- Each read is destructive, requiring 16 refresh operations
- 16× the overhead for a word operation
- **Status:** Known performance characteristic. This maps directly to magnetic core memory where each bit is a separate core.
  Future optimizations could include word-addressed CCEL cells.

---
