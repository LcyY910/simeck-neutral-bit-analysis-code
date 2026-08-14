from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CHECKSUM_FILE = ROOT / "SHA256SUMS.txt"
EXCLUDED_DIRS = {".git", ".venv", "build", "smoke_outputs", "reproduced_outputs", "__pycache__"}


def repository_files():
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or path == CHECKSUM_FILE:
            continue
        if any(part in EXCLUDED_DIRS for part in path.relative_to(ROOT).parts):
            continue
        yield path


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def generate() -> None:
    lines = [f"{digest(path)}  {path.relative_to(ROOT).as_posix()}" for path in repository_files()]
    CHECKSUM_FILE.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {len(lines)} checksums to {CHECKSUM_FILE.name}")


def verify() -> None:
    failures = []
    for line in CHECKSUM_FILE.read_text(encoding="utf-8").splitlines():
        expected, relative = line.split("  ", 1)
        path = ROOT / relative
        if not path.is_file() or digest(path) != expected:
            failures.append(relative)
    if failures:
        raise SystemExit("Checksum verification failed: " + ", ".join(failures))
    print("All archived checksums verified.")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", action="store_true")
    args = parser.parse_args()
    verify() if args.verify else generate()


if __name__ == "__main__":
    main()
