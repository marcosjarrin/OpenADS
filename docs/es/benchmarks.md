---
title: Benchmarks
layout: default
parent: Inicio (ES)
nav_order: 5
permalink: /es/benchmarks/
---

# Benchmarks

`tools/bench/openads_bench` genera un DBF sintético de 100 000
filas (`ID N(8,0)`, `TAG C(4)`, `AMT N(8,2)`) y mide un conjunto
fijo de cargas SQL a través de la ABI pública
(`AdsExecuteSQLDirect`). Mediana de 5 repeticiones por carga,
builds `Release`.

## Resultados v0.4.x (2026-05-06)

| Carga (mediana ms)     | Windows MSVC | Linux clang -O3 | macOS AppleClang |
|------------------------|-------------:|----------------:|-----------------:|
| crear DBF 100 k filas  |        63.5  |          57.9   |           34.0   |
| `SELECT COUNT(*)`      |       297.7  |          42.0   |          103.9   |
| `WHERE TAG = 'AAAA'`   |       303.7  |          48.3   |          108.4   |
| `SUM/AVG/MIN/MAX(AMT)` |       374.3  |         120.5   |          136.1   |
| `GROUP BY TAG`         |       321.9  |          58.6   |          120.9   |
| `ORDER BY AMT LIMIT 10`|       668.0  |         165.4   |          260.5   |
| `DISTINCT TAG`         |       598.4  |          95.2   |          213.4   |
| `BETWEEN 100 AND 500`  |       314.1  |          63.7   |          114.4   |

Linux clang -O3 gana en todas las cargas SQL — aproximadamente
7× más rápido que MSVC Release en el COUNT de tabla completa,
4× en el `ORDER BY` más pesado. macOS Intel queda en medio.

## Bench v2 — cargas con índices (Windows MSVC, 100 k filas)

| Carga (mediana ms)      | ms |
|-------------------------|---:|
| `CREATE INDEX ID_IDX`   | 38.0 |
| `WHERE ID = 50000` (post-índice)        | 308.0 |
| `WHERE ID BETWEEN 10000 AND 20000`      | 308.2 |
| `UNION ALL` de dos selects filtradas    | 608.2 |
| `GROUP BY TAG HAVING COUNT(*) > 100`    | 0.2 |

Que `indexed_eq` ~308 ms ≈ `seq_walk_where` ~315 ms expone una
oportunidad conocida: el planner SQL actualmente NO empuja los
predicados WHERE a un índice CDX/NTX coincidente. Cerrar esa
brecha es un milestone futuro.

## Ejecutar en tu hardware

```sh
cmake --build build/default --target openads_bench --config Release
./build/default/tools/bench/openads_bench --rows 100000 --repeats 5 --csv
```

El flag `--csv` emite una fila CSV por carga.
