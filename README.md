# Imaginary List Search

`Imaginary List Search` is a small C++ experiment that searches for a **deterministic "imaginary list"** (a 32-bit identifier) that contains as many values as possible from a given data block.

An **imaginary list** is not stored anywhere. It is defined mathematically:

*   **Universe:** All 16-bit values $U = \{0..65535\}$.
*   **Size:** Each list is a **perfect half** of the universe (exactly 32,768 elements).
*   **Membership:** Computed on-the-fly via a lightweight 16-bit Pseudo-Random Permutation (PRP) and a Most Significant Bit (MSB) test.

> [!IMPORTANT]
> This is **not cryptography**. The PRP is used for diffusion and uniform splitting, not for security purposes.

---

## Why "Imaginary Lists"?

If you can define an enormous family of half-universe sets **without storing them**, you can:
*   Compute membership extremely fast.
*   Search for a list that matches a local data block.
*   Represent the block (approximately) by a small identifier.

This repository contains the *core list model* and a simple heuristic search procedure.

---

## List Model

### PRP16 (4-round Feistel, 8+8)

A 16-bit permutation `prp16(x, listID)` is built using a 4-round Feistel network. Because the function is bijective, checking the most significant bit (MSB) splits the universe perfectly in half:

```cpp
hasValue(listID, x) := (prp16(x, listID) & 0x8000) == 0
```

**Key Property:** For any fixed `listID`, exactly 32,768 inputs map to `MSB=0` and 32,768 map to `MSB=1`. Thus, every list is perfectly balanced by construction.

### Polarity / Inversion Trick

During scoring, the code uses:
```cpp
score = max(inCount, N - inCount)
```
If a candidate list places "most" values on the `MSB=1` side, the inverted polarity (taking the complement half) is treated as the better match. This keeps the search symmetric and avoids wasting effort on polarity.

---

## Search Strategy

The solver uses a simple and fast heuristic:

1.  **Random Restarts:** Sample many random 32-bit `listID` candidates.
2.  **Greedy Hill-Climb:** For each candidate, repeatedly try flipping each of the 32 bits (`listID ^ (1<<b)`), accepting any improvement.
3.  **Selection:** Keep the best `listID` found.

The search is intentionally simple:
*   No global optimum guarantees.
*   Easy to tune.

---

## Usage

### Input Format

The program reads an input file as a little-endian `uint16_t` stream:
*   Bytes `[2*i], [2*i+1]` $\rightarrow$ one `uint16_t` value.
*   Trailing odd bytes (if any) are ignored.

It evaluates consecutive non-overlapping blocks:
*   **Block 0:** `[0 .. blockLen-1]`
*   **Block 1:** `[blockLen .. 2*blockLen-1]`

### Build

Tested with a standard C++17 toolchain.

```bash
g++ -O3 -march=native -std=c++17 -o imaginary_list_search main.cpp
```

> [!TIP]
> `-O3 -march=native` is strongly recommended because the program is dominated by tight loops (membership checks and scoring).

### Execution

```bash
./imaginary_list_search <file> [blockLen=64] [blocksToTest=50]
```

**Examples:**
```bash
./imaginary_list_search sample.bin
./imaginary_list_search sample.bin 64 1000
./imaginary_list_search sample.bin 128 200
```

### Output Format

The output provides the following information:
*   Total `uint16_t` count in the file.
*   Best score per block.
*   Global average and the best `listID` ever found.

**Example Output:**
```text
u16 count: 2276310
blockLen: 64, blocksToTest: 50
search: restarts=250 hillIters=8

Block 0 bestScore=61/64  listID=0xd2c9d229
...
Average bestScore: 59.52/64 (93.00%)
Best ever: 64/64  listID=0x789454a0
```

---

## Configuration Knobs

In `main.cpp`, you can adjust the following parameters:

*   `cfg.restarts = 250;` — Number of random initial keys.
*   `cfg.hillIters = 8;` — Number of greedy passes.

**Notes on Performance:**
*   Increasing `restarts` improves results but increases runtime linearly.
*   Increasing `hillIters` helps if local improvements exist.
*   For `blockLen=128`, runtime increases roughly proportional to `blockLen` because each score evaluation checks all elements.

---

## What "Success" Means Here

This tool measures **coverage**:
*   `bestScore`: How many block values are classified on the best side (`MSB=0` or `MSB=1`).
*   `bestScore / blockLen`: The coverage ratio.

Because each list is a 50/50 split, a naive random `listID` typically yields ~50% coverage. The search procedure attempts to raise this significantly using the `listID` degrees of freedom.

---

## Limitations & Notes

*   **Not Cryptographic:** The PRP is designed for diffusion and balanced splitting, not security.
*   **Heuristic Nature:** The method may converge to local optima.
*   **Data Entropy:** High-entropy data behaves close to random; perfect coverage on large blocks is not generally expected.

---

## Motivation / Call for Ideas

This repository is a minimal baseline for "imaginary half-universe list" membership and search heuristics. If you have ideas to improve the project, please open an issue or PR. Areas of interest include:

*   **Faster Search:** Bit-parallel scoring, incremental scoring, or SIMD.
*   **Better Optimizers:** Beam search, simulated annealing, or CMA-ES.
*   **Richer List Families:** More parameters while maintaining minimal metadata.
*   **Theoretical Bounds.**

---

## License

This project is licensed under the **Apache License Version 2.0**.

---
*This tool is designed to explore the "imaginary list" concept quickly and efficiently.*
