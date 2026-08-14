from __future__ import annotations

import argparse
import json
import math
import random
import statistics
import time
from collections import Counter
from multiprocessing import Pool

from neutral_bit_audit import single_bit_status


def sampled_differences(width: int, weights: list[int], samples: int, seed: int):
    rng = random.Random(seed ^ (width << 48))
    full_width = 2 * width
    for weight in weights:
        seen: set[int] = set()
        target = min(samples, math.comb(full_width, weight))
        while len(seen) < target:
            positions = rng.sample(range(full_width), weight)
            value = sum(1 << bit for bit in positions)
            seen.add(value)
        for index, value in enumerate(sorted(seen)):
            yield {
                "name": f"simeck{2 * width}_w{weight}_i{index}",
                "width": width,
                "weight": weight,
                "difference": value,
                "diff_l": value >> width,
                "diff_r": value & ((1 << width) - 1),
            }


def audit_one(config: dict) -> dict:
    started = time.perf_counter()
    neutral_bits = []
    for bit in range(2 * config["width"]):
        valid, _ = single_bit_status(
            config["name"],
            config["width"],
            config["diff_l"],
            config["diff_r"],
            bit,
        )
        if valid:
            neutral_bits.append(bit)

    active_bits = {
        bit
        for bit in range(2 * config["width"])
        if (config["difference"] >> bit) & 1
    }
    result = dict(config)
    result["neutral_bits"] = neutral_bits
    result["neutral_count"] = len(neutral_bits)
    result["independent_neutral_count"] = len(set(neutral_bits) - active_bits)
    result["elapsed_seconds"] = time.perf_counter() - started
    return result


def percentile(values: list[int], fraction: float) -> float:
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return float(ordered[lower])
    return ordered[lower] * (upper - position) + ordered[upper] * (position - lower)


def summarize(results: list[dict]) -> list[dict]:
    summaries = []
    groups = sorted({(r["width"], r["weight"]) for r in results})
    for width, weight in groups:
        group = [
            r for r in results if r["width"] == width and r["weight"] == weight
        ]
        counts = [r["neutral_count"] for r in group]
        independent = [r["independent_neutral_count"] for r in group]
        summaries.append(
            {
                "cipher": f"Simeck-{2 * width}",
                "state_width": 2 * width,
                "difference_weight": weight,
                "samples": len(group),
                "neutral_mean": statistics.fmean(counts),
                "neutral_median": statistics.median(counts),
                "neutral_q25": percentile(counts, 0.25),
                "neutral_q75": percentile(counts, 0.75),
                "neutral_min": min(counts),
                "neutral_max": max(counts),
                "empty_count": sum(value == 0 for value in counts),
                "empty_rate": sum(value == 0 for value in counts) / len(counts),
                "independent_mean": statistics.fmean(independent),
                "independent_empty_count": sum(value == 0 for value in independent),
                "independent_empty_rate": sum(value == 0 for value in independent)
                / len(independent),
                "neutral_histogram": dict(sorted(Counter(counts).items())),
            }
        )
    return summaries


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", type=int, default=32)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--seed", type=lambda value: int(value, 0), default=0x4E425343414C4531)
    args = parser.parse_args()

    configs = list(
        sampled_differences(16, [1, 2, 4, 8, 16, 24], args.samples, args.seed)
    )
    configs.extend(
        sampled_differences(24, [1, 2, 4, 8, 16, 24, 32], args.samples, args.seed)
    )

    started = time.perf_counter()
    with Pool(processes=args.workers) as pool:
        results = list(pool.imap_unordered(audit_one, configs, chunksize=1))
    results.sort(key=lambda r: (r["width"], r["weight"], r["difference"]))

    output = {
        "method": "Z3 universal single-bit semantic check",
        "rounds": 3,
        "plaintext_quantification": "all",
        "round_key_quantification": "all three arbitrary round-key words",
        "sampling": "fixed-seed uniform sampling without replacement within each Hamming-weight stratum",
        "samples_per_stratum": args.samples,
        "seed": args.seed,
        "workers": args.workers,
        "configuration_count": len(configs),
        "elapsed_seconds": time.perf_counter() - started,
        "summary": summarize(results),
        "results": results,
    }
    print(json.dumps(output, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
