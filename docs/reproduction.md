# Reproduction guide

## Tested environment

- Ubuntu 24.04.3 LTS
- Linux kernel 6.14.0-36-generic
- x86-64, four virtual CPUs, 15 GiB RAM
- GCC 13.3.0 with OpenMP
- Python 3.12
- Z3 4.15.4

The scripts use repository-relative paths and do not require the original
virtual-machine directory.

## Installation

```bash
sudo apt-get update
sudo apt-get install -y build-essential python3-venv
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

## Verification levels

1. `bash scripts/run_smoke_tests.sh` checks imports, implementation vectors,
   small symbolic audits, small correlation runs, and result processing.
2. `python scripts/analyze_results.py` regenerates all processed tables and
   figures from the archived raw outputs.
3. `bash scripts/run_paper_experiments.sh` reruns the reported experiment
   configurations and writes them to `reproduced_outputs/`.

The full correlation and key-ranking runs are CPU-intensive. Exact elapsed
times vary with compiler, processor, thread count, and system load. Fixed seeds
make the generated inputs deterministic for a given implementation.

## Comparing reproduced outputs

Numerical logs generated with the same compiler and floating-point environment
should closely match the archived results. Timing files are environmental and
are not expected to match. Re-run `scripts/analyze_results.py` after replacing
the archived logs only when intentionally preparing a new repository release.
