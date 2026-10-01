#!/usr/bin/env python3
"""
test_model_checksum_coverage.py - Model Checksum Governance Test (Point 12)
Validates that every selectable model in start.sh and env.example has a verified,
canonical 64-character SHA-256 entry in models/checksums.sha256.
"""

import os
import re
import sys

def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    checksums_file = os.path.join(repo_root, "models", "checksums.sha256")
    env_example_file = os.path.join(repo_root, "env.example")
    start_sh_file = os.path.join(repo_root, "start.sh")

    if not os.path.isfile(checksums_file):
        print(f"FAIL: Checksums file missing at {checksums_file}")
        sys.exit(1)

    # Load canonical checksums
    canonical_checksums = {}
    with open(checksums_file, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 2:
                sha = parts[0]
                filepath = parts[1]
                canonical_checksums[filepath] = sha
                canonical_checksums[os.path.basename(filepath)] = sha

    models_to_verify = set()

    # 1. Parse env.example
    with open(env_example_file, "r") as f:
        for line in f:
            m = re.match(r'^MODEL_[A-Z0-9_]+_FILE="([^"]+)"', line.strip())
            if m:
                models_to_verify.add(m.group(1))

    # 2. Parse start.sh selectable models (MAIN_FILE, CODER_FILE, SMOLLM_FILE, NOMIC_FILE)
    with open(start_sh_file, "r") as f:
        for line in f:
            m = re.match(r'^(MAIN|CODER|SMOLLM|NOMIC|MODEL_[A-Z0-9_]+)_FILE="([^"]+)"', line.strip())
            if m:
                models_to_verify.add(m.group(2))

    print(f"[TEST] Verifying {len(models_to_verify)} configured/selectable models against models/checksums.sha256...")

    missing = []
    for model in sorted(models_to_verify):
        basename = os.path.basename(model)
        has_hash = (model in canonical_checksums) or (basename in canonical_checksums)
        if has_hash:
            sha = canonical_checksums.get(model) or canonical_checksums.get(basename)
            if len(sha) == 64 and all(c in "0123456789abcdefABCDEF" for c in sha):
                print(f"  ✓ {model} -> {sha[:16]}... (VALID)")
            else:
                print(f"  ✗ {model} -> INVALID HASH: {sha}")
                missing.append(model)
        else:
            print(f"  ✗ {model} -> MISSING FROM checksums.sha256")
            missing.append(model)

    if missing:
        print(f"\nFAIL: {len(missing)} model(s) lack canonical SHA-256 checksums!")
        sys.exit(1)

    print(f"\n>>> 100% of selectable models have valid canonical SHA-256 checksums! <<<\n")
    sys.exit(0)

if __name__ == "__main__":
    main()
