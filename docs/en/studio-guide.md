---
title: Studio (web console)
layout: default
parent: Home (EN)
nav_order: 3
permalink: /en/studio-guide/
---

# Studio — web console

OpenADS Studio is a phpMyAdmin-style web console embedded in the
`openads_serverd` binary. It runs anywhere the daemon runs
(Windows, Linux, macOS) and is reachable from any browser on
the network — no native client to install.

## Enable + launch

```sh
# Configure with HTTP support
cmake -S . -B build/http -DOPENADS_WITH_HTTP=ON
cmake --build build/http --target openads_serverd --config Release

# Run
./build/http/tools/serverd/openads_serverd \
    --port 6262 \
    --http-port 6263 \
    --data /path/to/your/data
```

Then open `http://<server-host>:6263/` in any browser.

![Studio home pane — table list + welcome](/OpenADS/assets/img/studio/01-home.png)

## Panes

| Pane | What it does |
|------|--------------|
| **Browse**    | Paginated grid of records. Edit / delete / recall buttons per row. |
| **Structure** | Column metadata + record count + file size. Drop-table + Encrypt buttons. |
| **Insert**    | Form auto-generated from the table schema; appends a new record. |
| **SQL**       | Free-form SQL editor. Ctrl+Enter runs. Ctrl+Up / Ctrl+Down recalls history. CSV export. |
| **Server**    | Engine version + data dir + table list. |

### Browse

![Browse pane — paginated rows of employees.dbf](/OpenADS/assets/img/studio/02-browse.png)

### Structure

![Structure pane — columns, record count, drop / encrypt buttons](/OpenADS/assets/img/studio/03-structure.png)

### Insert

![Insert pane — schema-driven form](/OpenADS/assets/img/studio/04-insert.png)

### SQL

![SQL pane — query + result grid](/OpenADS/assets/img/studio/05-sql.png)

### Server

![Server pane — engine info](/OpenADS/assets/img/studio/06-server.png)

## URL deep links

The SPA reads `?table=<name>&tab=<browse|structure|insert|sql|server>`
on load so external links land directly on a specific view. The
`SQL` tab also accepts `?q=<urlencoded-sql>` and `&autorun=1`.

## REST API

Each panel is implemented on top of a small REST surface — useful
when scripting against the server from Python / curl / etc.

| Method + path | Purpose |
|---------------|---------|
| `GET /api/health`                          | liveness probe |
| `GET /api/server/info`                     | engine + tables |
| `GET /api/tables`                          | list `*.dbf` |
| `POST /api/tables`                         | CREATE TABLE (DDL via SQL) |
| `DELETE /api/tables/<n>`                   | drop file + sidecars |
| `GET /api/tables/<n>/schema`               | column metadata |
| `GET /api/tables/<n>/rows?offset=&limit=`  | paginated browse |
| `POST /api/tables/<n>/insert`              | append row |
| `POST /api/tables/<n>/update?recno=N`      | overwrite columns |
| `POST /api/tables/<n>/delete?recno=N`      | mark deleted (or `?recall=1` to undelete) |
| `POST /api/tables/<n>/encrypt`             | AES-256-CTR encrypt in place |
| `POST /api/sql`                            | run arbitrary SQL |

## Deployment shapes

- **Local admin**: `--http-port 6263`, browse via `localhost:6263`.
- **LAN admin**: same flag, browse via `http://server.lan:6263`.
- **Remote admin via SSH**: `ssh -L 6263:localhost:6263 server`,
  browse via `localhost:6263`. The SSH tunnel handles encryption
  and authentication; the daemon itself listens on `127.0.0.1`
  inside the remote host.
- **Mobile**: any responsive browser hits the same endpoint —
  Studio's CSS scales to phone-sized viewports.
