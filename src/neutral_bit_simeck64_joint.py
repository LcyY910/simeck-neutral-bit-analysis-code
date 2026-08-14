from __future__ import annotations

import json
import time

from neutral_bit_audit import joint_status, single_bit_status


def audit(name: str, diff_l: int, diff_r: int) -> dict:
    width = 32
    started = time.perf_counter()
    bits = []
    for bit in range(2 * width):
        valid, _ = single_bit_status(name, width, diff_l, diff_r, bit)
        if valid:
            bits.append(bit)
    mask = sum(1 << bit for bit in bits)
    joint, example = joint_status(name + "_joint", width, diff_l, diff_r, mask)
    return {
        "name": name,
        "cipher": "Simeck-64",
        "rounds": 3,
        "input_difference": [diff_l, diff_r],
        "deterministic_bits": bits,
        "mask": mask,
        "joint_status": joint,
        "joint_counterexample": example,
        "elapsed_seconds": time.perf_counter() - started,
    }


def main() -> None:
    started = time.perf_counter()
    results = [
        audit("simeck64_weight1_rbit4", 0, 0x00000010),
        audit("simeck64_weight2_rbits0_1", 0, 0x00000003),
        audit("simeck64_weight4_rbits0_3", 0, 0x0000000F),
    ]
    print(json.dumps({"method": "Z3 universal joint semantic check", "results": results,
                      "elapsed_seconds": time.perf_counter() - started}, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
