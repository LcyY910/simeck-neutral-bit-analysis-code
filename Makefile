PYTHON ?= python3

.PHONY: setup smoke analyze full checksums verify

setup:
	$(PYTHON) -m venv .venv
	.venv/bin/python -m pip install --upgrade pip
	.venv/bin/python -m pip install -r requirements.txt

smoke:
	bash scripts/run_smoke_tests.sh

analyze:
	$(PYTHON) scripts/analyze_results.py

full:
	bash scripts/run_paper_experiments.sh

checksums:
	$(PYTHON) scripts/checksums.py

verify:
	$(PYTHON) scripts/checksums.py --verify
