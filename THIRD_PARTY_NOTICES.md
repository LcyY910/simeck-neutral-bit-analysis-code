# Third-party notices

The correlation validation and endpoint configurations build on the public
artifact associated with:

Hosein Hadipour, Patrick Derbez, and Maria Eichlseder, "Revisiting
Differential-Linear Attacks via a Boomerang Perspective with Application to
AES, Ascon, CLEFIA, SKINNY, PRESENT, KNOT, TWINE, WARP, LBlock, Simeck, and
SERPENT," CRYPTO 2024, pp. 38-72.
https://doi.org/10.1007/978-3-031-68385-5_2

The referenced artifact files were distributed under the MIT License. The
implementations in this repository were reorganized and extended to use fixed
seeds, signed sums, control masks, and an unbiased squared-correlation
estimator. This repository does not redistribute Gurobi or other proprietary
solver components.

The implementation-vector values follow:

Gangqiang Yang, Bo Zhu, Valentin Suder, Mark D. Aagaard, and Guang Gong, "The
Simeck Family of Lightweight Block Ciphers," CHES 2015, pp. 307-329.
https://doi.org/10.1007/978-3-662-48324-4_16

Z3 is installed from the `z3-solver` Python package and is distributed under
the MIT License. NumPy, SciPy, and Matplotlib retain their respective upstream
licences.
