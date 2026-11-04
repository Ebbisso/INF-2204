import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os

plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
plt.rcParams.update({
    'font.family': 'sans-serif',
    'font.size': 11,
    'axes.labelsize': 12,
    'axes.titlesize': 13,
    'xtick.labelsize': 10,
    'ytick.labelsize': 10,
    'legend.fontsize': 10,
    'figure.titlesize': 14
})

os.makedirs("figures", exist_ok=True)

policies = ['LRU', 'FIFO', 'LFU', 'ARC']
colors = {'LRU': '#1f77b4', 'FIFO': '#ff7f0e', 'LFU': '#2ca02c', 'ARC': '#d62728'}
styles = {'LRU': '-', 'FIFO': '--', 'LFU': '-.', 'ARC': '-'}

def plot_experiment_1():
    fig, ax = plt.subplots(figsize=(8, 4.5), dpi=300)
    for pol in policies:
        path = f"results/exp1_{pol}.csv"
        if os.path.exists(path):
            df = pd.read_csv(path)
            ax.plot(df['AccessCount'], df['WMR'], label=pol, color=colors[pol], linestyle=styles[pol], linewidth=1.8)
            
    ax.axvline(x=10000, color='gray', linestyle=':', alpha=0.9, label='Phase Shift (W1 -> W2)')
    ax.set_title("Experiment 1: Transient Windowed Miss Rate (WMR) During W1 -> W2 Transition")
    ax.set_xlabel("Memory Access Count")
    ax.set_ylabel("Windowed Miss Rate (WMR)")
    ax.set_ylim(-0.02, 1.05)
    ax.legend(loc='upper right', frameon=True)
    
    plt.tight_layout()
    fig.savefig("figures/fig1_exp1_adaptability.png")
    plt.close()

def plot_experiment_2():
    fig, ax = plt.subplots(figsize=(8, 4.5), dpi=300)
    for pol in policies:
        path = f"results/exp2_{pol}.csv"
        if os.path.exists(path):
            df = pd.read_csv(path)
            ax.plot(df['AccessCount'], df['WMR'], label=pol, color=colors[pol], linestyle=styles[pol], linewidth=1.8)
            
    ax.axvspan(10000, 11024, color='orange', alpha=0.2, label='Scanning Phase (S)')
    ax.set_title("Experiment 2: Scan Thrashing Resistance (W -> S -> W)")
    ax.set_xlabel("Memory Access Count")
    ax.set_ylabel("Windowed Miss Rate (WMR)")
    ax.set_ylim(-0.02, 1.05)
    ax.legend(loc='upper right', frameon=True)
    
    plt.tight_layout()
    fig.savefig("figures/fig2_exp2_scan_thrashing.png")
    plt.close()

def plot_experiment_3():
    fig, ax = plt.subplots(figsize=(8, 4.5), dpi=300)
    for pol in policies:
        path = f"results/exp3_{pol}.csv"
        if os.path.exists(path):
            df = pd.read_csv(path)
            ax.plot(df['AccessCount'], df['WMR'], label=pol, color=colors[pol], linestyle=styles[pol], linewidth=1.5, alpha=0.85)
            
    ax.set_title("Experiment 3: WMR under High-Frequency Toggling (W1 <-> W2)")
    ax.set_xlabel("Memory Access Count")
    ax.set_ylabel("Windowed Miss Rate (WMR)")
    ax.set_ylim(-0.02, 1.05)
    ax.legend(loc='upper right', frameon=True)
    
    plt.tight_layout()
    fig.savefig("figures/fig3_exp3_high_freq.png")
    plt.close()

if __name__ == "__main__":
    plot_experiment_1()
    plot_experiment_2()
    plot_experiment_3()
    print("All plots generated successfully!")