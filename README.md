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

## Validation

The success criterion is that existing Harbour applications using `contrib/rddads` work unmodified when `ace32.dll` / `ace64.dll` are replaced with OpenADS builds, running the tests in `c:\harbour\contrib\rddads\tests\` (`datad.prg`, `manage.prg`).

## License

MIT.
