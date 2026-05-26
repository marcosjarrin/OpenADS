# Build a Harbour app against OpenADS with `hbmk2` (`.hbp`)

This is a turnkey **hbmk2** project — drop your `.prg` next to
`openads_demo.hbp`, point `OPENADS_LIB` at OpenADS' build output,
run `hbmk2`. The produced `.exe` loads OpenADS' `ace64.dll` /
`ace32.dll` instead of any SAP-shipped copy and drives DBF / CDX
tables through Harbour's stock `contrib/rddads` RDD.

> Reported by a user on the FiveTech forum: "alguna alma caritativa
> que proporcione un archivo de compilación `.hbp` para crear un
> programa con OpenADS — todos mis intentos han fracasado". This
> directory is that template.

## Files

| File                       | Role |
|----------------------------|------|
| `openads_demo.hbp`         | hbmk2 build script — x64 (links `rddads` + `ace64`). |
| `openads_demo_x86.hbp`     | Same, 32-bit (`ace32`). |
| `openads_demo.prg`         | Minimal console app: `AdsConnect` → `DbCreate` → `INDEX ON UPPER(NAME)` → `dbSeek`. |
| `build.cmd`                | Windows wrapper — sets `OPENADS_LIB`, puts `ace64.dll` on PATH, invokes `hbmk2`. |
| `build.sh`                 | POSIX counterpart for Linux / macOS. |

## Prerequisites

- **Harbour 3.2** with `contrib/rddads` built for your toolchain
  (e.g. `c:\harbour\lib\win\msvc64\rddads.lib`). Building Harbour
  itself is out of scope here — the standard `make install` does it.
- A **C compiler** — MSVC x64 (Developer Command Prompt) for the
  default `-comp=msvc64`. The `.hbp` comment block lists the other
  drivers `hbmk2` supports.
- A **built OpenADS** drop — typically `build/default/src/Release/`
  on Windows, holding `ace64.dll` + `ace64.lib`. **Use the
  OpenADS-produced DLL, not SAP's.** Two ways to obtain it:
  - `cmake --preset default && cmake --build build/default --config Release`
    from the repo root, or
  - download the latest release ZIP from
    <https://github.com/FiveTechSoft/OpenADS/releases> and unzip it.

## Build & run

### Windows (MSVC x64)

```cmd
:: 1. Open a Visual Studio x64 Developer Command Prompt
:: 2. From this directory:
build.cmd "C:\OpenADS\build\default\src\Release"

:: 3. Run it
openads_demo.exe
```

Equivalent without the wrapper:

```cmd
set OPENADS_LIB=C:\OpenADS\build\default\src\Release
set PATH=C:\harbour\bin\win\msvc64;%OPENADS_LIB%;%PATH%
hbmk2 openads_demo.hbp
copy /y "%OPENADS_LIB%\ace64.dll" .
openads_demo.exe
```

### Windows (x86)

```cmd
set OPENADS_LIB=C:\OpenADS\build\msvc-x86\src\Release
set PATH=C:\harbour\bin\win\msvc;%OPENADS_LIB%;%PATH%
hbmk2 openads_demo_x86.hbp
copy /y "%OPENADS_LIB%\ace32.dll" .
openads_demo.exe
```

### Linux / macOS

```sh
./build.sh /path/to/OpenADS/build/default/src
LD_LIBRARY_PATH=/path/to/OpenADS/build/default/src:$LD_LIBRARY_PATH \
    ./openads_demo
```

## Expected output

```
OpenADS hbmk2 demo
ACE DLL reports: 0.0a

Rows after append: 3

Walk via UPPER_NAME (compound CDX):
  rec 1 name=[alice] age=30
  rec 2 name=[BOB] age=25
  rec 3 name=[delta] age=99

Seek 'DELTA' (upper): Found rec 3
Done.
```

If `AdsVersion()` returns something like `12.0` or `11.10` you are
loading **SAP's** ace DLL, not OpenADS'. Check `where ace64.dll`
(Windows) / `ldd openads_demo` (Linux) and reorder PATH /
LD_LIBRARY_PATH.

## Typical errors

| Symptom | Likely cause |
|---|---|
| `unresolved external symbol AdsConnect60` (or any `Ads*`) | `OPENADS_LIB` not set, or wrong toolchain — `ace64.lib` is MSVC; for bcc64 / MinGW you need a matching import lib. |
| `Error linking, lib 'rddads' not found` | `contrib/rddads` not built for the `-comp=…` you selected. Rebuild Harbour's contrib for that toolchain. |
| Runtime `ace64.dll not found` | DLL not next to the exe and not on `PATH`. |
| `BASE/1003 rddads/<num>` at startup | Missing `REQUEST ADS, ADSCDX, ADSNTX` (rddads not registered) — `#include "ads.ch"` alone is not enough. |
| Strings show truncated in a TBrowse / xBrowse | Fixed in v1.0.0-rc27 (`AdsGetField` now pads CHAR to declared width). Use rc27 or newer. |

## FiveWin / GUI apps

`hbmk2` works fine for console / `gtwin` apps. For FiveWin (FWH)
GUI you need a fuller link line — see
[`examples/fivewin/`](../fivewin/) for `build_msvc64.cmd` /
`build64.cmd` that mirror FWH's stock build scripts with the two
extra link entries (`rddads.lib` + `ace64.lib`).

## See also

- `tests/smoke/harbour/` — headless smoke runs used by CI that
  exercise the same RDD path against OpenADS.
- [Getting started — hbmk2](../../docs/en/getting-started.md) on
  the docs site.
