---
title: Home (EN)
layout: default
nav_order: 2
permalink: /en/
has_children: true
---

# OpenADS — Documentation (English)

OpenADS is a free, clean-room implementation of an
ADS-compatible database engine. It is a **drop-in replacement**
for the Advantage Client Engine (`ace32.dll` / `ace64.dll` /
`libace.so`) — Harbour / Clipper applications that link against
`contrib/rddads` keep working without recompilation.

## What's in here

- **[Getting started](getting-started/)** — install, first build,
  smoke test.
- **[Architecture](architecture/)** — five-layer architecture
  (ABI / Session / SQL / Engine / Platform), where each module
  lives in the source tree.
- **[Wire protocol](wire-protocol/)** — formal spec of the
  OpenADS-native TCP wire (frame layout, every opcode, payload
  format, error semantics, versioning).
- **[Studio (web console)](studio-guide/)** — administer an
  OpenADS database from any browser through the embedded HTTP
  console hosted by `openads_serverd`.

## Other languages

[Español](/es/) · [Português](/pt/)
