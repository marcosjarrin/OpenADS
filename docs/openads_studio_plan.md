# OpenADS Studio — design plan

OpenADS-equivalent of the legacy graphical front-end ADS shipped
under the name "Advantage Data Architect" (also referred to as
"ARC"). The original was a Windows / Linux / macOS desktop tool for
designing tables, browsing data, running SQL, and managing schema
inside an Advantage-style data dictionary.

## Legal posture (must read first)

OpenADS Studio is a clean-room reimplementation of the *generic
features common to every database GUI* (table browser, SQL editor,
schema designer). It is built solely from publicly observable
behaviour and from feature lists that appear in third-party
publications, **not** from disassembling the original SAP-owned
binary, copying its UI artwork, or reading any material covered
by the Advantage SDK / ACE EULA.

Hard rules:

- The product **must not** be called "Advantage Data Architect" or
  "ARC". Working name: **OpenADS Studio**. Final brand to be
  picked by the project maintainer; any name that doesn't conflict
  with SAP-held marks is fine.
- No screenshots, icons, glyphs, or layout templates may be copied
  from the SAP product.
- No code may be written by reading the original tool's binary or
  decompiling it. Reverse-engineering UI from screenshots is also
  out of scope.
- Generic database-tool features (SQL editor with syntax
  highlighting, table grid, "right-click → New Index") are
  industry-standard idioms that every database GUI ships
  (DBeaver, DataGrip, Postico, MySQL Workbench…). Implementing
  those from first principles is not an SAP-derivative work.

## Scope (v0.1 of OpenADS Studio)

| Feature | Notes |
|---------|-------|
| Connect via local path or `tcp://` / `tls://` URI | Reuses `ace64.dll` / `libace.so`. |
| Schema browser (left panel) | Tree of tables, indexes, memo fields, encrypted-DBF flag. |
| Table data grid | Read rows lazily via `RemoteConnection::fetch_batch` (M12.11). |
| SQL editor | Plain text + syntax highlighting (keywords, strings, comments). Run on Ctrl+Enter; results in a tab-grid. |
| New Table wizard | Free-form schema editor (column name, type, len, dec); calls `AdsCreateTable`. |
| New Index wizard | Picks expression + ASC / DESC + UNIQUE; calls `AdsCreateIndex61`. |
| Encrypted-table toggle | Calls `AdsEncryptTable`. |
| Find / Replace inside SQL editor | Standard text-editor feature. |
| Export results to CSV | Walks the cursor returned by `AdsExecuteSQLDirect`. |

Out of scope for v0.1 (planned later):

- Form designer (the original tool's most product-specific feature;
  needs careful clean-room scoping).
- Visual query builder (graphical join layout).
- SQL debugger (step-through stored procedures).
- Live multi-user lock viewer.

## Tech choices

### GUI framework

- **Dear ImGui** (MIT) — vendored single header + .cpp into
  `third_party/imgui/`. Backends: SDL2 + OpenGL 3 (cross-platform,
  no native GUI runtime needed). Used by many DB / profiling tools
  (Tracy, RemoteryProfiler, etc).

  **Why**: matches the project's vendor-deps philosophy, single-
  binary deployment, MIT-compatible with Apache 2.0, no Qt LGPL
  dynamic-linking complications.

  **Alternatives considered**: Qt 6 (LGPL, large redistributable,
  worth a second look only if richer native widgets become
  important); Web UI served by an embedded HTTP server (low-
  friction install, but browser not guaranteed present).

### Build / packaging

- New top-level CMake target `openads_studio` (executable).
- Sources under `tools/studio/`.
- Vendor SDL2 and Dear ImGui into `third_party/`. SDL2 is zlib-
  licensed (MIT-compatible). OpenGL is system-provided.
- Optional packaging step (CPack) to produce single-file binaries
  per platform: `openads-studio-<ver>-<os>-<arch>.zip`.

### Process layout

```
+----------------------------+
|  OpenADS Studio (process)  |
|                            |
|  +----------------------+  |
|  | Dear ImGui UI (MT)   |  |
|  | -- panels, grids,    |  |
|  |    SQL editor        |  |
|  +-----------+----------+  |
|              |             |
|  +-----------v----------+  |
|  | ace64.dll / libace.so |  |
|  | (Ads* C ABI calls)    |  |
|  +-----------+----------+  |
|              |             |
+--------------+-------------+
               | local or
               | tcp:// / tls://
+--------------v-------------+
| OpenADS engine (in-proc)   |
| or openads_serverd over    |
| TCP / TLS                  |
+----------------------------+
```

Studio dynamically links `ace64.dll` (or the local-mode static
library on POSIX); every database operation is one or more
`Ads*` calls. The studio is **just another caller of the public
ABI** — it does not reach into the engine internals, so it
benefits from the same drop-in-replacement guarantee real
Harbour apps already enjoy.

## Phased milestones

| Tag       | Scope |
|-----------|-------|
| `studio.0.1` | Skeleton: connect to data dir / `tcp://`; left tree of tables; row grid for selected table; basic SQL editor with Run + result grid. |
| `studio.0.2` | Table designer (CREATE TABLE wizard) + Index wizard. |
| `studio.0.3` | SQL syntax highlighting via `ImGuiColorTextEdit` (MIT). |
| `studio.0.4` | Find / Replace in SQL editor + result CSV export. |
| `studio.0.5` | Encryption toggle (`AdsSetEncryptionPassword` + `AdsEncryptTable`). |
| `studio.0.6` | DD `.add` browser — list members, RI links, users / groups (read-only first cut). |
| `studio.0.7` | Restructure-table dialog — expose `AdsRestructureTable`. |
| `studio.0.8` | SQL history persisted to `~/.openads-studio/history.log`. |
| `studio.1.0` | Stabilisation, cross-platform release artefacts. |

## Risks + mitigations

| Risk | Mitigation |
|------|-----------|
| Trademark / trade-dress confusion with SAP product | Distinct name; no copied UI assets; no reverse-engineering. |
| LGPL contamination from Qt | Use Dear ImGui (MIT) instead. |
| User confusion about which engine is being driven | About-box clearly states "OpenADS engine vX.Y.Z" and the loaded `ace64.dll` build provenance. |
| Distribution of vendored mbedtls | Apache 2.0 + clearly attributed in `third_party/mbedtls/LICENSE` + `NOTICE`. |
| Cross-platform window managers (HiDPI on Mac / Wayland on Linux) | SDL2 abstracts these; Dear ImGui scales via `ImGui::GetIO().FontGlobalScale`. |

## Out-of-the-box experience

```sh
# After installing OpenADS:
openads-studio                                          # opens the GUI

# Or with a pre-set connection URL on the CLI:
openads-studio --connect file:///home/me/data
openads-studio --connect tcp://server.lan:6262/data
openads-studio --connect tls://server.lan:6263/data --user me --pwd ...
```

## Why ship a Studio at all

The drop-in `ace64.dll` already lets existing Harbour apps run on
top of OpenADS without recompiling. A separate Studio gives:

- A **first-time onboarding path** for users who don't already
  have a Harbour build environment but want to inspect a `.dbf`
  or run an SQL query.
- A **demo surface** for the OpenADS roadmap (TLS, AEP host,
  encrypted DBFs) — features that are otherwise invisible to a
  non-Harbour user.
- A **regression-test harness** with humans in the loop — running
  the same UI on Windows / Linux / macOS surfaces platform-
  specific bugs (path separators, code page surprises) faster
  than headless CI.

The Studio is **not** a goal of OpenADS 1.0; it is a separate
follow-on product that consumes OpenADS 1.0 through its public
ABI. The first Studio release would target **OpenADS 1.0.x** as
the engine version.
