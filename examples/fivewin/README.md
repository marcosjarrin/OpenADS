# FiveWin + xBrowse over ADS — running on OpenADS

`xbrowse_ads.prg` is a FiveWin (FWH) app that opens an Advantage table
through Harbour's `rddads` contrib (RDD `ADSCDX`) and shows it in an
`xBrowse` — but linked against **OpenADS' `ace64.dll`** instead of a
SAP-shipped ACE. It stages a small `customer.dbf` in a temp dir,
builds NAME/CITY orders, and browses it; `xbrowse_ads.exe /auto`
walks the browse (GoBottom / GoTop / re-sort via the RDD) and closes
itself, so it doubles as a quick "the FWH→rddads→OpenADS stack works"
smoke run.

> **Not in CI.** FWH is a commercial GUI library and the demo opens a
> window — manual showcase. (`tests/smoke/harbour/` is the headless,
> CI-friendly equivalent of the same `rddads`→OpenADS path.)

## Prerequisites

- **FiveWin (FWH)** — commercial, from FiveTech. `%FWDIR%` (defaults to
  `c:\fwteam` in `build64.cmd`). Not vendored here.
- **Harbour 3.2** with `contrib/rddads` built for bcc64
  (`%HBDIR%\lib\win\bcc64\librddads.a` + `%HBDIR%\contrib\rddads\libace64.a`).
- **Embarcadero bcc64** (`c:\bcc7764`) — FWH's x64 compiler.
- A built OpenADS `ace64.dll` + `ace64.lib` (e.g.
  `build/default/src/Release/`). **It must be the OpenADS DLL, not
  SAP's** — copy it next to the produced `.exe` or put its directory
  first on `PATH`.

## Build & run

```cmd
:: from this directory — MSVC 64-bit (the verified path)
build_msvc64.cmd  C:\OpenADS\build\default\src\Release
xbrowse_ads.exe          :: interactive window
xbrowse_ads.exe /auto    :: self-closing smoke run; exit 0 on success
```

`build_msvc64.cmd` mirrors FWH's `samples\build_new.bat` `:HM64` path
with two extra link entries — `rddads.lib` (Harbour's ADS RDD) and
OpenADS' `ace64.lib` — and copies OpenADS' `ace64.dll` next to the exe.
Verified: links cleanly and `xbrowse_ads.exe /auto` exits 0 (the
xBrowse drives `customer.dbf` through Harbour's `rddads` → OpenADS'
`ace64.dll`). `build64.cmd` is the equivalent for FWH's bcc64
toolchain (untested here — needs `librddads.a` for bcc64). Adjust the
`FWDIR` / `HBDIR` paths if your install differs from `c:\fwteam` /
`c:\harbour`.

## What it exercises

`USE customer VIA "ADSCDX"` → `INDEX ON … TAG …` (compound CDX through
`AdsCreateIndex`/`AdsCreateIndex90` over OpenADS) → `xBrowse` over the
alias (reads field structure, navigates, re-sorts via `OrdSetFocus`) →
`DbCloseArea`. The same ACE entry points a real FWH+rddads app hits,
served by OpenADS' DLL.

Rules: first-party source only — do **not** check in FWH, Harbour,
bcc64, or SAP `ace*.dll`/`ace*.lib` here.
