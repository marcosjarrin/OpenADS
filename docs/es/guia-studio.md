---
title: Studio (consola web)
layout: default
parent: Inicio (ES)
nav_order: 3
permalink: /es/guia-studio/
---

# Studio — consola web

OpenADS Studio es una consola web estilo phpMyAdmin embebida en
el binario `openads_serverd`. Corre donde corre el daemon
(Windows, Linux, macOS) y se accede desde cualquier navegador
de la red — sin cliente nativo que instalar.

## Habilitar + arrancar

```sh
cmake -S . -B build/http -DOPENADS_WITH_HTTP=ON
cmake --build build/http --target openads_serverd --config Release

./build/http/tools/serverd/openads_serverd \
    --port 6262 \
    --http-port 6263 \
    --data /ruta/a/tus/datos
```

Después abre `http://<host-servidor>:6263/`.

![Pestaña inicio de Studio](/OpenADS/assets/img/studio/01-home.png)

## Pestañas

| Pestaña | Función |
|---------|---------|
| **Browse**    | Grid paginado de registros. Botones Editar / Borrar / Recall por fila. |
| **Structure** | Metadatos de columnas + recuento + tamaño en disco. Botones Drop + Encrypt. |
| **Insert**    | Formulario auto-generado por el schema; añade un registro. |
| **SQL**       | Editor SQL libre. Ctrl+Enter ejecuta. Ctrl+Up / Ctrl+Down recupera del historial. Export CSV. |
| **Server**    | Versión motor + dir datos + lista de tablas. |

### Browse

![Pestaña Browse — filas paginadas de employees.dbf](/OpenADS/assets/img/studio/02-browse.png)

### Structure

![Pestaña Structure — columnas, recuento, botones Drop / Encrypt](/OpenADS/assets/img/studio/03-structure.png)

### Insert

![Pestaña Insert — formulario auto-generado por schema](/OpenADS/assets/img/studio/04-insert.png)

### SQL

![Pestaña SQL — query + grid resultado](/OpenADS/assets/img/studio/05-sql.png)

### Server

![Pestaña Server — info del motor](/OpenADS/assets/img/studio/06-server.png)

## Enlaces directos URL

La SPA lee `?table=<n>&tab=<browse|structure|insert|sql|server>`
al cargar, así enlaces externos caen directo a una vista. La
pestaña `SQL` también acepta `?q=<sql-urlencoded>` y `&autorun=1`.

## API REST

Cada panel se apoya en una superficie REST pequeña — útil para
scripting desde Python / curl.

| Método + ruta | Función |
|---------------|---------|
| `GET /api/health`                          | health probe |
| `GET /api/server/info`                     | motor + tablas |
| `GET /api/tables`                          | listar `*.dbf` |
| `POST /api/tables`                         | CREATE TABLE (DDL SQL) |
| `DELETE /api/tables/<n>`                   | borrar archivo + sidecars |
| `GET /api/tables/<n>/schema`               | metadatos columnas |
| `GET /api/tables/<n>/rows?offset=&limit=`  | browse paginado |
| `POST /api/tables/<n>/insert`              | añadir fila |
| `POST /api/tables/<n>/update?recno=N`      | sobrescribir columnas |
| `POST /api/tables/<n>/delete?recno=N`      | marcar borrado (`?recall=1` para recuperar) |
| `POST /api/tables/<n>/encrypt`             | cifrar in place AES-256-CTR |
| `POST /api/sql`                            | SQL arbitrario |

## Despliegues típicos

- **Admin local**: `--http-port 6263`, abre `localhost:6263`.
- **Admin LAN**: misma flag, abre `http://servidor.lan:6263`.
- **Admin remoto vía SSH**: `ssh -L 6263:localhost:6263 servidor`,
  abre `localhost:6263`. SSH cifra y autentica el túnel.
- **Móvil**: cualquier navegador responsive accede al mismo
  endpoint — el CSS de Studio escala a viewports de teléfono.
