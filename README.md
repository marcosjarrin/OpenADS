# OpenADS

A free and open-source implementation compatible with Advantage Database Server (ADS), discontinued by SAP.

The goal is to provide a *drop-in* replacement for the Advantage Client Engine (`ace32.dll` / `ace64.dll` / `libace.so`) so existing applications — particularly Harbour/Clipper apps using `contrib/rddads` — keep working without recompilation.

## Status

Phase 1 in design. No code yet.

## Phase 1 scope

| Topic | Decision |
|------|----------|
| Operation mode | LOCAL only (no remote server). Phase 2 will add a TCP server reusing the same engine. |
| Table formats | ADT + CDX + NTX + VFP (all four ADS-supported types). |
| Memo / index | ADM / FPT / DBT (memo) · ADI / CDX / NTX (index). |
| ABI compatibility | Identical C ABI to ACE; applications do not recompile. |
| Validation frontend | `c:\harbour\contrib\rddads`, unmodified. |
| SQL | Full Advantage SQL dialect (parser + planner + executor + xBase UDFs + AEP host for external stored procedures). |
| Concurrency | OS *byte-range* locking with ranges identical to ACE — coexistence with original ACE installations during migration. |
| Data Dictionary (`.add`) | Full support: member tables, users/groups/permissions, RI, views, procedures, links, validations, defaults. |
| Encryption | AES-128 / AES-256 (ADS 9+ format). The legacy proprietary cipher is out of scope for phase 1. |
| Transactions (TPS) | Multi-table ACID, savepoints, crash recovery via write-ahead log. |
| Platforms | Windows (x86 + x64), Linux, macOS, BSD. |
| Language / build | C++17 with `extern "C"` external ABI. CMake + GitHub Actions. |
| i18n | OEM ↔ ANSI ↔ UTF-8 ↔ UTF-16 (the API's `*W` variants). |
| License | MIT. |

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│  Harbour application (no recompilation)                          │
│    REQUEST ADS / dbUseArea( .T., "ADS", ... )                    │
└────────────────────────┬─────────────────────────────────────────┘
                         │  Clipper RDD API
┌────────────────────────▼─────────────────────────────────────────┐
│  rddads.lib  (contrib/rddads — untouched)                        │
│    ads1.c / adsfunc.c / adsx.c / adsmgmnt.c                      │
└────────────────────────┬─────────────────────────────────────────┘
                         │  ACE C ABI (~230 Ads* entry points)
                         │  ace.h headers
═════════════════════════╪══════════════════════════════════════════
                         │       ▼  OPENADS REPLACES HERE
┌────────────────────────▼─────────────────────────────────────────┐
│  L1 — ABI Layer  (libace32.dll / libace64.dll / libace.so)       │
│    extern "C" wrappers → ACE handle translation → C++ engine     │
│    Error code mapping (ACE codes ↔ engine errors)                │
│    OEM / ANSI / UTF-8 / UTF-16 translation                       │
└────────────────────────┬─────────────────────────────────────────┘
                         │  internal C++ API (RAII handles)
┌────────────────────────▼─────────────────────────────────────────┐
│  L2 — Session / Connection Layer                                 │
│    Connection (local path or Data Dictionary)                    │
│    Statement (prepared SQL cursor)                               │
│    HandleRegistry (ADSHANDLE → object pointer, thread-safe)      │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│  L3 — SQL Engine                                                 │
│    Lexer → Parser (AST) → Resolver → Planner → Executor          │
│    DD-aware Catalog, xBase UDFs                                  │
│    AEP host (stored procedures as .dll/.so plugins)              │
└────────────────────────┬─────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────────┐
│  L4 — Engine Core (transport-agnostic)                           │
│    Table / Index / MemoStore / Cursor / Filter (AOF)             │
│    LockMgr (OS byte-range, ACE-compatible ranges)                │
│    TxLog (multi-table WAL ACID + savepoints + crash recovery)    │
│    Catalog (DD .add reader/writer)                               │
│    PageCache / BufferMgr                                         │
└────────────────────────┬─────────────────────────────────────────┘
                         │  Driver trait (open / read / write page)
        ┌────────────────┼────────────────┬───────────────┐
        ▼                ▼                ▼               ▼
   ┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
   │AdtDriver│     │CdxDriver│     │NtxDriver│     │VfpDriver│
   │.adt+.adm│     │.dbf+.cdx│     │.dbf+.ntx│     │.dbf+.fpt│
   │   +.adi │     │   +.fpt │     │   +.dbt │     │   +.cdx │
   └────┬────┘     └────┬────┘     └────┬────┘     └────┬────┘
        └────────────────┴────────────────┴────────────────┘
                                 │
┌────────────────────────────────▼─────────────────────────────────┐
│  L5 — OS Abstraction (Platform)                                  │
│    File I/O · mmap · byte-range locks · paths · time · threads   │
│    Win32 and POSIX implementations                               │
└──────────────────────────────────────────────────────────────────┘
```

### Key boundaries

- **L1** is the only module with C ABI. Everything else is internal C++17.
- L4's **driver trait** is the extension point: each table format is a swappable driver.
- The **remote server** in phase 2 will simply be another L1 frontend (TCP transport layer); L2 through L5 are reused as is.
- The **SQL engine** (L3) consumes L4 only through `Cursor` and `Catalog`; it has no knowledge of file formats.
- The **LockMgr** preserves the exact byte-range semantics of ACE, allowing coexistence with original ACE installations on the same files during migration.

## Repository layout

```
OpenADS/
├── CMakeLists.txt              # top-level build, presets per platform
├── CMakePresets.json
├── LICENSE                     # MIT
├── README.md
├── docs/
│   ├── architecture.md
│   ├── ace-coverage.md         # entry-point status table (~230 fns)
│   ├── adt-format.md           # ADT/ADM/ADI on-disk spec
│   ├── lock-ranges.md          # ACE-compat byte-range table
│   ├── tx-log.md               # WAL format + recovery protocol
│   └── sql-grammar.md          # Advantage SQL EBNF + diffs
│
├── third_party/                # vendored deps
│   ├── tinyaes/                # AES-128/256 (MIT)
│   ├── utf8.h/                 # UTF conversion (MIT)
│   ├── doctest/                # unit test framework (MIT)
│   └── ace-headers/            # ace.h, adscd.h (Sybase, redistribution OK)
│
├── include/openads/            # public C++ headers (consumed by L1)
│   ├── engine.h
│   ├── connection.h
│   ├── table.h
│   ├── cursor.h
│   ├── catalog.h
│   └── error.h
│
├── src/
│   ├── abi/                    # L1 — ACE C ABI shim
│   │   ├── ace_exports.def     # Windows DLL export list
│   │   ├── ace_exports.cpp     # ~230 extern "C" thunks
│   │   ├── handle_registry.cpp # ADSHANDLE ↔ object map
│   │   ├── error_map.cpp       # ACE error codes
│   │   ├── charset.cpp         # OEM/ANSI/UTF conversion
│   │   └── version.cpp         # AdsGetVersion / AdsGetServerName
│   │
│   ├── session/                # L2
│   │   ├── connection.cpp
│   │   ├── statement.cpp
│   │   └── globals.cpp         # AdsSetDefault / AdsSetFileType / etc.
│   │
│   ├── sql/                    # L3
│   │   ├── lex/lexer.cpp
│   │   ├── parse/parser.cpp        # recursive-descent
│   │   ├── parse/ast.h
│   │   ├── resolve/resolver.cpp    # name binding, type check
│   │   ├── plan/planner.cpp        # logical → physical
│   │   ├── plan/optimizer.cpp      # predicate pushdown, index selection
│   │   ├── exec/executor.cpp       # iterator pipeline
│   │   ├── exec/operators/         # scan / filter / sort / agg / join / ...
│   │   ├── func/scalar.cpp         # xBase UDFs (LEFT/SUBSTR/CTOD/...)
│   │   ├── func/aggregate.cpp
│   │   └── aep/host.cpp            # AEP plugin loader (.dll / .so)
│   │
│   ├── engine/                 # L4 — core
│   │   ├── table.cpp
│   │   ├── cursor.cpp
│   │   ├── filter_aof.cpp      # Advantage Optimized Filter
│   │   ├── lock_mgr.cpp        # OS byte-range, ACE-compat ranges
│   │   ├── tx_log.cpp          # WAL writer
│   │   ├── tx_recover.cpp      # crash recovery
│   │   ├── savepoint.cpp
│   │   ├── catalog_dd.cpp      # .add reader / writer
│   │   ├── page_cache.cpp
│   │   ├── buffer_mgr.cpp
│   │   └── encryption.cpp      # AES-128 / 256
│   │
│   ├── drivers/                # L4 — format drivers
│   │   ├── driver_trait.h      # abstract interface
│   │   ├── adt/
│   │   │   ├── adt_table.cpp   # .adt header + records
│   │   │   ├── adi_index.cpp   # .adi B+tree
│   │   │   ├── adm_memo.cpp    # .adm blob store
│   │   │   └── field_types.cpp # autoinc / GUID / modtime / timestamp / ...
│   │   ├── cdx/
│   │   │   ├── dbf_table.cpp
│   │   │   ├── cdx_index.cpp   # FoxPro CDX compact index
│   │   │   └── fpt_memo.cpp
│   │   ├── ntx/
│   │   │   ├── dbf_table.cpp   # shared with cdx via dbf_common
│   │   │   ├── ntx_index.cpp   # Clipper NTX
│   │   │   └── dbt_memo.cpp
│   │   ├── vfp/
│   │   │   ├── vfp_table.cpp   # DBF v0x30 + nullable + autoinc
│   │   │   ├── cdx_index.cpp   # symlink to ../cdx
│   │   │   └── fpt_memo.cpp
│   │   └── dbf_common.cpp      # shared DBF header logic
│   │
│   ├── platform/               # L5 — OS abstraction
│   │   ├── file.h
│   │   ├── file_win32.cpp
│   │   ├── file_posix.cpp
│   │   ├── lock.h
│   │   ├── lock_win32.cpp      # LockFileEx
│   │   ├── lock_posix.cpp      # fcntl F_SETLK
│   │   ├── mmap.cpp
│   │   ├── path.cpp            # case-insensitive matching on unix
│   │   ├── time.cpp
│   │   └── thread.cpp
│   │
│   └── util/
│       ├── span.h
│       ├── result.h            # error-or-value
│       └── log.cpp
│
├── tests/
│   ├── unit/                   # doctest, per-module
│   │   ├── adt_table_test.cpp
│   │   ├── cdx_index_test.cpp
│   │   ├── lock_mgr_test.cpp
│   │   ├── tx_log_test.cpp
│   │   ├── sql_parser_test.cpp
│   │   └── ...
│   ├── integration/            # full ABI roundtrip
│   │   ├── harbour_smoke.prg   # runs against rddads
│   │   ├── byte_compat/        # diff vs reference ACE-produced files
│   │   └── conformance/        # ACE entry-point matrix
│   └── fixtures/               # canonical .adt / .dbf / .cdx samples
│
├── tools/
│   ├── adt-dump/               # CLI: hex-dump ADT structure
│   ├── cdx-dump/
│   ├── tx-replay/              # WAL replay / inspect
│   └── ace-trace/              # log every ACE call (debug shim)
│
├── benchmarks/
│   └── micro/                  # paged read, lock contention, SQL
│
└── .github/workflows/
    ├── ci.yml                  # matrix Win / Linux / macOS / BSD
    ├── compat.yml              # nightly run vs Harbour rddads tests
    └── release.yml             # tagged DLL / SO artifacts
```

### Module notes

- **`src/abi/ace_exports.cpp`** is the only contact point with the C ABI. A `static constexpr` table maps each ACE entry point to its internal handler. Optionally generated by a script from `ace.h`.
- **`src/drivers/driver_trait.h`** defines the minimum interface: `open / close / read_record / write_record / seek / scan / index_create / index_seek / lock_range`. Each driver is roughly 3–5 files.
- **`src/engine/lock_mgr.cpp`** centralises lock ranges — the single source of truth for ACE coexistence.
- **`src/engine/tx_log.cpp`** and **`tx_recover.cpp`** are driver-independent: the WAL log records `(driver_id, page_no, before_image, after_image)` as opaque payloads.
- **`src/sql/`** is driver-independent and operates against an abstract `Cursor`. SQL tests do not require real drivers (mock cursor).
- **`src/platform/`** is the only OS dependency. Engine tests use a platform mock to inject I/O failures.
- **`tools/`** is the debugging artillery — critical for byte-level diffs against original ACE.

## Data flow

### Example A — Opening a CDX table from Harbour

```
Harbour PRG
  USE clientes VIA "ADSCDX" SHARED
       │
       ▼
rddads.lib :: hb_adsOpen()                 [contrib/rddads/ads1.c]
  AdsConnect60( "C:\data", ..., &hConn )   ← OpenADS L1 entry
  AdsOpenTable( hConn, "clientes.dbf",     ← OpenADS L1 entry
                ADS_CDX, ADS_DEFAULT,
                ADS_NONE, ADS_DEFAULT,
                ADS_DEFAULT, &hTbl )
       │
       ▼  L1 ace_exports.cpp
extern "C" AdsConnect60(...)
  → openads::Connection::open( path, ... )
       │
       ▼  L2 session/connection.cpp
Connection ctor
  → resolves path, registers handle, returns ADSHANDLE via HandleRegistry
       │
       ▼  back to L1
extern "C" AdsOpenTable(...)
  → conn->open_table( "clientes.dbf", FormatHint::Cdx, OpenMode::Shared )
       │
       ▼  L2 → L4 engine/table.cpp
Table::open()
  1. DriverFactory::detect_or_force(path, hint) → CdxDriver
  2. CdxDriver::open()
       ├─ platform::file_open("clientes.dbf", RW)
       ├─ read DBF header (32 bytes + field descriptors)
       ├─ platform::file_open("clientes.cdx", RW)   if it exists
       ├─ CdxIndex::load_root_pages()
       └─ FptMemo::open("clientes.fpt")              if memo fields
  3. LockMgr::acquire_open_share()  (byte-range header lock, ACE range)
  4. PageCache::register(file_handles)
  5. TxLog::register_table(table_id) (no-op outside a transaction)
  6. returns Table*
       │
       ▼
HandleRegistry::register(Table*) → ADSHANDLE
       │
       ▼
return SUCCESS to rddads → returned to PRG
```

### Example B — `SELECT * FROM clientes WHERE saldo > 1000 ORDER BY nombre`

```
PRG
  AdsCreateSQLStatement( hConn, &hStmt )
  AdsExecuteSQLDirect( hStmt, "SELECT...", &hCursor )
       │
       ▼  L1
extern "C" AdsExecuteSQLDirect(hStmt, sql, &cursor)
  → stmt->execute_direct(sql)
       │
       ▼  L2 session/statement.cpp → L3
Statement::execute_direct(sql)
  ┌──────────── L3 sql/ pipeline ───────────────────────────┐
  │ Lexer    → tokens                                        │
  │ Parser   → AST (SelectStmt)                              │
  │ Resolver → bind "clientes" via Catalog → Table*          │
  │            bind columns saldo, nombre → ColumnRef        │
  │            type-check predicates                         │
  │ Planner  → LogicalPlan:                                  │
  │              Sort(nombre)                                │
  │                └─ Filter(saldo > 1000)                   │
  │                     └─ Scan(clientes)                    │
  │ Optimizer → predicate pushdown, index selection:         │
  │              if index on (saldo) → IndexRangeScan        │
  │              else                  → SeqScan + Filter    │
  │              if index on (nombre)  → drop Sort           │
  │ Executor → builds operator tree (iterator pipeline):     │
  │              SortOp                                      │
  │                └─ FilterOp                               │
  │                     └─ TableScanOp(Cursor over Table)    │
  └──────────────────────────────────────────────────────────┘
       │
       ▼  Cursor handed back
HandleRegistry::register(Cursor*) → ADSHANDLE returned as hCursor
       │
       ▼
PRG fetches rows via AdsGotoTop / AdsGetRecord / AdsSkip
  → each call drives one Executor::next() through L4 PageCache
  → AdsGetField → field decode (xBase types or ADT extended types,
                  depending on driver)
```

### Example C — Multi-table transaction

```
AdsBeginTransaction(hConn)
  → TxLog::begin(tx_id)         (write BEGIN record)
  → LockMgr::tx_attach(tx_id)

AdsLockRecord(hTblA, 42)
  → LockMgr::lock_record(tx_id, A, 42)   (escalates byte-range lock)

AdsUpdateRecord(hTblA, ...)
  → Table::update_record():
       1. Capture before-image of pages dirtied
       2. Apply change in PageCache
       3. TxLog::log_update(tx_id, A, page_no, before, after)

AdsAppendRecord(hTblB, ...)
  → similar, writes a log record for table B

AdsCommitTransaction(hConn)
  → TxLog::commit(tx_id)        (write COMMIT, fsync log)
  → PageCache::flush_tx_pages(tx_id)
  → LockMgr::tx_release(tx_id)

AdsRollbackTransaction(hConn)  [alternative]
  → TxLog::rollback_walk(tx_id):
       reverse-iterate log, restore before-images via PageCache
  → LockMgr::tx_release(tx_id)
```

#### Crash recovery (next process startup)

```
TxRecover::run()
  scan TxLog tail
  for each tx without COMMIT          → roll back (apply before-images)
  for each tx with COMMIT but pages unflushed → roll forward
  truncate log
```

## Validation

The success criterion is that existing Harbour applications using `contrib/rddads` work unmodified when `ace32.dll` / `ace64.dll` are replaced with OpenADS builds, running the tests in `c:\harbour\contrib\rddads\tests\` (`datad.prg`, `manage.prg`).

## License

MIT.
