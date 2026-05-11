# FiveWin + OpenADS example

A FiveWin (FWH) demo that opens an Advantage table through Harbour's
`rddads` and browses it in an `xbrowse` — but linked against OpenADS'
`ace64.dll` / `ace32.dll` instead of a SAP-shipped one.

> **Not in CI.** FiveWin is a commercial GUI library and the demo opens
> a window; it can't run on a headless build agent. This is a manual
> "drop-in and run" showcase.

## Prerequisites

- **FiveWin (FWH)** — commercial, from FiveTech. Not included here.
- **Harbour 3.2** with `contrib/rddads` (the static `rddads.lib`).
- A built OpenADS `ace64.dll` + `ace64.lib` (or x86 equivalents),
  e.g. `build/default/src/Release/`.

## Build & run (sketch)

```cmd
:: OpenADS DLL dir first on PATH so its ace64.dll wins
set PATH=C:\OpenADS\build\default\src\Release;%PATH%

:: link rddads + OpenADS' ace64.lib + FWH, as you would any FWH app
hbmk2 -comp=msvc64 ^
      -i"%FWDIR%\include" -L"%FWDIR%\lib" ^
      -i"C:\harbour\contrib\rddads" -lrddads ^
      -L"C:\OpenADS\build\default\src\Release" -lace64 ^
      fwh.hbc demo.prg

demo.exe
```

`demo.prg` (to be added) will: `AdsConnect`/`RddSetDefault("ADSCDX")`,
`USE customer VIA "ADSCDX"`, build/refresh an index, and show an
`xbrowse` over it. The point is purely visual confirmation that a real
FWH GUI app runs unchanged against OpenADS' DLL.

Rules: first-party source only — do **not** check in FWH, Harbour, or
SAP `ace*.dll`/`ace*.lib` here.
