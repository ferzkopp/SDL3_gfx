# Fuzz regressions

Inputs found by the fuzzers that still crash or time out. They are not replayed by CTest; when the
finding is fixed, move the input into `fuzz/corpus/<target>/` so it is replayed in every build.

| File | Finding | Notes |
|---|---|---|

Fixed and moved to the corpus: `rotozoom/regression-roto002` (ROTO-002), `primitives/regression-perf001`
(PERF-001, `filledPieRGBA` with radius 25557 took > 10 s, now 0.08 s).