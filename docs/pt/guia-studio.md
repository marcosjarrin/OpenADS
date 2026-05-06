---
title: Studio (console web)
layout: default
parent: Início (PT)
nav_order: 3
permalink: /pt/guia-studio/
---

# Studio — console web

OpenADS Studio é um console web no estilo phpMyAdmin embutido
no binário `openads_serverd`. Roda onde o daemon roda
(Windows, Linux, macOS) e é acessível de qualquer navegador na
rede — sem cliente nativo para instalar.

## Habilitar + iniciar

```sh
cmake -S . -B build/http -DOPENADS_WITH_HTTP=ON
cmake --build build/http --target openads_serverd --config Release

./build/http/tools/serverd/openads_serverd \
    --port 6262 \
    --http-port 6263 \
    --data /caminho/dos/seus/dados
```

Depois abra `http://<host-servidor>:6263/`.

## Abas

| Aba | Função |
|-----|--------|
| **Browse**    | Grid paginado de registros. Botões Editar / Apagar / Recall por linha. |
| **Structure** | Metadados de colunas + contagem + tamanho em disco. Botões Drop + Encrypt. |
| **Insert**    | Formulário auto-gerado a partir do schema; anexa um registro. |
| **SQL**       | Editor SQL livre. Ctrl+Enter executa. Ctrl+Up / Ctrl+Down recupera o histórico. Export CSV. |
| **Server**    | Versão do motor + dir dados + lista de tabelas. |

## API REST

Cada painel se apoia em uma superfície REST pequena — útil para
scripting de Python / curl.

| Método + caminho | Função |
|------------------|--------|
| `GET /api/health`                          | health probe |
| `GET /api/server/info`                     | motor + tabelas |
| `GET /api/tables`                          | listar `*.dbf` |
| `POST /api/tables`                         | CREATE TABLE (DDL SQL) |
| `DELETE /api/tables/<n>`                   | apagar arquivo + sidecars |
| `GET /api/tables/<n>/schema`               | metadados das colunas |
| `GET /api/tables/<n>/rows?offset=&limit=`  | browse paginado |
| `POST /api/tables/<n>/insert`              | adicionar linha |
| `POST /api/tables/<n>/update?recno=N`      | sobrescrever colunas |
| `POST /api/tables/<n>/delete?recno=N`      | marcar deletado (`?recall=1` para recuperar) |
| `POST /api/tables/<n>/encrypt`             | criptografar in place AES-256-CTR |
| `POST /api/sql`                            | SQL arbitrário |

## Cenários de implantação

- **Admin local**: `--http-port 6263`, abra `localhost:6263`.
- **Admin LAN**: mesma flag, abra `http://servidor.lan:6263`.
- **Admin remoto via SSH**: `ssh -L 6263:localhost:6263 servidor`,
  abra `localhost:6263`. SSH cifra e autentica o túnel.
- **Mobile**: qualquer navegador responsivo acessa o mesmo
  endpoint — o CSS do Studio escala para viewports de celular.
