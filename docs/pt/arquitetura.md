---
title: Arquitetura
layout: default
parent: Início (PT)
nav_order: 2
permalink: /pt/arquitetura/
---

# Arquitetura

OpenADS é um sistema de cinco camadas. Cada camada é um ponto
de troca que uma aplicação ou teste pode usar
independentemente.

```
L1  ABI               exports extern "C" Ads*
                      (ace32/64.dll, libace.so/.dylib)
L2  Sessão            Connection / Statement / HandleRegistry / Tx
L3  Motor SQL         Lexer → Parser → Resolver → Planner → Executor
                      AEP host, UDFs xBase
L4  Núcleo motor      Table / Index / MemoStore / Cursor / LockMgr
                      TxLog (WAL) / Catalog
L5  Plataforma        File / mmap / locks por intervalo / sockets
                      Implementações Win32 + POSIX
```

## Responsabilidades por camada

| Camada | Responsabilidade |
|--------|------------------|
| **L1** | Único módulo com ABI C. Traduz chamadas `Ads*` para a API C++ interna; converte códigos de erro ACE de/para `util::Error`; conversões OEM / ANSI / UTF-8 / UTF-16. |
| **L2** | Estado por conexão — tabelas abertas, declarações SQL preparadas, pilha de transações, registro de procedimentos AEP, chave de criptografia. |
| **L3** | Dialeto SQL completo do Advantage — árvores WHERE booleanas, joins (INNER / LEFT / RIGHT / FULL OUTER), subqueries (correlacionadas e não), GROUP BY + HAVING, UNION, funções janela, CTEs, CASE, projeção escalar / agregada / aritmética. |
| **L4** | Motor agnóstico ao formato — `Table`, `Index`, `MemoStore`, `Cursor`, `LockMgr`, `TxLog`, `Catalog`. O trait `Driver` é o ponto de extensão para novos formatos. |
| **L5** | Abstração multi-plataforma do SO (Win32 + POSIX). |

## Drivers (extensão L4)

```
AdtDriver    .adt + .adm + .adi    (ADS proprietário — fora de escopo)
CdxDriver    .dbf + .cdx + .fpt    (FoxPro)
NtxDriver    .dbf + .ntx + .dbt    (Clipper)
VfpDriver    .dbf + .cdx + .fpt    (Visual FoxPro)
```

## Servidor Phase 2

`openads_serverd` roda L2–L5 in-process e os expõe via o
protocolo de fio nativo OpenADS sobre TCP. A mesma DLL que
fala com um diretório local também fala com um servidor remoto
via URI `tcp://host:porta/<dir>`.

## Studio Phase 2 (console web)

Quando o daemon é compilado com `OPENADS_WITH_HTTP=ON`, um
servidor HTTP embutido (cpp-httplib) serve uma SPA de
administração em outra porta. Cada requisição REST abre uma
conexão ABI curta — o console web é **outro consumidor do ABI
público**, igual a uma app Harbour.
