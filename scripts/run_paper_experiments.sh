#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${PYTHON:-python3}"
CC="${CC:-gcc}"
THREADS="${THREADS:-4}"
OUT="${1:-$ROOT/reproduced_outputs}"
BUILD="$OUT/build"

mkdir -p "$OUT/neutral_bits" "$OUT/key_ranking" \
  "$OUT/correlations" "$OUT/validation" "$BUILD"

"$CC" -O3 -Wall -Wextra -Wpedantic \
  "$ROOT/src/test_vectors/main.c" -o "$BUILD/test_vectors"
"$BUILD/test_vectors" > "$OUT/validation/simeck_vectors.out"

"$PYTHON" "$ROOT/src/neutral_bit_audit.py" \
  > "$OUT/neutral_bits/nb_full_audit.json"
"$PYTHON" "$ROOT/src/neutral_bit_scalability.py" \
  --samples 32 --workers "$THREADS" > "$OUT/neutral_bits/nb_scalability.json"
"$PYTHON" "$ROOT/src/neutral_bit_simeck64.py" \
  --samples 16 --workers "$THREADS" > "$OUT/neutral_bits/nb_simeck64.json"
"$PYTHON" "$ROOT/src/neutral_bit_simeck64_joint.py" \
  > "$OUT/neutral_bits/nb_simeck64_joint.json"

"$CC" -O3 -march=native -fopenmp -Wall -Wextra -Wpedantic \
  "$ROOT/src/partial_key_ranking.c" -lm -o "$BUILD/partial_key_ranking"
"$BUILD/partial_key_ranking" 10000 0x4b43555256454131 "$THREADS" \
  > "$OUT/key_ranking/keycurve_seed_a_2p20.log"
"$BUILD/partial_key_ranking" 10000 0x4b43555256454232 "$THREADS" \
  > "$OUT/key_ranking/keycurve_seed_b_2p20.log"
"$BUILD/partial_key_ranking" 5000 0x4b43555256454131 "$THREADS" \
  > "$OUT/key_ranking/keycurve_seed_a_2p21.log"
"$BUILD/partial_key_ranking" 5000 0x4b43555256454232 "$THREADS" \
  > "$OUT/key_ranking/keycurve_seed_b_2p21.log"

"$CC" -O3 -march=native -fopenmp -Wall -Wextra -Wpedantic \
  -DKEY_COUNT=64 -DSAMPLE_LOG2=18 -DTEST_ROUNDS=8 \
  "$ROOT/src/correlation_simeck32.c" -lm -o "$BUILD/corr32_r8"
"$BUILD/corr32_r8" > "$OUT/correlations/corr32_r8.log"

build_corr48() {
  local output="$1"
  shift
  "$CC" -O3 -march=native -fopenmp -Wall -Wextra -Wpedantic \
    "$@" "$ROOT/src/correlation_simeck48.c" -lm -o "$BUILD/$output"
}

build_corr48 corr48_r14_seed_a \
  -DKEY_COUNT=64 -DSAMPLE_LOG2=26 -DTEST_ROUNDS=14 \
  -DTEST_DP_L=0 -DTEST_DP_R=0x000100u \
  -DTEST_LC_L=0 -DTEST_LC_R=0x000400u \
  -DBASE_SEED=0x48c0decafef00d00ull
"$BUILD/corr48_r14_seed_a" > "$OUT/correlations/corr48_r14_seed_a.log"

build_corr48 corr48_r14_seed_b \
  -DKEY_COUNT=64 -DSAMPLE_LOG2=26 -DTEST_ROUNDS=14 \
  -DTEST_DP_L=0 -DTEST_DP_R=0x000100u \
  -DTEST_LC_L=0 -DTEST_LC_R=0x000400u \
  -DBASE_SEED=0x14325245504c4943ull
"$BUILD/corr48_r14_seed_b" > "$OUT/correlations/corr48_r14_seed_b.log"

build_corr48 corr48_r17_seed_a \
  -DKEY_COUNT=64 -DSAMPLE_LOG2=25 -DTEST_ROUNDS=17 \
  -DTEST_DP_L=0x000100u -DTEST_DP_R=0x000200u \
  -DTEST_LC_L=0x000200u -DTEST_LC_R=0x000710u \
  -DBASE_SEED=0x48c0decafef00d00ull
"$BUILD/corr48_r17_seed_a" > "$OUT/correlations/corr48_r17_seed_a.log"

build_corr48 corr48_r17_seed_b \
  -DKEY_COUNT=64 -DSAMPLE_LOG2=26 -DTEST_ROUNDS=17 \
  -DTEST_DP_L=0x000100u -DTEST_DP_R=0x000200u \
  -DTEST_LC_L=0x000200u -DTEST_LC_R=0x000710u \
  -DBASE_SEED=0x17325245504c4943ull
"$BUILD/corr48_r17_seed_b" > "$OUT/correlations/corr48_r17_seed_b.log"

build_corr48 baseline17_table18 \
  -DKEY_COUNT=64 -DSAMPLE_LOG2=26 -DTEST_ROUNDS=17 \
  -DTEST_DP_L=0x000010u -DTEST_DP_R=0x000020u \
  -DTEST_LC_L=0x000010u -DTEST_LC_R=0x000008u \
  -DBASE_SEED=0x1700000000000001ull
"$BUILD/baseline17_table18" \
  > "$OUT/correlations/baseline17_table18_large.log"

echo "Full experiment suite completed: $OUT"
