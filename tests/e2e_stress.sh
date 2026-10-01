#!/bin/bash
# e2e_stress.sh: End-to-End stress test for DenseLite context and tool loops
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "$SCRIPT_DIR/e2e_assert_stress.py" "$@"
