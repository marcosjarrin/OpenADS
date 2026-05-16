# AdsMg* server-telemetry subsystem — design

Date: 2026-05-16
Status: approved (brainstorming), pending implementation plan

## Problem

The `AdsMg*` management API (~17 exports in `src/abi/ace_exports.cpp`,
lines 9773-9818, plus `AdsMgDumpInternalTables` at 10132) is currently
stubbed: each `AdsMgGet*` zero-fills the caller's struct via `mg_zero_`
and returns `AE_SUCCESS`. Harbour's `manage.prg` test consequently
prints every counter as `0` even when connected to a live server.

The stubs are correct as *placeholders* but report no real telemetry.
This design replaces them with a real subsystem that reports live
server (and local-process) activity.

## Goals

- Make all ~17 `AdsMg*` functions report real data.
- Single source of truth — no divergent local vs. remote telemetry.
- Remote path: client DLL queries the server daemon over the wire.
- Local path: in-process engine state is reported directly.
- ABI fidelity: fill the SAP-canonical `ADS_MGMT_*` structs from
  `include/openads/ace.h` exactly.
- Honesty: fields OpenADS genuinely lacks report a real `0`, with the
  reason documented — distinct from "stub returns 0".

## Non-goals

- NetWare/NLM-era telemetry (receive/send ECBs, burst packets) — these
  have no analogue in OpenADS; reported as `0`.
- Per-category memory accounting — reported as `0` except a process
  RSS total; a real allocator-instrumented breakdown is out of scope.
- A dedicated management TCP port — the existing data port is reused.

## Chosen approach — "one collector, two transports"

A single `MgCollector` class reads engine/server state and fills the
`ADS_MGMT_*` structs. It runs identically in two contexts:

- **Local mode** — `AdsMg*` invokes `MgCollector` directly against the
  in-process engine.
- **Remote mode** — the server's `MgRequest` opcode handler invokes its
  own `MgCollector` against the server's `Server` + `MgStats`, then
  serializes the reply.

The remote client side (`RemoteMgTelemetry`) is a thin wire client: it
ships the request and deserializes the reply. It never re-implements
telemetry collection, so local and remote can never diverge.

Approaches considered and rejected:
- *Provider interface with two collector impls* — `LocalMgTelemetry`
  and `RemoteMgTelemetry` each collecting telemetry; two collectors
  drift apart over time.
- *Inline local/remote branching in every export* — heavy duplication,
  no shared abstraction.

## Architecture

### New components

**`MgCollector`** — `src/mgmt/mg_collector.{h,cpp}`

Single source of truth. One method per `AdsMg` call:

| Method | Fills |
|---|---|
| `activity_info()` | `ADS_MGMT_ACTIVITY_INFO` |
| `comm_stats()` | `ADS_MGMT_COMM_STATS` |
| `install_info()` | `ADS_MGMT_INSTALL_INFO` |
| `config_params()` | `ADS_MGMT_CONFIG_PARAMS` |
| `config_memory()` | `ADS_MGMT_CONFIG_MEMORY` |
| `user_names()` | `vector<ADS_MGMT_USER_INFO>` |
| `open_tables()` | `vector<ADS_MGMT_TABLE_INFO>` |
| `open_indexes()` | `vector<ADS_MGMT_INDEX_INFO>` |
| `locks()` | `vector<ADS_MGMT_LOCK_INFO>` |
| `lock_owner(table, recno)` | `ADS_MGMT_LOCK_INFO` |
| `worker_thread_activity()` | `vector<ADS_MGMT_THREAD_ACTIVITY>` |
| `server_type()` | `UNSIGNED16` |
| `kill_user(conn_no)` | mutator |
| `reset_comm_stats()` | mutator |
| `dump_internal_tables()` | mutator (no-op success) |

Constructed with references to whatever it reads:
- Server context: `Server&` (session registry) + `MgStats&`.
- Local context: the in-process connection/handle registry + a
  process-local `MgStats`.

**`MgStats`** — `src/mgmt/mg_stats.h`

Process-global counter block, all `std::atomic`. Holds what
`sessions_snapshot()` cannot derive:

- `start_time` — set at `Server::start()`; drives uptime.
- Comm counters — packets in/out, bytes in/out, server-initiated
  disconnects, partial connections.
- High-water marks — max-seen users / connections / tables / indexes
  / locks (updated whenever the live count rises).

One instance per process.

**`mg_wire`** — `src/network/mg_wire.{h,cpp}`

Explicit field-by-field serialization of every `ADS_MGMT_*` struct,
fixed little-endian. **Not** raw POD `memcpy`: the client DLL may be
32-bit MinGW while the server is 64-bit; raw copy risks struct padding
/ ABI mismatch. Each numeric field is encoded LE; each char array is
length-prefixed or fixed-width copied. Round-trip property:
`decode(encode(x)) == x`.

### Wire protocol

New opcodes in `src/network/wire.h`, range `0xA0+`:

```
MgConnect   = 0xA0    MgConnectAck = 0xA1
MgRequest   = 0xA2    MgReplyAck   = 0xA3
Error       = 0xFF    (reused)
```

A single generic `MgRequest` / `MgReplyAck` pair carries all 17 calls.
Request payload: `[u8 mg_kind][args…]` where `mg_kind` selects the
method. Reply payload: the serialized struct or struct vector. This
keeps the wire surface to one opcode pair.

An unknown `mg_kind` yields an `Error` frame.

### Client side — `ace_exports.cpp`

- `LocalMgTelemetry` — wraps a local `MgCollector`.
- `RemoteMgTelemetry` — owns a socket / transport to the server;
  `AdsMgConnect` opens it.
- `AdsMgConnect` decides local vs. remote from the server path string:
  a server path (e.g. `\\host:port\`) → remote; empty → local. The
  result is registered as an `ADSHANDLE` tagged as a management handle.
- Each `AdsMg*` export resolves the handle → telemetry backend → calls
  the method → copies the struct into the caller buffer with the
  existing `copy_to_caller` size-clamping pattern (`*pusLen` in/out).

### Server side — `server.cpp`

`session_loop` dispatches `MgRequest`: builds a server-context
`MgCollector(server, mg_stats)`, runs the requested method, serializes
the reply into a `MgReplyAck` frame.

## Telemetry sourcing

Already available from `Server::sessions_snapshot()`:
- connections = session count
- work areas / tables = sum of `SessionInfo.open_tables`
- users = `SessionInfo.user` + peer ip/port + `connected_at`
- frame counts = `frames_in` / `frames_out`

To be instrumented:
- **Uptime** — `MgStats.start_time` at `Server::start()`.
- **Comm stats** — packets/bytes in/out, server-initiated disconnects,
  partial connections: counters bumped in the transport layer.
- **High-water marks** — `MgStats` tracks max-seen counts.
- **Locks** — `lock_mgr` enumerates active locks → `ADS_MGMT_LOCK_INFO`;
  `lock_owner` looked up by table + record number.
- **Open indexes** — per-table order count → `ADS_MGMT_INDEX_INFO`.
- **Worker threads** — server thread count → `ADS_MGMT_THREAD_ACTIVITY`.
- **Memory** — process RSS via a `platform` call into
  `ADS_MGMT_CONFIG_MEMORY.ulTotalConfigMem`; per-category fields `0`.
- **Config params** — populated from real OpenADS server config (port,
  data dir, paths, configured maxima).
- **Install info** — product string + version string.

### Honesty: fields reported as a real zero

These have no OpenADS analogue. They report `0` because `0` is the
accurate value, and the spec records why:

| Field(s) | Reason |
|---|---|
| `dPercentCheckSums`, `ulCheckSumFailures` | wire framing has no checksum |
| `ulRcvPktOutOfSeq`, `ulRcvReqOutOfSeq` | TCP stream — no app-level sequencing |
| `ulInvalidPackets`, `ulRecvFromErrors`, `ulSendToErrors` | NT/NLM-specific counters |
| `usNumReceiveECBs`, `usNumSendECBs`, `usNumBurstPackets` | NetWare/NLM-era; no analogue |
| `aucSerialNumber`, `aucEvalExpireDate` | OpenADS is not serial-licensed |
| TPS* elem counts and memory | transaction-processing internals not exposed |
| per-category memory in `ADS_MGMT_CONFIG_MEMORY` | no allocator instrumentation |

## Error handling

- Remote wire failure → `AE_*` connection error code.
- Unknown / non-management handle → `AE_INVALID_HANDLE`.
- Caller buffer smaller than the struct → fill what fits, set `*pusLen`
  to the actual size, return `AE_SUCCESS` (matches `copy_to_caller`).
- `kill_user` on an unknown connection → `AE_SUCCESS` (idempotent).
- `MgRequest` with an unknown `mg_kind` → `Error` frame; client maps it
  to `AE_INVALID_OPTION`.

## Testing

- Unit — `MgCollector` against a fabricated engine/server state;
  assert each struct field.
- Unit — `mg_wire` serialization round-trip (`decode(encode(x)) == x`).
- Integration — start a server, `AdsMgConnect` remote,
  `AdsMgGetActivityInfo`; assert `stConnections.ulInUse` reflects a
  separately opened `AdsConnect`.
- `tests/unit/abi_mgmt_test.cpp` — updated; no longer asserts all-zero.
- Harbour smoke — run `manage.prg` against the iMac server; expect
  non-zero connections and uptime.

## Files

New:
- `src/mgmt/mg_collector.{h,cpp}`
- `src/mgmt/mg_stats.h`
- `src/network/mg_wire.{h,cpp}`

Modified:
- `src/network/wire.h` — new opcodes
- `src/network/server.{h,cpp}` — `MgRequest` handler, `MgStats` wiring
- `src/network/transport.*` / `socket.*` — comm-counter bumps
- `src/abi/ace_exports.cpp` — real `AdsMg*`, `Local`/`RemoteMgTelemetry`
- `tests/unit/abi_mgmt_test.cpp`
- `tests/smoke/harbour` — `manage.prg` expectations

## Implementation phasing

The implementation plan should sequence this as:

1. `MgStats` + `MgCollector` (local context only) + unit tests.
2. `mg_wire` serialization + round-trip tests.
3. Wire opcodes + server `MgRequest` handler.
4. Client `Local`/`RemoteMgTelemetry` + real `AdsMg*` exports.
5. Transport-layer counter instrumentation.
6. Test refresh + Harbour smoke.
