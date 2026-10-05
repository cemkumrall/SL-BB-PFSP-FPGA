# Benchmark data

The experiments use the 120 Taillard permutation flow-shop scheduling problem (PFSP) instances (ta001-ta120), cited in the manuscript as:

E. Taillard, "Benchmarks for basic scheduling problems," European Journal of Operational Research, 64(2), 278-285, 1993. https://doi.org/10.1016/0377-2217(93)90182-M

To keep this repository focused and avoid redistributing third-party benchmark files, the raw Taillard instance files are not included. `taillard_manifest.csv` records the instance dimensions and metadata used by the study.

For local reproduction, place the 120 instance files under `data/taillard/` using names `ta001.txt` ... `ta120.txt`.
