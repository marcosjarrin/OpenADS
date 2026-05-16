# OpenADS PHP binding (`bindings/php`) — design

Status: approved 2026-05-16. Target release: post-rc25.

## 1. Motivation

The proprietary Advantage Database Server shipped a PHP extension
that stopped working around PHP 5.2 and was never modernised. No
maintained PHP binding for the ACE C API exists today. OpenADS
already exposes a clean C ABI (`ace64.dll` / `ace32.dll` /
`libace.so`); a PHP binding lets PHP web apps reach OpenADS data
directly instead of going through `mod_harbour`.

This spec covers the first deliverable: a PHP FFI binding,
distributed as a pure-PHP Composer package, with a modern
object-oriented API.

## 2. Decisions

| Question | Decision |
|----------|----------|
| Connection mode | Both local and remote |
| API style | Own namespaced OOP classes (`OpenADS\`) |
| Operation scope | SQL + table navigation + writes |
| Binding mechanism | PHP FFI wrapper (no compiled C) |
| Parameter binding | Client-side in PHP (ACE has no host-variable binding) |
| Errors | Exceptions |

Local vs remote needs no branching in the binding: ACE's
`AdsConnect60` already dispatches on the connection URI — a local
filesystem path, `tcp://host:port/<dir>`, or `tls://...`. One
code path covers both modes.

A native Zend (PECL-style) extension is explicitly **out of
scope** for this deliverable; it is recorded as a possible
phase-2 backend behind the same OOP API.

## 3. Architecture

Pure-PHP Composer package `openads/openads-php`. No C
compilation. It loads `ace64.dll` / `ace32.dll` / `libace.so`
through PHP's `ext-ffi` and wraps the exports in a thin FFI layer
plus an OOP surface.

```
bindings/php/
  composer.json
  src/
    Ffi/AceLibrary.php       # loads lib, declares FFI signatures, path resolution
    Ffi/AceTypes.php         # ADS_* constants, ACE error codes
    Connection.php           # connect/disconnect; factory for Statement/Table
    Statement.php            # SQL: prepare, bind params, execute -> Cursor
    Cursor.php               # iterate SELECT rows (Iterator); fetch assoc/num
    Table.php                # navigation: open, top/skip/seek/goto, AOF
    Record.php               # fields: get/set, append, delete, recall, locks
    Exception/
      OpenAdsException.php    # base; maps ACE code -> message
      ConnectionException.php
      QueryException.php
  tests/                     # PHPUnit: remote serverd + local DBF
  examples/
    query.php
    navigate.php
  README.md
```

## 4. Components

Each component has one purpose, a defined interface, and is
testable on its own.

### 4.1 `Ffi\AceLibrary`

Singleton. The only file that touches `\FFI`.

- **Library resolution**, in order: `OPENADS_ACE_LIB` env var →
  a bundled library path next to the package → the system
  default search path.
- **Bitness**: picks `ace64` vs `ace32` from PHP's own bitness
  (`PHP_INT_SIZE`).
- **Signature declaration**: declares only the ACE exports the
  binding uses (see §8), via `FFI::cdef` against an inline C
  declaration string maintained in this file.
- Exposes a `call(string $fn, ...$args)` style accessor and
  helpers for ACE's pointer/out-parameter conventions
  (`UNSIGNED8*` buffers, `ADSHANDLE*` out-handles).

### 4.2 `Connection`

`new Connection(string $uri, ?string $user = null, ?string $pass = null)`.

- Constructor calls `AdsConnect60`; stores the connection handle.
- `statement(): Statement` — factory; calls `AdsCreateSQLStatement`.
- `table(string $name, ...): Table` — factory; opens a table.
- `close(): void` — `AdsDisconnect`; idempotent.
- Closes on destruct if still open.

### 4.3 `Statement`

SQL execution surface.

- `query(string $sql, array $params = []): Cursor|int` —
  bind params (§5), then `AdsExecuteSQLDirect` (not
  `AdsExecuteSQL`, which truncates SQL at 2048 bytes). Returns a
  `Cursor` for a result set, or an affected-row count for
  non-SELECT statements.
- `prepare(string $sql): self` / `execute(array $params)` — for
  re-running one statement with different params; re-binds and
  re-sends each time (ACE has no server-side prepared params).
- Closes the underlying statement handle on destruct.

### 4.4 `Cursor`

Wraps the cursor `ADSHANDLE` returned by a SELECT.

- Implements `\Iterator`: `AdsGotoTop` / `AdsSkip` / `AdsAtEOF`.
- Column metadata read once via `AdsGetNumFields` +
  `AdsGetFieldName` / `AdsGetFieldType`.
- `fetchAssoc(): ?array`, `fetchNum(): ?array`,
  `fetchAll(): array`, `count(): int` (`AdsGetRecordCount`).
- Values read with `AdsGetField`, converted to PHP types from
  the ACE field type (char→string, numeric→int/float,
  date→`DateTimeImmutable`, logical→bool, memo→string).

### 4.5 `Table`

Navigational xbase-style access.

- `open` is done by the `Connection::table()` factory.
- `gotoTop()`, `gotoBottom()`, `skip(int $n)`, `gotoRecord(int)`,
  `seek($key, ...)`, `recordCount()`, `atEof()`, `atBof()`.
- `setAof(string $expr)` / `clearAof()` — Rushmore filter.
- `record(): Record` — the cursor's current row.

### 4.6 `Record`

Fields of the table's current record.

- `get(string $field)`, `set(string $field, $value)`.
- `append(): void`, `delete(): void`, `recall(): void`.
- `lock(): bool`, `unlock(): void`.
- Type conversion identical to `Cursor` (§4.4).

### 4.7 Exceptions

- Every ACE export returns `UNSIGNED32`. A central check helper
  turns any non-`AE_SUCCESS` code into an `OpenAdsException`
  carrying the numeric code and the message from
  `AdsGetLastError`.
- `ConnectionException` for connect/disconnect failures,
  `QueryException` for SQL failures; both extend
  `OpenAdsException`.

## 5. Parameter binding

OpenADS ACE has **no host-variable parameter binding** —
`AdsPrepareSQL` only stores the SQL text, and there is no
`AdsSetSQLParameter` family. The binding therefore substitutes
parameters **client-side in PHP**, before the SQL reaches ACE.

- Two placeholder styles, not mixed in one statement:
  positional `?` and named `:name`.
- Quoting is per PHP value type:
  - `string` → single-quoted, embedded `'` doubled.
  - `int` / `float` → bare numeric literal.
  - `null` → `NULL`.
  - `bool` → `.T.` / `.F.`.
  - `DateTimeInterface` → ACE date/timestamp literal.
  - any other type → `QueryException`.
- This is the anti-injection boundary: callers pass values as
  params, never concatenate into SQL themselves. Escaping is
  centralised in one function with its own unit tests.

## 6. Error handling

- All ACE failures surface as exceptions (§4.7); no return-code
  checking leaks into user code.
- A peer-closed remote connection maps to the wire layer's
  `AE_INTERNAL_ERROR` (5000) and raises `ConnectionException`.
- `Connection::close()` and destructors never throw.

## 7. Clean-room / legal

- The API is original and namespaced under `OpenADS\`. It does
  **not** copy the names, signatures, or help text of the old
  proprietary PHP extension or the Data Architect help file.
- Function names of the ACE C API itself are already part of the
  public `include/openads/ace.h` clean-room surface and may be
  referenced.
- No proprietary SAP product names in the package, per repo
  policy — only the existing "Advantage Database Server"
  compatibility clause if a compat note is needed.

## 8. ACE exports used

The FFI layer declares only what the binding calls. Initial set:

- Connection: `AdsConnect60`, `AdsDisconnect`, `AdsGetLastError`.
- SQL: `AdsCreateSQLStatement`, `AdsCloseSQLStatement`,
  `AdsExecuteSQLDirect`.
- Cursor / navigation: `AdsGotoTop`, `AdsGotoBottom`, `AdsSkip`,
  `AdsGotoRecord`, `AdsAtEOF`, `AdsAtBOF`, `AdsGetRecordCount`,
  `AdsGetRecordNum`.
- Fields: `AdsGetNumFields`, `AdsGetFieldName`, `AdsGetFieldType`,
  `AdsGetFieldLength`, `AdsGetField`, `AdsSetField`.
- Table: `AdsOpenTable90`, `AdsCloseTable`, `AdsSeek`,
  `AdsSetAOF`, `AdsClearAOF`.
- Writes: `AdsAppendRecord`, `AdsDeleteRecord`, `AdsRecallRecord`,
  `AdsLockRecord`, `AdsUnlockRecord`.

Exact signatures are taken from `include/openads/ace.h` during
implementation; any export found missing or stubbed in OpenADS
is flagged in the implementation plan rather than worked around
silently.

## 9. Testing & distribution

- **PHPUnit** suite, run two ways: against a live
  `openads_serverd` (remote URI) and against a local DBF
  directory (local URI), so both connection modes are covered.
- The parameter-binding/escaping function gets dedicated unit
  tests independent of any live engine.
- **CI**: a new `release.yml` / `ci.yml` leg installs PHP 8.x
  with `ext-ffi` and runs the suite.
- **Distribution**: published on Packagist as
  `openads/openads-php`. README documents requirements: PHP
  7.4+, `ext-ffi` enabled, an `ace` library reachable via
  `OPENADS_ACE_LIB` or the system path.

## 10. Out of scope (YAGNI)

- Native Zend (PECL) extension — recorded as a possible phase-2
  backend behind the same OOP API; not built now.
- A PDO driver.
- `ext-ffi` preloading / opcache tuning — a README note, not
  code.
