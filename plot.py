import json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

with open('results.json') as f:
    data = json.load(f)

# Group benchmarks by category
categories = {
    'insert': {'BM_Insert_Sequential': 'Sequential', 'BM_Insert_Random': 'Random'},
    'contain': {'BM_Contain_Sequential': 'Sequential', 'BM_Contain_Random': 'Random'},
    'remove': {'BM_Remove_Sequential': 'Sequential', 'BM_Remove_Random': 'Random'},
    'mixed': {
        'BM_Mixed_Equal': 'Equal (30/40/30)',
        'BM_Mixed_ReadHeavy': 'Read-Heavy (20/70/10)',
        'BM_Mixed_WriteHeavy': 'Write-Heavy (80/20/0)'
    }
}

# Parse points
points = {cat: {lbl: [] for lbl in categories[cat].values()} for cat in categories}

for bm in data.get('benchmarks', []):
    name = bm['name']
    real_time = bm['real_time']
    if '/threads:' not in name:
        continue
    base_name, thread_info = name.split('/threads:')
    threads = int(thread_info)
    
    # Calculate aggregate throughput: threads * (1e6 / real_time_ns)
    throughput = (threads * 1000000.0) / real_time
    
    for cat, mappings in categories.items():
        if base_name in mappings:
            label = mappings[base_name]
            points[cat][label].append((threads, throughput))

# Plot category graphs using matplotlib (simple and default style)
for cat, series in points.items():
    plt.figure(figsize=(7, 4.5))
    for label, pts in series.items():
        pts.sort()
        x = [p[0] for p in pts]
        y = [p[1] for p in pts]
        plt.plot(x, y, marker='o', label=label)
    plt.title(f"{cat.capitalize()} Performance vs Threads")
    plt.xlabel('Threads')
    plt.ylabel('Throughput (ops/ms)')
    plt.legend()
    plt.grid(True, linestyle='--')
    plt.tight_layout()
    plt.savefig(f"{cat}_performance.png")
    plt.close()
    print(f"Generated {cat}_performance.png")
