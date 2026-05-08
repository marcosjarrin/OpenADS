# openads_bench

Synthetic SQL workload timer for cross-platform comparison of the
OpenADS engine. Useful for spotting regressions across releases or
between platforms (Windows / macOS / Linux).

## Where the data comes from

`openads_bench` **does not** read a pre-existing fixture, and it
**does not** depend on any external dataset. Every run:

1. Creates a fresh temp directory under the OS temp path.
2. Builds a synthetic three-column DBF in-place (no DLL calls,
   raw bytes written to disk so the engine sees a "real" file
   produced by an external tool):

   | Column | Type    | Contents                              |
   |--------|---------|---------------------------------------|
   | ID     | N(8,0)  | sequential 1 .. `--rows`              |
   | TAG    | C(4)    | cyclic 4-char tags ('AAAA', 'BBBB', …) |
   | AMT    | N(8,2)  | pseudo-random 0..1000 (deterministic)  |

3. Opens that DBF through the public `Ads*` ABI, runs each
   workload `--repeats` times, and reports the median runtime.

4. Removes the temp directory on exit.

The synthetic dataset is reproducible (the random seed is fixed)
so two runs at the same `--rows` value produce identical content.
Bench numbers are therefore directly comparable across hosts.

## Usage

```text
openads_bench [--rows N] [--repeats R] [--csv]

  --rows     row count for the synthetic DBF (default 100 000)
  --repeats  per-workload repeats (default 5; reports median)
  --csv      emit CSV header + one row per workload
```

Output columns:

```text
workload, rows, run_ms_min, run_ms_med, run_ms_max, note
```

## Why a synthetic dataset?

- **No licensing issues.** Nothing copyrighted ships with OpenADS;
  the bench dataset is generated each run.
- **Deterministic.** Same `--rows` yields the same bytes.
- **Cross-platform comparable.** No network / no filesystem
  layout differences influence the numbers — only the engine.
- **No external dependency.** A user can run the bench on a fresh
  checkout immediately after `cmake --build`; nothing to download
  or unpack.
