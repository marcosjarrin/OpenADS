---
title: Studio (consola web)
layout: default
parent: Inicio (ES)
nav_order: 3
permalink: /es/guia-studio/
---

# Studio — consola web

OpenADS Studio es una consola web estilo phpMyAdmin que lista
las tablas de la conexión, muestra su esquema, ejecuta SQL ad-hoc
e inspecciona registros (incluidos campos memo / binarios). Viene
en dos modos:

- **Modo Remote-Server** — embebida dentro de `openads_serverd.exe`.
  El daemon expone tanto el protocolo wire OpenADS (TCP) como el
  listener HTTP de Studio en paralelo. Recomendado para
  despliegues compartidos / multi-usuario.
- **Modo LocalServer** — embebida dentro de `ace64.dll` /
  `ace32.dll`. Una aplicación Harbour / X# / Clipper que carga la
  DLL OpenADS directamente obtiene la misma consola web Studio en
  su propio proceso, sin necesidad de daemon separado. Recomendado
  para apps de escritorio mono-usuario, sesiones de depuración o
  para inspeccionar un proceso Clipper en marcha desde el navegador.

## Habilitar + arrancar — Remote Server (`openads_serverd`)

```sh
cmake -S . -B build/http -DOPENADS_WITH_HTTP=ON
cmake --build build/http --target openads_serverd --config Release

./build/http/tools/serverd/openads_serverd \
    --port 6262 \
    --http-port 6263 \
    --data /ruta/a/tus/datos \
    --http-user admin:secret      # opcional — registra un login
```

Después abre `http://<host-servidor>:6263/`.

## Habilitar + arrancar — LocalServer (in-process)

Studio se compila dentro de `openads_ace` (es decir, `ace64.dll` /
`ace32.dll`) cuando el build se configura con
`-DOPENADS_WITH_HTTP=ON`. Tres entry points propios de OpenADS
controlan la consola in-process:

```c
UNSIGNED32 AdsStudioStart(UNSIGNED16 usPort, UNSIGNED8* pucDataDir);
UNSIGNED32 AdsStudioStop (void);
UNSIGNED32 AdsStudioPort (UNSIGNED16* pusPort);
```

Dos formas de habilitarlo:

**1) Programáticamente.** Desde la app anfitriona (cualquier
lenguaje que pueda llamar al ABI C — Harbour, X#, Clipper, C++,
Python vía `ctypes`, …):

```c
AdsStudioStart(8080, (UNSIGNED8*)"C:\\app\\datos");
/* ... ShellExecute("http://localhost:8080") ... */
AdsStudioStop();
```

`AdsStudioStart` devuelve `AE_SUCCESS` (0) si todo va bien,
`AE_INTERNAL_ERROR` si el bind / listen falla (puerto ocupado o
`pucDataDir == NULL`), o `AE_FUNCTION_NOT_AVAILABLE` si la DLL se
compiló sin `-DOPENADS_WITH_HTTP=ON`.

**2) Auto-start por variable de entorno.** Define
`OPENADS_STUDIO_PORT=<puerto>` antes de lanzar la app anfitriona
y la DLL arranca Studio automáticamente al cargarse:

```bat
set OPENADS_STUDIO_PORT=8080
set OPENADS_STUDIO_DATA=C:\app\datos      :: por defecto = "."
set OPENADS_STUDIO_HOST=127.0.0.1         :: por defecto = 127.0.0.1
start MiApp.exe
```

El hook de auto-start corre desde `DllMain DLL_PROCESS_ATTACH` en
Windows y desde un constructor attribute en POSIX. Sin
`OPENADS_STUDIO_PORT` el hook no hace nada — la DLL no bindea
ningún puerto a menos que la app lo pida expresamente. Los
fallos de bind durante el auto-start son silenciosos para que el
proceso anfitrión nunca falle al cargar por una colisión de
puerto Studio; el `AdsStudioStart()` explícito sí devuelve
`AE_INTERNAL_ERROR` en ese caso.

### Locking + acceso compartido

Studio abre las tablas en sólo-lectura mediante conexiones ABI
de corta vida. Si la app tiene una tabla en modo EXCLUSIVE, el
navegador verá un error "table busy" para esa tabla hasta que la
app libere el lock exclusivo. Los opens compartidos conviven sin
problema, así que el patrón típico `USE … SHARED` de Harbour
funciona out of the box.

### Host de bind por defecto

El host de bind por defecto es `127.0.0.1`, **no** `0.0.0.0` —
Studio queda local-only por defecto, así que una app de escritorio
que cargue la DLL no expone silenciosamente su directorio de datos
en la LAN. Define `OPENADS_STUDIO_HOST=0.0.0.0` (o pasa un host
explícito por wrapper) cuando se necesite visibilidad LAN, y
combínalo con HTTP Basic auth (Remote Server admite usuarios vía
`--http-user`; LocalServer deja la consola abierta por diseño —
ponla detrás de un reverse proxy si tiene que estar en algo
distinto de `localhost`).

![Pestaña inicio de Studio](/OpenADS/assets/img/studio/01-home.png)

## Header

La barra superior tiene:

- **Selector de idioma** (`EN` / `ES` / `PT`) — UI cambia en vivo;
  persistido en `localStorage`.
- **🌙 / ☀ tema** — alterna paleta dark / light (CSS variables;
  persistido en `localStorage`).
- **📖 Docs** — link a este sitio.
- **Badge de modo** — 🏠 `LocalServer` (verde) si la consola corre
  in-process dentro de `ace64.dll` / `ace32.dll`, o 🌐 `Remote Server`
  (azul) si la sirve `openads_serverd`. Hover sobre el badge muestra
  el directorio de datos activo. Señal proviene del campo `mode` de
  `/api/health`.
- **Status** — resumen del dataset actual o último error.

## Sidebar

El sidebar izquierdo lista cada `*.dbf` del directorio. Tres
botones junto al título **Tables**:

| Botón | Acción |
|-------|--------|
| `↻` | Refrescar lista. |
| `⇪` | File picker nativo; subida multi-fichero vía `POST /api/upload`. |
| `+` | Modal Nueva tabla (columna por columna → CREATE TABLE DDL). |

Una segunda sección **Server / Info** enlaza a la pestaña Server.

## Pestañas

| Pestaña | Función |
|---------|---------|
| **Browse**    | Grid paginado de registros. Click en cabecera ordena; filtro encima del grid acota filas en la página actual. Botones Editar / Borrar / Recall por fila. Click en celda abre modal con valor completo (memo / texto largo). |
| **Structure** | Metadatos columnas + recuento + tamaño en disco. Botones Reindex / Pack / Zap / Download / Encrypt / Drop. Form 'Create index' inline (tag + expresión + DESC + UNIQUE). Lista de archivos compañeros (`.cdx`, `.ntx`, `.fpt`, `.dbt`, `.dbv`). |
| **Insert**    | Formulario auto-generado por schema; añade un registro. |
| **SQL**       | Editor SQL libre. Ctrl+Enter ejecuta. Ctrl+Up / Ctrl+Down recupera historial. Export CSV. Errores muestran mensaje del parser + hint 'did you mean…?' si la query mezcla comillas. |
| **Server**    | Versión motor + dir datos + lista tablas + breakdown bytes en disco (DBF / sidecar / total) + count diccionarios. |
| **Sessions**  | Registro vivo de cada sesión wire activa: peer IP / port, user, dir, tiempo conectado, idle, frames in/out, tablas abiertas. Auto-refresh 3 s. |
| **Dict**      | Browse / edit Data Dictionary `.add`: dropdown selector, lista TABLE / USER / INDEX / LINK / RI / DBPROP; forms add/remove; New-dict + Drop-dict. |

### Browse

![Pestaña Browse — filas paginadas de employees.dbf](/OpenADS/assets/img/studio/02-browse.png)

### Structure

![Pestaña Structure — columnas + botones Reindex / Pack / Zap](/OpenADS/assets/img/studio/03-structure.png)

### Insert

![Pestaña Insert — formulario por schema](/OpenADS/assets/img/studio/04-insert.png)

### SQL

![Pestaña SQL — query + grid resultado](/OpenADS/assets/img/studio/05-sql.png)

### Server

![Pestaña Server — info motor + breakdown disco](/OpenADS/assets/img/studio/06-server.png)

### Sessions

![Pestaña Sessions — conexiones wire vivas](/OpenADS/assets/img/studio/07-sessions.png)

### Dict

![Pestaña Dict — CRUD Data Dictionary](/OpenADS/assets/img/studio/08-dd.png)

## Enlaces directos URL

| Param        | Efecto |
|--------------|--------|
| `?table=<n>`                      | Pre-selecciona tabla en sidebar. |
| `?tab=<browse\|structure\|insert\|sql\|server\|sessions\|dd>` | Pre-abre pestaña. |
| `?q=<sql-urlencoded>`             | Pre-rellena editor (con `tab=sql`). |
| `&autorun=1`                      | Ejecuta query al cargar. |

## API REST

Mismo subset documentado en EN — cada panel se apoya en
endpoints REST scriptables desde Python / curl.

## Autenticación

Cuando se pasa `--http-user user:password` (repetible), cada
request requiere `Authorization: Basic …`. El navegador muestra
prompt nativo. Sin `--http-user` la consola es abierta.

## Despliegues típicos

- **Admin local**: `--http-port 6263`, abre `localhost:6263`.
- **Admin LAN**: misma flag, abre `http://servidor.lan:6263`.
- **Admin remoto vía SSH**: `ssh -L 6263:localhost:6263 servidor`,
  abre `localhost:6263`. SSH cifra y autentica el túnel.
- **Móvil**: cualquier navegador responsive accede al mismo
  endpoint — el CSS escala a viewports de teléfono.

## Hitos Studio

| Tag                | Scope |
|--------------------|-------|
| `studio.web.0.1`   | Skeleton: connect, lista tablas, editor SQL, grid resultado. |
| `studio.web.0.2`   | CRUD + browse paginado + pestaña Server. |
| `studio.web.0.3`   | CREATE / DROP table + Encrypt + historial SQL persistente. |
| `studio.web.0.4`   | Sessions tab. |
| `studio.web.0.5`   | Data Dictionary tab + REST. |
| `studio.web.0.6`   | Reindex / Pack / Zap + CREATE INDEX wizard + memo viewer. |
| `studio.web.0.7`   | Sidecar list + server-stats + DBF upload + refresh. |
| `studio.web.0.8`   | HTTP Basic auth + table download + theme toggle. |
| `studio.web.0.9`   | Browse sort + filter + i18n (EN / ES / PT). |
