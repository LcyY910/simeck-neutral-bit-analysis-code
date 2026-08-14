from __future__ import annotations

import csv
import json
import math
import re
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from scipy import stats


ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data" / "raw"
NB_DATA = DATA / "neutral_bits"
KEY_DATA = DATA / "key_ranking"
CORR_DATA = DATA / "correlations"
TABLES = ROOT / "data" / "processed"
FIGURES = ROOT / "figures"

RESULT_RE = re.compile(r"^RESULT (.+)$", re.MULTILINE)
KEY_RE = re.compile(
    r"^KEY id=(?P<id>\d+) signed=\s*(?P<signed>[-+0-9.eE]+) "
    r"abs=(?P<abs>[-+0-9.eE]+) unbiased_sq=\s*(?P<sq>[-+0-9.eE]+) "
    r"control_signed=\s*(?P<ctl_signed>[-+0-9.eE]+) "
    r"control_abs=(?P<ctl_abs>[-+0-9.eE]+) "
    r"control_unbiased_sq=\s*(?P<ctl_sq>[-+0-9.eE]+)$",
    re.MULTILINE,
)
SUMMARY_RE = re.compile(
    r"^(?P<label>TARGET|CONTROL) mean_signed=\s*(?P<signed>[-+0-9.eE]+) "
    r"mean_abs=(?P<abs>[-+0-9.eE]+) abs_exp=(?P<abs_exp>[-+0-9.eE]+) "
    r"mean_unbiased_sq=\s*(?P<sq>[-+0-9.eE]+) "
    r"sq_se=(?P<se>[-+0-9.eE]+) z=(?P<z>[-+0-9.eE]+) "
    r"rms=(?P<rms>[-+0-9.eE]+) rms_exp=(?P<rms_exp>[-+0-9.eEinf]+)$",
    re.MULTILINE,
)
CONFIG_RE = re.compile(
    r"CONFIG rounds=(?P<rounds>\d+) keys=(?P<keys>\d+) "
    r"samples_per_key=2\^(?P<sample_log2>\d+)"
)


def wilson(successes: int, trials: int, z: float = 1.959963984540054) -> tuple[float, float]:
    p = successes / trials
    denom = 1 + z * z / trials
    center = (p + z * z / (2 * trials)) / denom
    half = z * math.sqrt(p * (1 - p) / trials + z * z / (4 * trials * trials)) / denom
    return center - half, center + half


def parse_keycurve(path: Path) -> list[dict]:
    rows = []
    for match in RESULT_RE.finditer(path.read_text(encoding="utf-8")):
        values = match.group(1).split(",")
        rows.append(
            {
                "plaintexts": int(values[0]),
                "pairs": int(values[1]),
                "trials": int(values[2]),
                "top_included": int(values[3]),
                "unique_top": int(values[4]),
                "rank_le_2": int(values[5]),
                "rank_le_3": int(values[6]),
                "random_tiebreak_rate": float(values[7]),
                "mean_rank": float(values[8]),
                "mean_correct_abs_corr": float(values[9]),
                "mean_correct_signed_corr": float(values[10]),
                "mean_best_wrong_abs_corr": float(values[11]),
                "ties1": int(values[12]),
                "ties2": int(values[13]),
                "ties3": int(values[14]),
                "ties4": int(values[15]),
            }
        )
    return rows


def combined_keycurve() -> list[dict]:
    sources = {
        "A": parse_keycurve(KEY_DATA / "keycurve_seed_a_2p20.log"),
        "B": parse_keycurve(KEY_DATA / "keycurve_seed_b_2p20.log"),
    }
    high_sources = {
        "A": parse_keycurve(KEY_DATA / "keycurve_seed_a_2p21.log"),
        "B": parse_keycurve(KEY_DATA / "keycurve_seed_b_2p21.log"),
    }
    levels = [2**10, 2**12, 2**14, 2**16, 2**18, 2**20, 2**21]
    output = []
    for plaintexts in levels:
        chosen = high_sources if plaintexts == 2**21 else sources
        rows = [next(row for row in chosen[seed] if row["plaintexts"] == plaintexts) for seed in ("A", "B")]
        trials = sum(row["trials"] for row in rows)
        top = sum(row["top_included"] for row in rows)
        unique = sum(row["unique_top"] for row in rows)
        top_ci = wilson(top, trials)
        unique_ci = wilson(unique, trials)
        weighted = lambda field: sum(row[field] * row["trials"] for row in rows) / trials
        output.append(
            {
                "log2_plaintexts": int(math.log2(plaintexts)),
                "plaintexts": plaintexts,
                "pairs": plaintexts // 2,
                "trials": trials,
                "top_included": top,
                "top_rate": top / trials,
                "top_ci_low": top_ci[0],
                "top_ci_high": top_ci[1],
                "unique_top": unique,
                "unique_top_rate": unique / trials,
                "unique_ci_low": unique_ci[0],
                "unique_ci_high": unique_ci[1],
                "random_tiebreak_rate": weighted("random_tiebreak_rate"),
                "mean_rank": weighted("mean_rank"),
                "mean_correct_abs_corr": weighted("mean_correct_abs_corr"),
                "mean_correct_signed_corr": weighted("mean_correct_signed_corr"),
                "mean_best_wrong_abs_corr": weighted("mean_best_wrong_abs_corr"),
                "seed_a_top_rate": rows[0]["top_included"] / rows[0]["trials"],
                "seed_b_top_rate": rows[1]["top_included"] / rows[1]["trials"],
            }
        )
    return output


def parse_corr(path: Path, label: str) -> dict:
    text = path.read_text(encoding="utf-8")
    config = CONFIG_RE.search(text)
    summaries = {m.group("label"): m.groupdict() for m in SUMMARY_RE.finditer(text)}
    keys = [m.groupdict() for m in KEY_RE.finditer(text)]
    target = summaries["TARGET"]
    result = {
        "label": label,
        "rounds": int(config.group("rounds")),
        "keys": int(config.group("keys")),
        "sample_log2": int(config.group("sample_log2")),
        "total_log2": math.log2(int(config.group("keys"))) + int(config.group("sample_log2")),
        "mean_signed": float(target["signed"]),
        "mean_abs": float(target["abs"]),
        "abs_exp": float(target["abs_exp"]),
        "mean_unbiased_sq": float(target["sq"]),
        "sq_se": float(target["se"]),
        "rms": float(target["rms"]),
        "rms_exp": float(target["rms_exp"]) if target["rms_exp"] != "inf" else math.inf,
        "key_sq": [float(item["sq"]) for item in keys],
    }
    if result["key_sq"]:
        values = np.asarray(result["key_sq"])
        critical = stats.t.ppf(0.975, len(values) - 1)
        mean = float(values.mean())
        se = float(stats.sem(values))
        result.update(
            {
                "mean_unbiased_sq": mean,
                "sq_se": se,
                "ci_low": mean - critical * se,
                "ci_high": mean + critical * se,
                "p_signal_one_sided": float(stats.ttest_1samp(values, 0.0, alternative="greater").pvalue),
            }
        )
    return result


def rms_bound(value: float) -> float:
    return math.sqrt(value) if value > 0 else 0.0


def correlation_analysis() -> dict:
    r8 = parse_corr(CORR_DATA / "corr32_r8.log", "Simeck-32 8-round positive control")
    r14a = parse_corr(CORR_DATA / "corr48_r14_seed_a.log", "Simeck-48 14-round seed A")
    r14b = parse_corr(CORR_DATA / "corr48_r14_seed_b.log", "Simeck-48 14-round seed B")
    r17a = parse_corr(CORR_DATA / "corr48_r17_seed_a.log", "Simeck-48 17-round seed A")
    r17b = parse_corr(CORR_DATA / "corr48_r17_seed_b.log", "Simeck-48 17-round seed B")

    r14_values = np.asarray(r14a["key_sq"] + r14b["key_sq"])
    r14_mean = float(r14_values.mean())
    r14_se = float(stats.sem(r14_values))
    r14_crit = stats.t.ppf(0.975, len(r14_values) - 1)
    r14_p = float(stats.ttest_1samp(r14_values, 0.0, alternative="greater").pvalue)
    r14_pooled = {
        "label": "Simeck-48 14-round pooled",
        "keys": len(r14_values),
        "total_samples": 2 * 2**32,
        "mean_unbiased_sq": r14_mean,
        "sq_se": r14_se,
        "ci_low": r14_mean - r14_crit * r14_se,
        "ci_high": r14_mean + r14_crit * r14_se,
        "p_signal_one_sided": r14_p,
        "rms": rms_bound(r14_mean),
    }

    weights = np.asarray([1 / r17a["sq_se"] ** 2, 1 / r17b["sq_se"] ** 2])
    means = np.asarray([r17a["mean_unbiased_sq"], r17b["mean_unbiased_sq"]])
    r17_mean = float(np.sum(weights * means) / np.sum(weights))
    r17_se = float(math.sqrt(1 / np.sum(weights)))
    r17_p = float(stats.norm.sf(r17_mean / r17_se))
    r17_pooled = {
        "label": "Simeck-48 17-round inverse-variance pooled",
        "keys": r17a["keys"] + r17b["keys"],
        "total_samples": 2**31 + 2**32,
        "mean_unbiased_sq": r17_mean,
        "sq_se": r17_se,
        "ci_low": r17_mean - 1.959963984540054 * r17_se,
        "ci_high": r17_mean + 1.959963984540054 * r17_se,
        "p_signal_one_sided": r17_p,
        "rms": rms_bound(r17_mean),
    }

    raw_p = [r14_p, r17_p]
    order = np.argsort(raw_p)
    holm = [0.0, 0.0]
    running = 0.0
    for rank, index in enumerate(order):
        adjusted = min(1.0, (len(raw_p) - rank) * raw_p[index])
        running = max(running, adjusted)
        holm[index] = running
    r14_pooled["holm_p_two_targets"] = holm[0]
    r17_pooled["holm_p_two_targets"] = holm[1]

    claims = {
        "r8_corr": 2 ** -1.1387,
        "r14_corr": 2 ** -12.2488,
        "r17_corr": 2 ** -12.28,
    }
    r14_pooled["claimed_corr"] = claims["r14_corr"]
    r14_pooled["claimed_sq"] = claims["r14_corr"] ** 2
    r14_pooled["claim_above_estimate_se"] = (
        r14_pooled["claimed_sq"] - r14_mean
    ) / r14_se
    r17_pooled["claimed_corr"] = claims["r17_corr"]
    r17_pooled["claimed_sq"] = claims["r17_corr"] ** 2
    r17_pooled["claim_above_estimate_se"] = (
        r17_pooled["claimed_sq"] - r17_mean
    ) / r17_se

    for run in (r14a, r14b, r17a, r17b):
        run["rms_ci_low"] = rms_bound(run["ci_low"])
        run["rms_ci_high"] = rms_bound(run["ci_high"])
    for pooled in (r14_pooled, r17_pooled):
        pooled["rms_ci_low"] = rms_bound(pooled["ci_low"])
        pooled["rms_ci_high"] = rms_bound(pooled["ci_high"])

    return {
        "positive_control": r8,
        "r14_runs": [r14a, r14b],
        "r14_pooled": r14_pooled,
        "r17_runs": [r17a, r17b],
        "r17_pooled": r17_pooled,
        "claims": claims,
    }


def write_csv(path: Path, rows: list[dict], fields: list[str] | None = None) -> None:
    if fields is None:
        fields = list(rows[0])
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def configure_plots() -> None:
    plt.rcParams.update(
        {
            "font.family": "serif",
            "font.serif": ["Times New Roman", "DejaVu Serif"],
            "font.size": 9,
            "axes.labelsize": 9,
            "legend.fontsize": 8,
            "figure.dpi": 150,
            "savefig.dpi": 300,
            "savefig.bbox": "tight",
            "axes.spines.top": False,
            "axes.spines.right": False,
            "axes.grid": True,
            "grid.alpha": 0.18,
            "grid.linestyle": "-",
            "lines.linewidth": 1.8,
            "lines.markersize": 5,
        }
    )


def plot_keycurve(rows: list[dict]) -> None:
    x = np.asarray([row["log2_plaintexts"] for row in rows])
    top = np.asarray([100 * row["top_rate"] for row in rows])
    low = np.asarray([100 * row["top_ci_low"] for row in rows])
    high = np.asarray([100 * row["top_ci_high"] for row in rows])
    tie = np.asarray([100 * row["random_tiebreak_rate"] for row in rows])
    fig, ax = plt.subplots(figsize=(4.9, 3.0))
    ax.errorbar(
        x,
        top,
        yerr=np.vstack([top - low, high - top]),
        color="#0072B2",
        marker="o",
        capsize=2.5,
        label="Correct class is top-ranked",
        zorder=3,
    )
    ax.plot(x, tie, color="#D55E00", marker="s", linestyle="--",
            label="Random tie-break success")
    ax.axhline(25, color="#777777", linestyle=":", linewidth=1.2,
               label="Four-class random baseline")
    ax.axhline(95, color="#009E73", linestyle="-.", linewidth=1.2,
               label="95% target")
    ax.set_xticks(x)
    ax.set_xticklabels([rf"$2^{{{value}}}$" for value in x])
    ax.set_xlabel("Chosen plaintexts")
    ax.set_ylabel("Success rate (%)")
    ax.set_ylim(20, 101)
    ax.legend(loc="lower right", ncol=1)
    fig.tight_layout()
    fig.savefig(FIGURES / "fig_key_recovery_curve.pdf")
    fig.savefig(FIGURES / "fig_key_recovery_curve.png")
    plt.close(fig)


def plot_nb_scalability(summary: list[dict]) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(6.75, 2.7), sharey=False)
    for ax, cipher in zip(axes, ("Simeck-32", "Simeck-48")):
        rows = [row for row in summary if row["cipher"] == cipher]
        weights = np.asarray([row["difference_weight"] for row in rows])
        means = np.asarray([row["neutral_mean"] for row in rows])
        empty = np.asarray([100 * row["empty_rate"] for row in rows])
        ax.plot(weights, means, color="#0072B2", marker="o", label="Mean deterministic NBs")
        ax.set_xlabel("Input-difference Hamming weight")
        ax.set_ylabel("Mean deterministic NB count", color="#0072B2")
        ax.tick_params(axis="y", labelcolor="#0072B2")
        ax.set_xticks(weights)
        ax2 = ax.twinx()
        ax2.plot(weights, empty, color="#D55E00", marker="s", linestyle="--",
                 label="Empty-set rate")
        ax2.set_ylim(-3, 103)
        ax2.set_ylabel("Empty-set rate (%)", color="#D55E00")
        ax2.tick_params(axis="y", labelcolor="#D55E00")
        ax.set_title(cipher)
        handles1, labels1 = ax.get_legend_handles_labels()
        handles2, labels2 = ax2.get_legend_handles_labels()
        ax.legend(handles1 + handles2, labels1 + labels2, loc="center right")
    fig.tight_layout()
    fig.savefig(FIGURES / "fig_nb_scalability.pdf")
    fig.savefig(FIGURES / "fig_nb_scalability.png")
    plt.close(fig)


def plot_correlation(corr: dict) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(6.75, 2.75), sharex=False)
    panels = [
        (axes[0], corr["r14_runs"], corr["r14_pooled"], "Simeck-48, 14 rounds"),
        (axes[1], corr["r17_runs"], corr["r17_pooled"], "Simeck-48, 17 rounds"),
    ]
    for ax, runs, pooled, title in panels:
        entries = runs + [pooled]
        y = np.arange(len(entries))[::-1]
        means = np.asarray([entry["mean_unbiased_sq"] for entry in entries]) * 1e8
        lows = np.asarray([entry["ci_low"] for entry in entries]) * 1e8
        highs = np.asarray([entry["ci_high"] for entry in entries]) * 1e8
        colors = ["#56B4E9", "#56B4E9", "#0072B2"]
        for idx in range(len(entries)):
            ax.errorbar(
                means[idx], y[idx],
                xerr=[[means[idx] - lows[idx]], [highs[idx] - means[idx]]],
                color=colors[idx], marker="o" if idx < 2 else "D", capsize=3,
                markersize=5 if idx < 2 else 6, zorder=3,
            )
        ax.axvline(0, color="#666666", linewidth=1.0)
        ax.axvline(pooled["claimed_sq"] * 1e8, color="#D55E00", linestyle="--",
                   linewidth=1.4, label="Manuscript claim")
        ax.set_yticks(y)
        ax.set_yticklabels(["Seed A", "Seed B", "Pooled"])
        ax.set_xlabel(r"Mean unbiased $C^2$ ($\times 10^{-8}$)")
        ax.set_title(title)
        ax.legend(loc="lower right")
    fig.tight_layout()
    fig.savefig(FIGURES / "fig_correlation_validation.pdf")
    fig.savefig(FIGURES / "fig_correlation_validation.png")
    plt.close(fig)


def clean_for_json(value):
    if isinstance(value, dict):
        return {key: clean_for_json(item) for key, item in value.items() if key != "key_sq"}
    if isinstance(value, list):
        return [clean_for_json(item) for item in value]
    if isinstance(value, float) and not math.isfinite(value):
        return None
    return value


def main() -> None:
    TABLES.mkdir(parents=True, exist_ok=True)
    FIGURES.mkdir(parents=True, exist_ok=True)
    configure_plots()

    keycurve = combined_keycurve()
    write_csv(TABLES / "key_recovery_curve.csv", keycurve)

    nb = json.loads((NB_DATA / "nb_scalability.json").read_text(encoding="utf-8"))
    write_csv(
        TABLES / "nb_scalability_summary.csv",
        nb["summary"],
        [
            "cipher", "state_width", "difference_weight", "samples",
            "neutral_mean", "neutral_median", "neutral_q25", "neutral_q75",
            "neutral_min", "neutral_max", "empty_count", "empty_rate",
            "independent_mean", "independent_empty_count", "independent_empty_rate",
        ],
    )

    nb64 = json.loads((NB_DATA / "nb_simeck64.json").read_text(encoding="utf-8"))
    write_csv(
        TABLES / "simeck64_scalability_summary.csv",
        nb64["summary"],
        [
            "cipher", "state_width", "difference_weight", "samples",
            "neutral_mean", "neutral_median", "neutral_q25", "neutral_q75",
            "neutral_min", "neutral_max", "empty_count", "empty_rate",
            "independent_mean", "independent_empty_rate",
        ],
    )

    nb64_joint = json.loads(
        (NB_DATA / "nb_simeck64_joint.json").read_text(encoding="utf-8")
    )

    corr = correlation_analysis()
    correlation_rows = []
    for key in ("r14_runs", "r17_runs"):
        correlation_rows.extend(corr[key])
    correlation_rows.extend([corr["r14_pooled"], corr["r17_pooled"]])
    write_csv(
        TABLES / "correlation_summary.csv",
        correlation_rows,
        [
            "label", "keys", "sample_log2", "total_log2", "mean_abs",
            "mean_unbiased_sq", "sq_se", "ci_low", "ci_high", "rms",
            "rms_ci_low", "rms_ci_high", "p_signal_one_sided",
            "holm_p_two_targets", "claimed_corr", "claimed_sq",
            "claim_above_estimate_se",
        ],
    )

    analysis = {
        "key_recovery_curve": keycurve,
        "nb_scalability": {key: value for key, value in nb.items() if key != "results"},
        "simeck64_scalability": {
            key: value for key, value in nb64.items() if key != "results"
        },
        "simeck64_joint_checks": nb64_joint,
        "correlation": clean_for_json(corr),
    }
    (TABLES / "analysis_summary.json").write_text(
        json.dumps(analysis, indent=2, ensure_ascii=False), encoding="utf-8"
    )

    plot_keycurve(keycurve)
    plot_nb_scalability(nb["summary"])
    plot_correlation(corr)

    print(json.dumps(clean_for_json(corr), indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
