---
title: Getting started
layout: default
parent: Home (EN)
nav_order: 1
permalink: /en/getting-started/
---

# Getting started

OpenADS is a CMake project written in C++17. It builds on
Windows (MSVC), Linux (clang or gcc), and macOS (AppleClang).

## Build

```sh
git clone https://github.com/FiveTechSoft/OpenADS
cd OpenADS
cmake --preset default
cmake --build build/default --config Release
ctest --test-dir build/default --output-on-failure -C Release
```

Output binaries:

- `ace64.dll` (Windows) / `libace.so` (Linux) / `libace.dylib`
  (macOS) under `build/default/src/Release/` — the drop-in ACE
  replacement.
- `tools/serverd/openads_serverd` — standalone TCP server CLI.
- `tools/bench/openads_bench` — cross-platform SQL workload timer.

## Optional features

- `cmake -DOPENADS_WITH_TLS=ON …` — enables `tls://` client URIs
  in `AdsConnect60`. Vendors `mbedtls 3.6 LTS` (Apache 2.0) at
  configure time via FetchContent.
- `cmake -DOPENADS_WITH_HTTP=ON …` — enables the **Studio**
  web console embedded in `openads_serverd`. Vendors
  `cpp-httplib` and `nlohmann/json`.

## Smoke test (drop-in)

Place `ace64.dll` (or `libace.so`) on a Harbour application's
`PATH` ahead of any SAP-shipped copy. Existing `contrib/rddads`
calls now hit OpenADS.

## Smoke test (TCP server + Studio)

```sh
# 1. Build with HTTP enabled
cmake -S . -B build/http -DOPENADS_WITH_HTTP=ON
cmake --build build/http --target openads_serverd --config Release

# 2. Launch the daemon
./build/http/tools/serverd/openads_serverd \
    --port 6262 \
    --http-port 6263 \
    --data /path/to/your/data

# 3. Open the Studio in any browser
xdg-open http://localhost:6263/         # Linux
open       http://localhost:6263/       # macOS
start      http://localhost:6263/       # Windows
```
