import random
import os

# Address space boundaries (32-bit aligned addresses)
BLOCK_SIZE = 64  # 64 bytes per cache block

def generate_working_set(num_accesses, working_set_size_bytes, start_addr=0x10000, seed=42):
    """Generates random repeated accesses within a small bounded working set (High Temporal Locality)[cite: 5]."""
    random.seed(seed)
    num_blocks = max(1, working_set_size_bytes // BLOCK_SIZE)
    blocks = [start_addr + (i * BLOCK_SIZE) for i in range(num_blocks)]
    return [f"D 0x{random.choice(blocks):08x} 4" for _ in range(num_accesses)]

def generate_scanning(num_accesses, scan_size_bytes, start_addr=0x800000):
    """Generates sequential streaming accesses across a large contiguous region (Zero Temporal Locality)[cite: 5]."""
    num_blocks = max(1, scan_size_bytes // BLOCK_SIZE)
    traces = []
    for i in range(num_accesses):
        addr = start_addr + ((i % num_blocks) * BLOCK_SIZE)
        traces.append(f"D 0x{addr:08x} 4")
    return traces

def make_experiment_1_trace(filename, cache_capacity_bytes=32768, num_accesses=20000):
    """Protocol 1: Abrupt transition between non-overlapping working sets (W1 -> W2)[cite: 5]."""
    ws_size = cache_capacity_bytes // 2  # Fits easily in cache
    w1 = generate_working_set(num_accesses // 2, ws_size, start_addr=0x100000, seed=101)
    w2 = generate_working_set(num_accesses // 2, ws_size, start_addr=0x500000, seed=202)
    
    with open(filename, 'w') as f:
        f.write("\n".join(w1 + w2) + "\n")
    print(f"Generated Protocol 1 trace: {filename}")

def make_experiment_2_trace(filename, scan_multiplier=2.0, cache_capacity_bytes=32768):
    """Protocol 2: Scan Thrashing (W -> S -> W)[cite: 5, 6]."""
    ws_size = cache_capacity_bytes // 2
    scan_size = int(cache_capacity_bytes * scan_multiplier)
    
    w_before = generate_working_set(10000, ws_size, start_addr=0x100000, seed=303)
    scan_phase = generate_scanning(int(scan_size // BLOCK_SIZE), scan_size, start_addr=0x900000)
    w_after = generate_working_set(10000, ws_size, start_addr=0x100000, seed=303) # Return to same W
    
    with open(filename, 'w') as f:
        f.write("\n".join(w_before + scan_phase + w_after) + "\n")
    print(f"Generated Protocol 2 trace ({scan_multiplier}x cache size): {filename}")

def make_experiment_3_trace(filename, phase_length=1000, total_phases=20, cache_capacity_bytes=32768):
    """Protocol 3: High-Frequency Phase Toggling (W1 <-> W2)[cite: 6]."""
    ws_size = cache_capacity_bytes // 2
    traces = []
    for p in range(total_phases):
        addr = 0x100000 if (p % 2 == 0) else 0x600000
        seed = 400 + (p % 2)
        traces.extend(generate_working_set(phase_length, ws_size, start_addr=addr, seed=seed))
        
    with open(filename, 'w') as f:
        f.write("\n".join(traces) + "\n")
    print(f"Generated Protocol 3 trace (phase_len={phase_length}): {filename}")

if __name__ == "__main__":
    os.makedirs("traces", exist_ok=True)
    make_experiment_1_trace("traces/exp1_w1_w2.trace")
    make_experiment_2_trace("traces/exp2_scan_2x.trace", scan_multiplier=2.0)
    make_experiment_3_trace("traces/exp3_toggle_1000.trace", phase_length=1000)