# OpenADS smoke tests — third-party RDD harnesses

These harnesses drive OpenADS' ACE-compatible DLL (`ace64.dll` /
`ace32.dll`) *through a real client RDD*, the way an actual Harbour or
X# application would — end-to-end, not through OpenADS' own C++ test
harness. They complement (don't replace) the doctest suite in
`tests/unit/`.

| Subdir | Toolchain needed | What it covers |
|--------|------------------|----------------|
| `harbour/` | Harbour 3.2 + `contrib/rddads` + MSVC (`vcvars64`) | Read/write/index/multi-tag/transactions/memo through `rddads.lib` linked against OpenADS' `ace64.lib` + `ace64.dll`. |
| `xsharp/` | X# compiler + runtime (xsharp.eu) | `DbCreate` / `OrdCreate` / `DbAppend` / `DbSeek` / navigation through X#'s `AXDBFCDX` RDD, which P/Invokes `ace32.dll` / `ace64.dll`. |

## Ground rules

- **Opt-in.** None of these run in the default `ctest` pass — they need
  toolchains that aren't guaranteed to be present. Build/run them
  explicitly (see each subdir's `README.md`).
- **Only first-party code here.** The `.prg` / project files are
  written for this repo. We do **not** vendor Harbour, X#, or their
  runtimes/libraries — install those separately. And never check in
  SAP-shipped binaries (`ace*.dll` / `ace*.lib` from an Advantage
  install): the point is to test *OpenADS'* DLL.
- Each harness expects OpenADS' freshly-built `ace64.dll` to be ahead
  of any other `ace64.dll` on `PATH`.

GUI showcases (FiveWin apps, X# WinForms) live under `examples/`, not
here — they can't run headless in CI.
