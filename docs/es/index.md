---
title: Inicio (ES)
layout: default
nav_order: 3
permalink: /es/
has_children: true
---

# OpenADS — Documentación (Español)

OpenADS es una implementación libre y *clean-room* de un motor
de base de datos compatible con ADS. Funciona como **reemplazo
directo** del Advantage Client Engine (`ace32.dll` /
`ace64.dll` / `libace.so`) — las aplicaciones Harbour / Clipper
que enlazan contra `contrib/rddads` siguen funcionando sin
recompilar.

## Contenido

- **[Primeros pasos](primeros-pasos/)** — instalación, primer
  build, smoke test.
- **[Arquitectura](arquitectura/)** — arquitectura de cinco
  capas (ABI / Sesión / SQL / Motor / Plataforma).
- **[Protocolo de cable](protocolo-cable/)** — especificación
  formal del protocolo TCP nativo OpenADS (frames, opcodes,
  payload, errores, versionado).
- **[Studio (consola web)](guia-studio/)** — administración del
  motor desde cualquier navegador a través de la consola HTTP
  embebida en `openads_serverd`.

## Otros idiomas

[English](/en/) · [Português](/pt/)
