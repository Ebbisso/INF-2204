#!/bin/bash
set -e

# Compile C simulator[cite: 4]
gcc -O3 cache.c -o cache

# Generate synthetic traces
python3 traces.py

mkdir -p results

CAPACITY=32768   # 32 KB Cache[cite: 5]
ASSOC=8          # 8-way associative[cite: 5]
WINDOW=100       # Sliding window size Delta_t[cite: 5, 6]

echo "=== Running Experiment 1: Baseline Adaptability (W1 -> W2) ==="[cite: 5]
for pol in LRU FIFO LFU; do
    ./cache_sim traces/exp1_w1_w2.trace $CAPACITY $ASSOC $pol $WINDOW "results/exp1_${pol}.csv"
done

echo "=== Running Experiment 2: Scan Thrashing Resistance (W -> S -> W) ==="[cite: 5, 6]
./cache traces/exp2_scan_2x.trace $CAPACITY $ASSOC LRU $WINDOW "results/exp2_LRU.csv"
./cache traces/exp2_scan_2x.trace $CAPACITY $ASSOC LFU $WINDOW "results/exp2_LFU.csv"

echo "=== Running Experiment 3: Phase Transition Frequency (W1 <-> W2) ==="[cite: 6]
./cache traces/exp3_toggle_1000.trace $CAPACITY $ASSOC LRU $WINDOW "results/exp3_LRU.csv"
./cache traces/exp3_toggle_1000.trace $CAPACITY $ASSOC LFU $WINDOW "results/exp3_LFU.csv"

echo "All experimental runs complete! Output saved in results/"