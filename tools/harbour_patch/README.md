# Harbour rddads compatibility patches

Two small patches against `harbour/contrib/rddads/` are required so the
standard rddads contrib links cleanly against `ace64.dll`/`ace.h` from
this repo and exposes the same record-state semantics as Harbour's
clean-room `dbfcdx` (which the `tests/rddtest/rddtst.prg` baseline was
recorded against).

Apply by hand or via `git apply` from a Harbour source tree:

```sh
cd /path/to/harbour
git apply /path/to/openads/tools/harbour_patch/rddads-compat.patch
```

Then rebuild rddads:

```sh
HB_WITH_ADS=/path/to/openads-sdk \
    bin/win/msvc64/hbmk2 contrib/rddads/rddads.hbp -comp=msvc64
```

## Patch contents

### `contrib/rddads/rddads.h`

Adds an inline `ADSFIELD( UNSIGNED16 n )` helper. Harbour's rddads
sources (`ads1.c`, `adsfunc.c`) reference `ADSFIELD( n )` to fetch the
n-th field's name as `UNSIGNED8*` from the current workarea, but no
upstream header in the Harbour build defines it. The shim looks up
the workarea via `hb_rddGetCurrentWorkAreaPointer()` and reads the
field's symbol name through `hb_dynsymName`. (Without this patch
`AdsGetMemoDataType()` and ~30 other `HB_FUNC` wrappers fail to link.)

### `contrib/rddads/ads1.c`

Aligns the not-positioned `hb_adsSkip()` branch with `dbf1.c`'s
`hb_dbfSkip()`: setting one direction flag clears the opposite,
distinguishing real Bof / Eof from the Limbo state of a
freshly-opened empty table. Without this patch the rddtst sequence
`USE empty → DBSKIP(±1) → DBSKIP(0)` keeps both flags set instead of
cleanly transitioning to a single-flag state.
