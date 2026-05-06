---
title: Benchmarks
layout: default
parent: Início (PT)
nav_order: 5
permalink: /pt/benchmarks/
---

# Benchmarks

`tools/bench/openads_bench` gera um DBF sintético de 100 000
linhas (`ID N(8,0)`, `TAG C(4)`, `AMT N(8,2)`) e cronometra um
conjunto fixo de cargas SQL através da ABI pública
(`AdsExecuteSQLDirect`). Mediana de 5 repetições por carga,
builds `Release`.

## Resultados v0.4.x (2026-05-06)

| Carga (mediana ms)     | Windows MSVC | Linux clang -O3 | macOS AppleClang |
|------------------------|-------------:|----------------:|-----------------:|
| criar DBF 100 k linhas |        63.5  |          57.9   |           34.0   |
| `SELECT COUNT(*)`      |       297.7  |          42.0   |          103.9   |
| `WHERE TAG = 'AAAA'`   |       303.7  |          48.3   |          108.4   |
| `SUM/AVG/MIN/MAX(AMT)` |       374.3  |         120.5   |          136.1   |
| `GROUP BY TAG`         |       321.9  |          58.6   |          120.9   |
| `ORDER BY AMT LIMIT 10`|       668.0  |         165.4   |          260.5   |
| `DISTINCT TAG`         |       598.4  |          95.2   |          213.4   |
| `BETWEEN 100 AND 500`  |       314.1  |          63.7   |          114.4   |

Linux clang -O3 vence em todas as cargas SQL — aproximadamente
7× mais rápido que MSVC Release no COUNT de tabela completa,
4× no `ORDER BY` mais pesado. macOS Intel fica no meio.

## Bench v2 — cargas com índices (Windows MSVC, 100 k linhas)

| Carga (mediana ms)      | ms |
|-------------------------|---:|
| `CREATE INDEX ID_IDX`   | 38.0 |
| `WHERE ID = 50000` (pós-índice)         | 308.0 |
| `WHERE ID BETWEEN 10000 AND 20000`      | 308.2 |
| `UNION ALL` de dois selects filtrados   | 608.2 |
| `GROUP BY TAG HAVING COUNT(*) > 100`    | 0.2 |

Que `indexed_eq` ~308 ms ≈ `seq_walk_where` ~315 ms expõe uma
oportunidade conhecida: o planner SQL atualmente NÃO empurra os
predicados WHERE para um índice CDX/NTX correspondente. Fechar
essa lacuna é um milestone futuro.

## Executar no seu hardware

```sh
cmake --build build/default --target openads_bench --config Release
./build/default/tools/bench/openads_bench --rows 100000 --repeats 5 --csv
```

O flag `--csv` emite uma linha CSV por carga.
