# Cache Policy Adaptability Under Dynamic Workloads

A trace-driven cache simulator in C and automated Python evaluation pipeline designed to measure transient recovery latency, scan thrashing resistance, and phase-shift adaptability across classical (`LRU`, `FIFO`, `LFU`) and adaptive (`ARC`) replacement policies[cite: 39, 40, 41, 43, 44].

## Prerequisites
* **C Compiler:** `gcc` or `clang`[cite: 4, 43]
* **Python 3** with `pandas`, `matplotlib`, and `seaborn`[cite: 4, 41]:
  ```bash
  pip3 install pandas matplotlib seaborn

## How to run

Make executable:
```chmod +x run.sh```

Compile and generate traces:
```./run.sh```

Generate figures:
```python3 plot.py````
