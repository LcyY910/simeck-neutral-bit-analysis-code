# Data dictionary

## Neutral-bit JSON files

- `difference`, `diff_l`, `diff_r`: sampled input difference and its two words.
- `neutral_bits`: bit positions passing the exact single-bit query.
- `neutral_count`: number of passing positions.
- `independent_neutral_count`: passing positions excluding active difference bits.
- `joint_status`: solver result for the full candidate mask.
- `joint_counterexample`: model returned when the joint query is satisfiable.
- `empty_rate`: fraction of sampled configurations with no passing bit.
- `seed`: fixed pseudorandom sampling seed.

All bit positions are zero-based. For a state with word size `n`, positions
`0` through `n-1` refer to the right word and positions `n` through `2n-1`
refer to the left word.

## Key-ranking logs

- `plaintexts`, `pairs`: chosen-plaintext and unordered-pair counts.
- `top_included`: trials in which the correct key class shares the top score.
- `unique_top`: trials in which the correct key class is uniquely top-ranked.
- `random_tiebreak_success`: expected success under uniform tie breaking.
- `mean_rank`: mean rank of the correct four-way key class.
- `ties1` through `ties4`: number of trials with the indicated number of tied classes.

## Correlation logs

- `mean_signed`: mean signed empirical correlation across keys.
- `mean_abs`: mean absolute empirical correlation across keys.
- `mean_unbiased_sq`: finite-sample unbiased estimate of squared correlation.
- `sq_se`: standard error of the mean unbiased squared-correlation estimate.
- `rms`: square root of the positive part of `mean_unbiased_sq`.
- `control_*`: the same statistics for the prespecified control mask.
- `sample_log2`: base-2 logarithm of the number of pairs per key.

## Processed tables

CSV files use decimal floating-point values and UTF-8 encoding. Confidence
interval fields correspond to the calculations in `scripts/analyze_results.py`.
No missing-value sentinel is used; unavailable JSON quantities are represented
as `null`.
