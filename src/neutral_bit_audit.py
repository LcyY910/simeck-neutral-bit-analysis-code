from __future__ import annotations

import json
import time

from z3 import BitVec, BitVecVal, Extract, Or, RotateLeft, Solver, sat


def round_function(left, right, key, width):
    mask = (1 << width) - 1
    new_left = right ^ (left & RotateLeft(left, 5)) ^ RotateLeft(left, 1) ^ key
    return new_left & mask, left


def encrypt(left, right, keys, width):
    for key in keys:
        left, right = round_function(left, right, key, width)
    return left, right


def single_bit_status(name, width, diff_l, diff_r, bit):
    p_l = BitVec(f"{name}_b{bit}_pl", width)
    p_r = BitVec(f"{name}_b{bit}_pr", width)
    keys = [BitVec(f"{name}_b{bit}_k{i}", width) for i in range(3)]
    noise = 1 << bit
    noise_l = (noise >> width) & ((1 << width) - 1)
    noise_r = noise & ((1 << width) - 1)

    a_l, a_r = encrypt(p_l, p_r, keys, width)
    b_l, b_r = encrypt(p_l ^ diff_l, p_r ^ diff_r, keys, width)
    c_l, c_r = encrypt(p_l ^ noise_l, p_r ^ noise_r, keys, width)
    d_l, d_r = encrypt(
        p_l ^ diff_l ^ noise_l,
        p_r ^ diff_r ^ noise_r,
        keys,
        width,
    )
    solver = Solver()
    solver.add(Or((a_l ^ b_l) != (c_l ^ d_l), (a_r ^ b_r) != (c_r ^ d_r)))
    status = solver.check()
    if status != sat:
        return True, None
    model = solver.model()
    example = {
        "p_l": model.eval(p_l, model_completion=True).as_long(),
        "p_r": model.eval(p_r, model_completion=True).as_long(),
        **{
            f"k{i}": model.eval(key, model_completion=True).as_long()
            for i, key in enumerate(keys)
        },
    }
    return False, example


def joint_status(name, width, diff_l, diff_r, allowed_mask):
    full_width = 2 * width
    p_l = BitVec(f"{name}_joint_pl", width)
    p_r = BitVec(f"{name}_joint_pr", width)
    keys = [BitVec(f"{name}_joint_k{i}", width) for i in range(3)]
    noise = BitVec(f"{name}_joint_noise", full_width)
    allowed = BitVecVal(allowed_mask, full_width)
    noise_l = Extract(full_width - 1, width, noise)
    noise_r = Extract(width - 1, 0, noise)

    a_l, a_r = encrypt(p_l, p_r, keys, width)
    b_l, b_r = encrypt(p_l ^ diff_l, p_r ^ diff_r, keys, width)
    c_l, c_r = encrypt(p_l ^ noise_l, p_r ^ noise_r, keys, width)
    d_l, d_r = encrypt(
        p_l ^ diff_l ^ noise_l,
        p_r ^ diff_r ^ noise_r,
        keys,
        width,
    )

    solver = Solver()
    solver.add(noise != 0)
    solver.add((noise & ~allowed) == 0)
    solver.add(Or((a_l ^ b_l) != (c_l ^ d_l), (a_r ^ b_r) != (c_r ^ d_r)))
    status = solver.check()
    if status != sat:
        return "unsat", None
    model = solver.model()
    example = {
        "p_l": model.eval(p_l, model_completion=True).as_long(),
        "p_r": model.eval(p_r, model_completion=True).as_long(),
        "noise": model.eval(noise, model_completion=True).as_long(),
        **{
            f"k{i}": model.eval(key, model_completion=True).as_long()
            for i, key in enumerate(keys)
        },
    }
    return "sat", example


def audit_configuration(name, width, diff_l, diff_r, expected_mask):
    deterministic = []
    invalid = []
    invalid_examples = {}
    for bit in range(2 * width):
        valid, example = single_bit_status(name, width, diff_l, diff_r, bit)
        if valid:
            deterministic.append(bit)
        else:
            invalid.append(bit)
            invalid_examples[str(bit)] = example

    maximal_mask = sum(1 << bit for bit in deterministic)
    joint, joint_example = joint_status(
        f"{name}_maximal", width, diff_l, diff_r, maximal_mask
    )
    result = {
        "name": name,
        "width": width,
        "input_difference": [diff_l, diff_r],
        "deterministic_bits": deterministic,
        "non_deterministic_bits": invalid,
        "maximal_mask": maximal_mask,
        "expected_mask": expected_mask,
        "mask_matches_expected": maximal_mask == expected_mask,
        "joint_counterexample_status": joint,
        "joint_counterexample": joint_example,
        "maximality_reason": (
            "joint mask is UNSAT and every excluded bit has a single-bit SAT counterexample"
        ),
        "excluded_bit_counterexamples": invalid_examples,
    }
    return result


def main():
    started = time.time()
    results = [
        audit_configuration(
            "simeck32_practical", 16, 0x0000, 0x2000, 0x0400AC21
        ),
        audit_configuration(
            "simeck32_theoretical", 16, 0x0800, 0x1000, 0x00006108
        ),
        audit_configuration(
            "simeck48_theoretical", 24, 0x000100, 0x000200, 0x108421318C63
        ),
    ]
    claimed_status, claimed_example = joint_status(
        "simeck32_claimed16", 16, 0x0000, 0x2000, 0x1C61BCE3
    )
    output = {
        "solver": "Z3",
        "rounds": 3,
        "round_key_model": "three arbitrary words; reachable as the first three Simeck master-key words",
        "claimed_practical_mask": {
            "mask": 0x1C61BCE3,
            "counterexample_status": claimed_status,
            "counterexample": claimed_example,
        },
        "configurations": results,
        "elapsed_seconds": time.time() - started,
    }
    print(json.dumps(output, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
