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

- `OPENADS_WITH_HTTP=ON` (**por defecto desde v1.0.0-rc20**) —
  compila la consola web **Studio** dentro de `openads_serverd`
  *y* dentro de `ace64.dll` / `ace32.dll` (modo LocalServer).
  Pasa `-DOPENADS_WITH_HTTP=OFF` para excluirla.
- `cmake -DOPENADS_WITH_TLS=ON …` — habilita URIs `tls://` en
  `AdsConnect60`. Empaqueta `mbedtls 3.6 LTS` (Apache-2.0) y la
  **enlaza estáticamente** desde v1.0.0-rc8 — cero dependencias
  runtime de `libssl` / `libcrypto` / `mbedtls`.

El ZIP de release Windows incluye **ambos** `ace64.dll` (x64) y
`ace32.dll` (x86) más `openads_serverd_{x64,x86}.exe` desde
v1.0.0-rc8, así que apps X#, Harbour-x86 y Clipper legacy eligen
la bitness correcta desde una sola descarga.

## Smoke test (drop-in)

Coloca `ace64.dll` (o `libace.so`) en el `PATH` de una
aplicación Harbour antes de cualquier copia de SAP. Las
llamadas existentes de `contrib/rddads` ahora caen en OpenADS.

## Compila tu propia app Harbour contra OpenADS (`hbmk2` / `.hbp`)

El repo trae una plantilla `hbmk2` lista para usar en
[`examples/harbour-hbmk2/`](https://github.com/FiveTechSoft/OpenADS/tree/main/examples/harbour-hbmk2)
— coloca tu `.prg` junto a `openads_demo.hbp`, define
`OPENADS_LIB` apuntando al build de OpenADS, ejecuta `hbmk2`. El
`.exe` resultante mueve tablas DBF / CDX a través del RDD estándar
`contrib/rddads` de Harbour, pero cada llamada `Ads*` cae en
`ace64.dll` de OpenADS en vez de cualquier copia firmada por SAP.

```cmd
:: Desde un Developer Command Prompt x64 de Visual Studio:
cd examples\harbour-hbmk2
set OPENADS_LIB=C:\OpenADS\build\default\src\Release
set PATH=C:\harbour\bin\win\msvc64;%OPENADS_LIB%;%PATH%
hbmk2 openads_demo.hbp
copy /y "%OPENADS_LIB%\ace64.dll" .
openads_demo.exe
```

El `.hbp` es deliberadamente minimal — solo las dos entradas de
enlace que cambian para OpenADS:

```hbmk
openads_demo.prg
-comp=msvc64
-lrddads                    # RDD ADS de Harbour (contrib/rddads)
-L${OPENADS_LIB}
-lace64                     # Import lib de OpenADS (en vez de la de SAP)
-lrddcdx
-lrddntx
-lrddfpt
```

Hay una variante 32-bit (`openads_demo_x86.hbp` → `-lace32`) y
un `build.sh` para Linux / macOS. Para apps GUI FiveWin (FWH)
`hbmk2` no basta — mira
[`examples/fivewin/`](https://github.com/FiveTechSoft/OpenADS/tree/main/examples/fivewin)
para `build_msvc64.cmd`, que replica el build estándar FWH
añadiendo `rddads.lib` + `ace64.lib` de OpenADS.

Errores típicos "mi `.hbp` no compila":

| Síntoma | Causa probable |
|---|---|
| `unresolved external symbol AdsConnect60` (o cualquier `Ads*`) | `OPENADS_LIB` sin definir, o toolchain incorrecto — `ace64.lib` es MSVC; para bcc64 / MinGW usa la lib import correspondiente. |
| `lib 'rddads' not found` | `contrib/rddads` no compilado para el `-comp=…` elegido. Recompila el contrib de Harbour para ese toolchain. |
| Runtime `ace64.dll not found` | DLL no está junto al exe ni en `PATH`. |
| Cadenas truncadas en un TBrowse / xBrowse | Corregido en v1.0.0-rc27 — `AdsGetField` ahora paddea CHAR al ancho declarado. |
| `AdsVersion()` reporta algo como `12.0` / `11.10` | Cargaste el `ace64.dll` de SAP. `where ace64.dll` y reordena `PATH`. |

## Smoke test (servidor TCP + Studio)

```sh
cmake --preset default
cmake --build build/default --target openads_serverd --config Release

./build/default/tools/serverd/openads_serverd \
    --port 6262 \
    --http-port 6263 \
    --data /ruta/a/tus/datos
```

Después abre `http://localhost:6263/` en cualquier navegador.

## Studio LocalServer (in-process)

Desde v1.0.0-rc9 la misma consola Studio queda embebida en
`ace64.dll` / `ace32.dll`. Una app Harbour / X# / Clipper que
cargue la DLL OpenADS obtiene la SPA en su propio proceso — sin
daemon. Define `OPENADS_STUDIO_PORT=8080` antes de lanzar la app
para auto-arranque, o llama a `AdsStudioStart(port, data_dir)`
desde el código host. Detalles en [Studio](guia-studio/).

## Ejecutar `openads_serverd` como servicio

Desde v1.0.0-rc14:

- **Windows**: `openads_serverd --install-service` (auto-start
  vía SCM); `--uninstall-service` lo retira.
- **Linux**: `scripts/openads-serverd.service` es una unit
  systemd hardened (`User=openads`, `ProtectSystem=strict`,
  `NoNewPrivileges`).
- **macOS**: `scripts/com.openads.serverd.plist` es un launchd
  plist con KeepAlive on crash.

Detalle en [Despliegue como servicio](servicio-despliegue/).
