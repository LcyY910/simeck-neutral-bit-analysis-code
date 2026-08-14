#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${PYTHON:-python3}"
CC="${CC:-gcc}"
OUT="$ROOT/smoke_outputs"
BUILD="$ROOT/build"

mkdir -p "$OUT" "$BUILD"

"$PYTHON" -m py_compile "$ROOT"/src/*.py "$ROOT/scripts/analyze_results.py"

"$CC" -O2 -Wall -Wextra -Wpedantic \
  "$ROOT/src/test_vectors/main.c" -o "$BUILD/test_vectors"
"$BUILD/test_vectors" > "$OUT/test_vectors.out"
grep -Fxq "Simeck32/64 770d 2c76" "$OUT/test_vectors.out"
grep -Fxq "Simeck48/96 f3cf25 e33b36" "$OUT/test_vectors.out"
grep -Fxq "Simeck64/128 45ce6902 5f7ab7ed" "$OUT/test_vectors.out"

"$PYTHON" "$ROOT/src/neutral_bit_audit.py" > "$OUT/neutral_bit_audit.json"
"$PYTHON" "$ROOT/src/neutral_bit_scalability.py" \
  --samples 1 --workers 1 > "$OUT/neutral_bit_scalability.json"
"$PYTHON" "$ROOT/src/neutral_bit_simeck64.py" \
  --samples 1 --workers 1 > "$OUT/neutral_bit_simeck64.json"
"$PYTHON" "$ROOT/src/neutral_bit_simeck64_joint.py" \
  > "$OUT/neutral_bit_simeck64_joint.json"

"$CC" -O2 -fopenmp -Wall -Wextra -Wpedantic \
  "$ROOT/src/partial_key_ranking.c" -lm -o "$BUILD/partial_key_ranking"
"$BUILD/partial_key_ranking" 2 0x4b43555256454131 1 \
  > "$OUT/partial_key_ranking.log"

"$CC" -O2 -fopenmp -Wall -Wextra -Wpedantic \
  -DKEY_COUNT=4 -DSAMPLE_LOG2=12 -DTEST_ROUNDS=8 \
  "$ROOT/src/correlation_simeck32.c" -lm -o "$BUILD/correlation_simeck32"
"$BUILD/correlation_simeck32" > "$OUT/correlation_simeck32.log"

"$CC" -O2 -fopenmp -Wall -Wextra -Wpedantic \
  -DKEY_COUNT=4 -DSAMPLE_LOG2=12 -DTEST_ROUNDS=14 \
  -DTEST_DP_L=0 -DTEST_DP_R=0x000100u \
  -DTEST_LC_L=0 -DTEST_LC_R=0x000400u \
  "$ROOT/src/correlation_simeck48.c" -lm -o "$BUILD/correlation_simeck48"
"$BUILD/correlation_simeck48" > "$OUT/correlation_simeck48.log"

"$PYTHON" "$ROOT/scripts/analyze_results.py" > "$OUT/analysis_summary.log"
echo "Smoke tests completed successfully."
