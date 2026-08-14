# Automated Neutral-Bit Analysis of Simeck

This repository contains the source code, raw experiment outputs, processed
tables, and figure source files supporting the manuscript **Automated
Neutral-Bit Extensions for Differential-Linear Cryptanalysis of Simeck**.

The experiments cover:

- exact three-round neutral-bit certification with Z3;
- Hamming-weight-stratified audits for Simeck-32, Simeck-48, and Simeck-64;
- joint symbolic checks for representative Simeck-64 masks;
- eleven-round Simeck-32 partial-key ranking experiments;
- bias-corrected correlation measurements for Simeck-32 and Simeck-48;
- an independent audit of the CRYPTO 2024 Simeck-48 endpoint configuration;
- published Simeck implementation-vector checks.

The fixed-seed scalability experiments characterize the sampled input
differences. They are not universal claims over all differences of the same
Hamming weight.

## Repository structure

```text
.
|-- src/                    Experiment implementations
|-- scripts/                Reproduction and analysis scripts
|-- data/raw/               Preserved experiment outputs
|-- data/processed/         Machine-readable summary tables
|-- figures/                Figures generated from the raw outputs
|-- docs/                   Reproduction and data documentation
|-- metadata/               Experiment manifest
|-- CITATION.cff            Citation metadata
|-- requirements.txt        Python dependencies
`-- SHA256SUMS.txt          File-integrity checksums
```

## Quick start

The reported experiments were run on Ubuntu 24.04 with GCC 13.3.0, Python
3.12, four virtual CPUs, 15 GiB RAM, and Z3 4.15.4.

```bash
sudo apt-get update
sudo apt-get install -y build-essential python3-venv
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
bash scripts/run_smoke_tests.sh
```

To regenerate the processed tables and figures from the included raw outputs:

```bash
python scripts/analyze_results.py
```

To rerun the full experiment suite:

```bash
bash scripts/run_paper_experiments.sh
```

The full script evaluates billions of block pairs and is substantially more
expensive than the smoke tests. It writes new outputs to `reproduced_outputs/`
and does not overwrite the archived raw data.

## Main archived results

- The 16-bit Simeck-32 candidate mask has a concrete counterexample, while the
  seven-bit mask `0x0400ac21` passes the exact joint query.
- The Simeck-32/48 scalability audit contains 416 fixed-seed configurations.
- The Simeck-64 audit contains 96 fixed-seed configurations and three joint
  `unsat` checks.
- The eleven-round ranking statistic separates four key classes determined by
  two last-round key bits.
- The eight-round Simeck-32 positive control reproduces the expected signal.
- The 14-round Simeck-48 experiment retains a weak signal; the investigated
  17-round configuration is statistically consistent with zero under the
  unbiased squared-correlation estimator.

See [docs/experiment-map.md](docs/experiment-map.md) for the mapping between
each experiment, command, raw output, processed table, and manuscript result.

## Integrity and citation

Verify the archived files with:

```bash
python scripts/checksums.py --verify
```

Use the citation metadata in `CITATION.cff` when citing this repository. After
creating a GitHub release, archive that release in Zenodo and add the assigned
DOI to the repository description and manuscript availability statements.

## Licences and provenance

Source code in this repository is released under the MIT License. Data and
generated figures are released under CC BY 4.0. Portions of the validation and
correlation workflow build on openly released reference implementations; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
