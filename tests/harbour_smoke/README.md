# Harbour rddads smoke test (M8.1)

This directory contains a minimal Harbour `.prg` that links against
Harbour's `contrib/rddads` static library and the OpenADS-shipped
`ace64.dll` / `ace64.lib`. The point is to validate **end-to-end** that
every ACE entry point Harbour expects is resolvable from OpenADS.

## What we proved

`c:\harbour\lib\win\msvc64\rddads.lib` references **225 distinct `Ads*`
entry points**. After M8.1, OpenADS's `ace64.dll` exports all of them:
80 real implementations (M0-M7) + 146 stubs that return
`AE_FUNCTION_NOT_AVAILABLE` (5004) so the link succeeds.

Linking `smoke.prg` against `rddads.lib` + `ace64.lib` produces a clean
resolution of every `HB_FUN_ADSVERSION`/`AdsGetVersion`/etc. symbol
chain.

## End-to-end validation result (M8.2)

`smoke.exe` builds **and runs**. Output:

    OpenADS smoke test
    rddads version probe...
    ACE DLL reports: 0.0a

The flow exercised:

1. Harbour PP compiles `smoke.prg` and resolves `AdsVersion()` to the
   `HB_FUN_ADSVERSION` wrapper inside `rddads.lib`.
2. `rddads.lib`'s wrapper calls `AdsGetVersion(...)` — a true import
   resolved through the OpenADS-shipped `ace64.lib` import library.
3. At runtime, `ace64.dll` (loaded from `c:\harbour\bin\win\msvc64\`)
   answers the call and returns OpenADS' version string.

### Legacy CRT shims

Harbour's prebuilt `msvc64` libs were compiled against MSVC 2013-era
CRT entry points that disappeared in the VS2015 UCRT split. To keep
Harbour usable without rebuilding Harbour itself, `ace64.dll` exports
shims for the missing symbols (`abi/legacy_crt_shims.cpp`):

| Legacy symbol | Replacement                                 |
|---------------|---------------------------------------------|
| `_dclass`     | `std::fpclassify`                           |
| `_dsign`      | `std::signbit`                              |
| `_wfsopen`    | UCRT `_wfsopen`                             |
| `_getch`      | UCRT `_getch` (used by gtstd)               |
| `_kbhit`      | UCRT `_kbhit` (used by gtstd)               |
| `_eof`        | UCRT `_eof`   (used by gtstd)               |

`openads_ace.def` aliases these legacy names onto OpenADS-prefixed
implementations so the DLL exports the names hbcommon / gtstd expect.

### Drop-in install

`run_build.bat` (and any future automation) currently overwrites the
two Harbour-shipped artefacts:

    c:\harbour\lib\win\msvc64\ace64.lib   (import lib for the DLL)
    c:\harbour\bin\win\msvc64\ace64.dll   (loaded at runtime)

with the OpenADS-built versions. After that, `hbmk2 -comp=msvc64
-gtstd -lrddads -lace64 smoke.prg` produces a runnable executable.

## Running

```cmd
:: From the OpenADS root:
cmake --build build\default --config Release
cd tests\harbour_smoke
run_build.bat
```

`run_build.bat`:
1. Calls `vcvars64.bat` to bring the MSVC link toolchain into PATH.
2. Puts `c:\harbour\bin\win\msvc64` and the OpenADS Release output on
   PATH so `hbmk2` and `ace64.dll` are found.
3. Invokes `hbmk2 -comp=msvc64 -lrddads -L<openads-out> -lace64`.
