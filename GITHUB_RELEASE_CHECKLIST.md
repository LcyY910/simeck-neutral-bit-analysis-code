# GitHub release checklist

1. Create a public repository named `simeck-neutral-bit-analysis`.
2. Upload the contents of this directory, including files beginning with a dot.
3. Confirm that `.venv/`, `build/`, `smoke_outputs/`, and
   `reproduced_outputs/` are not committed.
4. Use a short repository description such as: "Reproducibility package for
   automated neutral-bit and differential-linear analysis of Simeck."
5. Add the topics `cryptography`, `simeck`, `differential-linear`,
   `formal-verification`, and `reproducible-research`.
6. Confirm that `README.md`, `LICENSE`, `CITATION.cff`,
   `THIRD_PARTY_NOTICES.md`, and `SHA256SUMS.txt` are visible.
7. Create a GitHub release tagged `v1.0.0` and attach the prepared release zip.
8. Connect the repository to Zenodo, archive release `v1.0.0`, and record the
   assigned DOI.
9. Add the DOI badge and DOI link to `README.md` after Zenodo assigns them.
10. Use the permanent release URL and Zenodo DOI in the manuscript's Data
    Availability and Code Availability statements.
