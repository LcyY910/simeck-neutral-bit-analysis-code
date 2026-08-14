# Experiment map

| ID | Experiment | Implementation | Archived output | Derived output |
|---|---|---|---|---|
| EXP-01 | Published implementation vectors | `src/test_vectors/` | `data/raw/validation/simeck_vectors.out` | validation only |
| EXP-02 | Exact three-round mask audit | `src/neutral_bit_audit.py` | `data/raw/neutral_bits/nb_full_audit.json` | manuscript neutral-bit table |
| EXP-03 | Simeck-32/48 scalability audit, 416 configurations | `src/neutral_bit_scalability.py` | `data/raw/neutral_bits/nb_scalability.json` | `data/processed/nb_scalability_summary.csv`, `figures/fig_nb_scalability.*` |
| EXP-04 | Simeck-64 scalability audit, 96 configurations | `src/neutral_bit_simeck64.py` | `data/raw/neutral_bits/nb_simeck64.json` | `data/processed/simeck64_scalability_summary.csv` |
| EXP-05 | Simeck-64 representative joint checks | `src/neutral_bit_simeck64_joint.py` | `data/raw/neutral_bits/nb_simeck64_joint.json` | `data/processed/analysis_summary.json` |
| EXP-06 | Seven-bit stress test and early exhaustive ranking checks | `src/simeck32_experiment.c` | `data/raw/exploratory/` | validation only |
| EXP-07 | Eleven-round four-class key ranking | `src/partial_key_ranking.c` | `data/raw/key_ranking/keycurve_seed_*.log` | `data/processed/key_recovery_curve.csv`, `figures/fig_key_recovery_curve.*` |
| EXP-08 | Simeck-32 eight-round positive control | `src/correlation_simeck32.c` | `data/raw/correlations/corr32_r8.log` | `data/processed/correlation_summary.csv` |
| EXP-09 | Simeck-48 fourteen-round replications | `src/correlation_simeck48.c` | `data/raw/correlations/corr48_r14_seed_*.log` | `data/processed/correlation_summary.csv`, `figures/fig_correlation_validation.*` |
| EXP-10 | Simeck-48 seventeen-round replications | `src/correlation_simeck48.c` | `data/raw/correlations/corr48_r17_seed_*.log` | `data/processed/correlation_summary.csv`, `figures/fig_correlation_validation.*` |
| EXP-11 | CRYPTO 2024 Table 18 endpoint audit | `src/correlation_simeck48.c` | `data/raw/correlations/baseline17_table18_large.log` | manuscript endpoint-audit table |

Files under `data/raw/exploratory/` document preliminary scaling checks but are
not used in the final processed tables. Timing and build logs are preserved for
provenance and are not scientific response variables.
