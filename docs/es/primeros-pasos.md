---
title: Primeros pasos
layout: default
parent: Inicio (ES)
nav_order: 1
permalink: /es/primeros-pasos/
---

# Primeros pasos

OpenADS es un proyecto CMake en C++17. Compila en Windows
(MSVC), Linux (clang o gcc) y macOS (AppleClang).

## Compilar

```sh
git clone https://github.com/FiveTechSoft/OpenADS
cd OpenADS
cmake --preset default
cmake --build build/default --config Release
ctest --test-dir build/default --output-on-failure -C Release
```

Binarios generados:

- `ace64.dll` (Windows) / `libace.so` (Linux) / `libace.dylib`
  (macOS) bajo `build/default/src/Release/` — el reemplazo
  directo del ACE.
- `tools/serverd/openads_serverd` — CLI servidor TCP.
- `tools/bench/openads_bench` — temporizador de cargas SQL
  multi-plataforma.

## Opciones del build

- `cmake -DOPENADS_WITH_TLS=ON …` — habilita URIs `tls://` en
  `AdsConnect60`. Empaqueta `mbedtls 3.6 LTS` (Apache 2.0) en
  tiempo de configuración.
- `cmake -DOPENADS_WITH_HTTP=ON …` — habilita la consola web
  **Studio** embebida en `openads_serverd`. Empaqueta
  `cpp-httplib` y `nlohmann/json`.

## Smoke test (drop-in)

Coloca `ace64.dll` (o `libace.so`) en el `PATH` de una
aplicación Harbour antes de cualquier copia de SAP. Las
llamadas existentes de `contrib/rddads` ahora caen en OpenADS.

## Smoke test (servidor TCP + Studio)

```sh
cmake -S . -B build/http -DOPENADS_WITH_HTTP=ON
cmake --build build/http --target openads_serverd --config Release

./build/http/tools/serverd/openads_serverd \
    --port 6262 \
    --http-port 6263 \
    --data /ruta/a/tus/datos
```

Después abre `http://localhost:6263/` en cualquier navegador.
