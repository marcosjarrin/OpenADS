---
title: Primeiros passos
layout: default
parent: Início (PT)
nav_order: 1
permalink: /pt/primeiros-passos/
---

# Primeiros passos

OpenADS é um projeto CMake em C++17. Compila no Windows
(MSVC), Linux (clang ou gcc) e macOS (AppleClang).

## Compilar

```sh
git clone https://github.com/FiveTechSoft/OpenADS
cd OpenADS
cmake --preset default
cmake --build build/default --config Release
ctest --test-dir build/default --output-on-failure -C Release
```

Binários gerados:

- `ace64.dll` (Windows) / `libace.so` (Linux) / `libace.dylib`
  (macOS) em `build/default/src/Release/` — o substituto direto
  do ACE.
- `tools/serverd/openads_serverd` — CLI servidor TCP.
- `tools/bench/openads_bench` — temporizador de cargas SQL
  multi-plataforma.

## Opções de build

- `cmake -DOPENADS_WITH_TLS=ON …` — habilita URIs `tls://` em
  `AdsConnect60`. Empacota `mbedtls 3.6 LTS` (Apache 2.0).
- `cmake -DOPENADS_WITH_HTTP=ON …` — habilita o console web
  **Studio** embutido em `openads_serverd`.

## Smoke test (drop-in)

Coloque `ace64.dll` (ou `libace.so`) no `PATH` da aplicação
Harbour antes de qualquer cópia da SAP. As chamadas existentes
de `contrib/rddads` agora caem no OpenADS.

## Smoke test (servidor TCP + Studio)

```sh
cmake -S . -B build/http -DOPENADS_WITH_HTTP=ON
cmake --build build/http --target openads_serverd --config Release

./build/http/tools/serverd/openads_serverd \
    --port 6262 \
    --http-port 6263 \
    --data /caminho/dos/seus/dados
```

Depois abra `http://localhost:6263/` em qualquer navegador.
