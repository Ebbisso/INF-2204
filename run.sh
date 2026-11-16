#!/bin/bash
set -e

gcc -O3 cache.c -o cache
python3 traces.py

mkdir -p results figures

#CAPACITY=32768   # 32 KB Cache
CAPACITY=16384   # 16 KB Cache (matches single working set size)
ASSOC=8          # 8-way associative
WINDOW=100       # Sliding window size Delta_t

echo "=== Running Experiment 1: Baseline Adaptability (W1 -> W2) ==="
for pol in LRU FIFO LFU ARC; do
    ./cache traces/exp1_w1_w2.trace $CAPACITY $ASSOC $pol $WINDOW "results/exp1_${pol}.csv"
done

echo "=== Running Experiment 2: Scan Thrashing Resistance (W -> S -> W) ==="
for pol in LRU FIFO LFU ARC; do
    ./cache traces/exp2_scan_2x.trace $CAPACITY $ASSOC $pol $WINDOW "results/exp2_${pol}.csv"
done

echo "=== Running Experiment 3: Phase Transition Frequency (W1 <-> W2) ==="
for pol in LRU FIFO LFU ARC; do
    ./cache traces/exp3_toggle_1000.trace $CAPACITY $ASSOC $pol $WINDOW "results/exp3_${pol}.csv"
done

echo "All experimental runs complete! Output saved in results/"