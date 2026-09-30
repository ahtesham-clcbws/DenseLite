import os
import glob

files = glob.glob('benchmarks/*.md') + glob.glob('.agents/benchmarks/*.md') + ['README.md', '.agents/process_logs.md']

replacements = [
    # Issue 1: RAM Ceiling (14,117 -> 15.6 GiB or 50% rule)
    ('14,117 MiB Max', '~15.6 GiB (50% host RAM)'),
    ('14,117 MB RAM (Hard ceiling)', '~15.6 GB RAM (50% host)'),
    ('< 14,117 MB | ✅ Well within 14 GB ceiling', '< 15,974 MB | ✅ Well within ~15.6 GB ceiling'),
    ('Configured RAM Ceiling: 14,117 MiB (~14.1 GB max process allocation)', 'Configured RAM Ceiling: ~15.6 GiB (50% host RAM allocation)'),
    
    # Issue 2: 50% max host load -> 50% hardware threads for compute
    ('50% max host load', '50% hardware threads for compute'),
    ('50% max CPU on 4-thread host', '50% hardware threads for compute'),
    ('50% max CPU load on 4-thread host', '50% hardware threads for compute'),

    # Issue 5: README hardware requirements (Already largely fixed, but just in case for .agents)
    ('4 CPU cores minimum (AVX2 + FMA required)', '2 cores / 4 threads minimum (AVX2 + FMA required)'),
    
    # Issue 7: Completion Gate Anti-Hallucination
    ('Evidence-Based Anti-Hallucination', 'Policy Evaluation Throughput'),
    
    # Issue 8: Multimodal PASS -> Synthetic
    ('🟢 PASS (23,970x Real-time)', '⚠️ SYNTHETIC / PROTOTYPE'),
    ('🟢 PASS (29,444x Real-time)', '⚠️ SYNTHETIC / PROTOTYPE'),
    
    # Issue 9: Benchmark index report 05
    ('Zvec ANN recall', 'TurboQuant exhaustive SIMD search'),
    
    # Issue 10: TurboQuant claims exact-match
    ('exact-match semantic memory slicing', 'exhaustive quantized cosine search'),
    
    # Issue 12: TurboQuant scalability
    ('infinitely via TurboQuant', 'extended via TurboQuant (O(N) exhaustive scan)'),
    
    # Issue 16: Dependency wording
    ('without external runtime dependencies', 'without external ML runtime dependencies'),
    
    # Issue 17: Benchmark reproducibility
    ('Date:** 2026-09-29', 'Date:** 2026-09-30'),
    
    # Issue 18: Benchmark evidence FROZEN / VERIFIED -> STALE
    ('🟢 FROZEN & EMPIRICALLY VERIFIED', '⚠️ OUTDATED / RE-VALIDATION REQUIRED'),
    
    # Issue 19: Resource policy hardcoded 2 threads, 16 GB, 85% VRAM
    ('16 GB headroom limit', 'ResourcePolicy RAM ceiling'),
    ('2 OpenMP compute threads', 'half of hardware threads'),
    
    # Issue 21: Master report Vector Search 512-dim
    ('512-dim Cosine Similarity', 'TurboQuant 1536-dim Quantized Cosine Search'),
]

for f in set(files):
    if not os.path.exists(f): continue
    with open(f, 'r') as file:
        content = file.read()
    
    original = content
    for old, new in replacements:
        content = content.replace(old, new)
        
    if content != original:
        with open(f, 'w') as file:
            file.write(content)
        print(f"Updated {f}")

