# Changelog

All notable changes to OpenADS are recorded here. The project follows
[Semantic Versioning](https://semver.org/) once 1.0 ships; until then
0.x.y releases may break the C ABI between minor versions to track the
real ACE SDK.

## 0.2.0 — 2026-05-04

The 0.2.0 release closes the entire 226-symbol Harbour-reachable
`Ads*` ABI surface — every export resolves to either a real
implementation or a documented local-mode silent-success. No exports
hard-fail with `AE_FUNCTION_NOT_AVAILABLE` at the function level any
more. The release also relicensed the project from MIT to Apache
License 2.0 and added a clean-room provenance / non-commercial /
no-warranty disclaimer block + NOTICE file.

### Highlights

- **Compound CDX expression evaluator.** `UPPER`, `LOWER`,
  `LTRIM` / `RTRIM` / `ALLTRIM`, `STR(n[,len[,dec]])`, `DTOS(date)`,
  `SUBSTR(s,start[,len])`, and string concatenation with `+`. UPPER /
  LOWER / SUBSTR walk UTF-8 codepoints (ASCII + Latin-1 supplement
  case map, `ÿ↔Ÿ` pair); the Latin-1 case mapping table closes the
  M9.17 `*W` Unicode surface.
- **Real CRUD for tables and indexes.** `AdsCreateTable` parses the
  rddads `NAME,Type,Len,Dec;…` field-def syntax; `AdsCreateIndex61` /
  `AdsCreateIndex` build CDX or NTX bags compatible with FoxPro and
  Clipper layouts. `AdsZapTable` / `AdsPackTable` / `AdsReindex`
  match the Clipper bound-index lifecycle.
- **Multi-file index binding.** Multiple `.ntx` files (or multiple
  pre-built `.cdx` bags) coexist on a single Table; same-path reopen
  refreshes; `AdsCloseIndex` drops the closed view without disturbing
  the active order.
- **Transactions + savepoints + WAL recovery.** `AdsBeginTransaction`
  / `AdsCommitTransaction` / `AdsRollbackTransaction` /
  `AdsCreateSavepoint` / `AdsRollbackTransaction80(savepoint)`. Mid-
  tx crash + reopen replays the WAL and writes back before-images
  for orphan transactions.
- **Memo (DBT / FPT) read + write + binary type.** Text memos
  round-trip through DBT and FPT; `AdsGetBinary` /
  `AdsGetBinaryLength` / `AdsSetBinary` carry binary blobs through
  FPT block-type tags (Text / Picture / Object); chunked
  `AdsSetBinary` writes reassemble through a per-(table, field)
  accumulator.
- **Locking with retry policy.** `AdsLockTable` / `AdsLockRecord`
  use non-blocking byte-range acquires (`LockFileEx
  LOCKFILE_FAIL_IMMEDIATELY` on Windows, `fcntl F_SETLK` on POSIX);
  `AdsSetLockCycle` / `AdsSetLockRetryCount` configure the retry
  budget.
- **Full-text search.** `AdsCreateFTSIndex` writes a clean-room
  `# OpenADS FTS v0` text file per table; `AdsFTSSearch` and the
  SQL `CONTAINS(<col>, '<query>')` predicate intersect per-token
  recno lists with AND semantics.
- **Server / dictionary surface.** `AdsMg*` (15 calls) report local-
  mode "everything quiescent" responses; `AdsDD*` (14 advanced-DD
  calls) accept silently and zero-fill property getters. Real
  persistence in the OpenADS DD format lands with 0.3.x.
- **Schema evolution.** `AdsRestructureTable` ADD-fields path
  rewrites the DBF with extended schema and preserves every
  record's original-field bytes; DELETE / CHANGE arguments still
  surface AE_FUNCTION_NOT_AVAILABLE pending VFP / ADT structural
  extensions.
- **Misc.** Real `AdsGetServerName` / `AdsGetServerTime`,
  `AdsGetLongLong`, `AdsSetFieldRaw`, `AdsVerifySQL`,
  `AdsFailedTransactionRecovery`, `AdsGetAllLocks`, `AdsSkipUnique`,
  `AdsFindFirstTable` / `AdsFindNextTable` / `AdsFindClose`,
  `AdsCopyTable` / `AdsCopyTableContents` / `AdsConvertTable`,
  `AdsAddCustomKey` / `AdsDeleteCustomKey`.

### Project posture

- License: relicensed **MIT → Apache License 2.0** (`LICENSE` +
  `NOTICE`).
- Independence + non-commercial purpose + clean-room provenance +
  no-warranty + downstream responsibility — block added to the
  README and mirrored to the NOTICE file (Apache 4(d) preservation).
- Tests: **214** doctest cases, **3865** assertions, all green on
  Windows / MSVC Release. CI matrix builds Windows + Linux + macOS
  cleanly through `.github/workflows/ci.yml`.

### Milestones

| Tag | Milestone |
|-----|-----------|
| `m9.1`   | Compound CDX expression evaluator |
| `m9.2`   | Stub batch reorganised into real / no-op / missing |
| `m9.3`   | Compound expressions validated through Harbour |
| `m9.4`   | `AdsGotoRecord` + table / file metadata |
| `m9.5`   | `AdsCreateTable` |
| `m9.6`   | `AdsRefreshRecord` + `AdsExtractKey` |
| `m9.7`   | `AdsCreateIndex61` with compound expression |
| `m9.8`   | `AdsZapTable` + `AdsPackTable` |
| `m9.9`   | `AdsReindex` |
| `m9.10`  | NTX multi-level B+tree split |
| `m9.11`  | `AdsCopyTable` / `AdsCopyTableContents` / `AdsConvertTable` |
| `m9.12`  | `AdsFindFirstTable` / `AdsFindNextTable` / `AdsFindClose` |
| `m9.13`  | Binary memo (`AdsGetBinary` / `AdsSetBinary` / `AdsGetBinaryLength`) |
| `m9.14`  | NTX multi-tag binding |
| `m9.15`  | Real `AdsGetServerName` / `AdsGetServerTime` + binding-leak fix |
| `m9.16`  | Chunked `AdsSetBinary` |
| `m9.17`  | Unicode `*W` variants |
| `m9.18`  | Lock retry / cycle policy |
| `m9.19`  | `AdsCreateFTSIndex` |
| `m9.20`  | `AdsAddCustomKey` / `AdsDeleteCustomKey` |
| `m9.21`  | FTS search side (`AdsFTSSearch` + SQL `CONTAINS`) |
| `m9.22`  | UTF-8 codepoint-aware index-expression evaluator |
| `m9.23`  | Misc MISS fillers (LongLong / FieldRaw / VerifySQL / FailedTxRecovery / GetAllLocks / SkipUnique) |
| `m9.24`  | Local-mode `AdsMg*` surface (15 calls) |
| `m9.25`  | Local-mode `AdsDD*` CRUD surface (14 calls) |
| `m9.26`  | `AdsRestructureTable` (ADD-fields path) |
| `m9.27`  | CI matrix portability |

## 0.1.0 — 2026-05-04

Final 0.1.0. The post-rc1 work below extends the Harbour smoke
beyond the read path covered in 0.1.0-rc1: a real Harbour app now
also drives multi-tag focus swaps, ARIES-style transactions, and
memo M-field round-trips end-to-end through OpenADS' `ace64.dll`.

### M8.9 — Multi-tag CDX + OrdSetFocus

- `AdsOpenIndex` widened to its real 4-arg signature
  `(hTable, pucName, ahIndex[], &pu16ArrayLen)`. Every tag inside a
  compound CDX is opened by name through `CdxIndex::open_named`;
  the first tag's IIndex moves into `Table::set_order` and the rest
  park in their bindings.
- `Table::take_order()` / `Order::release()` surrender the active
  index's `unique_ptr<IIndex>` so a focus swap can park it in the
  previous binding's slot.
- `get_table` and `table_for_index` now call `activate_binding(h)`
  whenever a navigation call arrives with an index handle, so
  rddads' `pArea->hOrdCurrent` swaps drive the Table's active order
  in lockstep.
- `AdsGetIndexHandle` strips trailing whitespace from the caller's
  tag name; `AdsGetIndexName` / `AdsGetIndexExpr` read each
  binding's metadata directly so parked tags report their real name
  even before they become live.
- `AdsGetNumIndexes` returns the per-table binding count.

### M8.10 — Transactions through Harbour

- A real Harbour app drives `AdsBeginTransaction` /
  `AdsRollback` / `AdsCommitTransaction` directly. BEGIN + APPEND +
  ROLLBACK leaves the appended row in the DBF flagged deleted (CDX
  index entries persist by design — `Found()` still reports `T` but
  `Deleted()` is `T`); BEGIN + APPEND + COMMIT persists durably to
  both the DBF and every CDX tag.
- `Table::register_extra_index_view` /
  `Table::unregister_extra_index_view` /
  `Table::clear_extra_index_views` track the parked CDX sub-tags as
  non-owning views; the binding still owns the IIndex lifetime.
- `Table::snapshot_index_keys_()` captures the pre-write key per
  index — active order plus extras — and `sync_all_indexes_(snap)`
  erases each prior `(recno, prev_key)` and inserts the new one in
  lockstep, so a `set_field` on a multi-tag CDX keeps every tag
  consistent (M8.8 only synced the active order).
- `Table::flush()` flushes the active order **and** every extra
  view so a multi-tag commit reaches disk for every tag.

### M8.11 — Memo M-fields (FPT)

- A real Harbour app appends rows whose `FIELD->NOTES` carries a
  short memo (43 bytes) and a longer memo (280 bytes), closes the
  area, reopens, and reads the memos back via the standard Clipper
  RDD surface.
- `make_cdx.exe` now also writes an empty `data.fpt` next to
  `data.cdx` via `FptMemo::create`, so `Connection::open_table` finds
  a memo store to auto-attach when the DBF declares an M field.
- `AdsGetMemoLength` / `AdsGetMemoDataType` / `AdsGetString` are now
  real implementations using `resolve_field_index` (M4 had earlier
  versions that only accepted string field names; rddads passes the
  `ADSFIELD(n)` integer form).
- `AdsCloseTable` flushes the table before releasing the handle so
  non-transactional appends reach disk on `USE` close.
- `ADS_MEMO_TEXT` / `ADS_MEMO_PICTURE` aliases resolve to the
  M8.4-verified `ADS_STRING` (4) and `ADS_IMAGE` (7) values.

## 0.1.0-rc1 — 2026-05-03

First end-to-end validation against Harbour `contrib/rddads`. A real
`.prg` compiled with `hbmk2 -comp=msvc64 -lrddads -lace64` opens a
DBF, walks records, runs `dbSeek`, appends rows, and reopens — every
call lands on OpenADS' `ace64.dll` with no Harbour rebuild.

### M0–M3.10 (engine + drivers)

- 5-layer architecture: ABI shim → Session/Connection → SQL → Engine
  (Table / Index / MemoStore / LockMgr / TxLog) → OS abstraction.
- DBF read/write for C / N / L / D columns; deletion flag; flush.
- CDX driver with compound layout (file header + structure-tag root +
  per-tag CDXTAGHEADER + sub-tag B+tree). Multi-tag-per-file API
  (`add_tag` / `open_named` / `list_tags`). FoxPro-equivalent leaf
  bit-pack (mirrors Harbour `hb_cdxPageLeafInitSpace`). Compound CDX
  closes the last reviewer-flagged compat-breaking item.
- NTX driver with cache-based in-order traversal for multi-level
  trees; leaf-split fix promotes the separator without duplicating it.
- AES-128 / AES-256 ECB primitives (vendored tiny-AES-c, Unlicense),
  validated against FIPS-197 + NIST SP 800-38A.
- DBT + FPT memo round-trip.
- Data Dictionary (`.add`): `TABLE alias=path` text format with alias
  resolution.
- Minimal SQL: `SELECT * FROM <table> [WHERE col op 'lit' [AND ...]]`
  with six comparison operators.

### M4 — Locking

- OS byte-range locking with ranges compatible with original ACE so
  installs can coexist during migration.

### M5 — Transactions

- WAL with CRC-32C records (BEGIN / UPDATE / COMMIT / ABORT).
- In-memory ordered op log with named savepoints (M5.3).
- Group commit (M5.4): each record carries a monotonic LSN;
  `sync_to(lsn)` is the group-commit primitive — first thread to
  observe `last_synced_lsn_ < lsn` issues a single fsync covering
  the high-water mark.
- Idempotent recovery (M5.5) via `openads.lsnmap` sidecar.
  Concurrent recovery passes can never regress the per-record
  watermark.

### M6 — Data Dictionary

- `.add` parser, `Connection::open(<path>.add)` resolves member
  tables through the dictionary on every `AdsOpenTable`.

### M7 — SQL

- Parser + executor for `SELECT *` + multi-clause `WHERE` joined by
  implicit `AND`. Compiles to a `Table::RowPredicate` closure used by
  `AdsExecuteSQLDirect` to filter the cursor's record stream.

### M8 — Harbour conformance (this release)

- **M8.0** `ace64.dll` / `ace32.dll` SHARED CMake target with a `.def`
  exporting 80 real `Ads*` entry points.
- **M8.1** 226 `Ads*` exports — superset of every symbol Harbour
  `rddads.lib` references; the 146 newly-stubbed entries return
  `AE_FUNCTION_NOT_AVAILABLE` (5004) so the link resolves cleanly.
- **M8.2** Six legacy MSVC2013-era CRT shims (`_dclass`, `_dsign`,
  `_wfsopen`, `_getch`, `_kbhit`, `_eof`) re-exported under aliases
  so Harbour's prebuilt msvc64 libs link against modern UCRT.
- **M8.2** `smoke.exe` runs end-to-end: `AdsVersion()` resolves
  through the rddads wrapper to OpenADS' `AdsGetVersion`.
- **M8.3** `USE data VIA "ADSCDX"` + walk records. `Connection::open_table`
  auto-appends `.dbf`. `AdsGetField` / `AdsGetFieldType` /
  `AdsGetFieldLength` accept either string field names or the
  `ADSFIELD(n)` integer-cast-as-pointer form. `AdsConnect`
  is now a real wrapper around `AdsConnect60`.
- **M8.4** ACE field-type constants verified by sweeping
  `AdsGetFieldType`'s return through 0..40 against rddads. Result:
  `ADS_LOGICAL = 1`, `ADS_NUMERIC = 2`, `ADS_DATE = 3`,
  `ADS_STRING = 4`, ... — the inverse of the public ACE SDK ordering
  in some places. Mapping captured in `include/openads/ace.h`.
- **M8.5** Multi-field DBF (C/N/L/D) end-to-end. `AdsGetFieldDecimals`,
  `AdsGetLong`, `AdsGetDouble`, `AdsGetJulian` real impls (was 5004
  stubs); `AdsGetJulian` parses `YYYYMMDD` and computes Clipper Julian
  Day Numbers using the same Gregorian formula as `hb_dateEncode`.
- **M8.6** `dbSeek` end-to-end through OpenADS' CDX. `Table::path()`,
  index path resolution + auto-extension, polymorphic `get_table`
  (accepts table or index handles — rddads' `adsGoTop` calls
  `AdsGotoTop(hOrdCurrent)` when an order is active), 6-arg `AdsSeek`
  signature matching rddads, real `AdsIsFound` reading
  `Table::last_seek_found_`.
- **M8.7** Write path: `dbAppend` + `FIELD-> := value` + `dbCommit` +
  reopen. `AdsSetString` / `AdsSetLogical` / `AdsSetDouble` /
  `AdsSetLongLong` / `AdsSetJulian` real impls; field index resolution
  via `resolve_field_index`.
- **M8.8** Active index auto-syncs on every record mutation.
  `Table::compute_index_key_` evaluates bare-field-name expressions
  against the current `record_buf_`; `Table::sync_active_index_`
  erases the prior `(recno, prev_key)` and inserts the new one.
  `Table::flush()` flushes both the driver and the index.

### Tests

- 135 doctest cases / 1820 assertions passing on Windows / MSVC
  Release.
- One `tests/harbour_smoke/` integration harness producing a
  runnable `smoke.exe` that exercises the full Harbour →
  rddads.lib → OpenADS path.
