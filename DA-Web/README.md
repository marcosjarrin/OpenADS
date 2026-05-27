# DA-Web — OpenADS Data Architect

Web-based replacement for SAP Data Architect. Manages OpenADS data dictionaries through a browser: browse tables, users, views, stored procedures, triggers, RI objects, and links. Execute ad-hoc SQL. All operations go through the **php_openads** native PHP extension.

---

## Stack

| Layer | Technology |
|---|---|
| Backend | PHP 8.x + php_openads (native Zend extension) |
| Tree | [jsTree 3.3.x](https://www.jstree.com/) + jQuery 3.7.x |
| Grid | [Tabulator 6.x](https://tabulator.info/) |
| Layout | [Split.js 1.6.x](https://split.js.org/) |
| Styles | Custom dark CSS (no Bootstrap dependency) |
| Server | Apache + mod_php (or any PHP-capable web server) |

---

## Directory structure

```
DA-Web/
├── index.php               Main application shell
├── setup.bat               Downloads all vendor files (run once)
├── api/
│   ├── connect.php         Open / close DD connections (session-stored)
│   ├── dictionaries.php    CRUD for the list of known data dictionaries
│   ├── execute_sql.php     Execute arbitrary SQL
│   ├── session_state.php   Return currently-open connections
│   └── tree.php            jsTree lazy-load provider
├── config/
│   └── dictionaries.json   Persisted list of data dictionaries (auto-managed)
├── css/
│   └── app.css             Application styles
├── js/
│   └── app.js              Application logic (menu, tree, tabs, modals)
└── vendor/                 Client-side libraries (see below — offline install)
    ├── jquery/
    ├── jstree/
    ├── tabulator/
    └── split.js/
```

---

## Quick start

### 1. Install vendor libraries (offline-safe)

Run **once** from the `DA-Web/` directory:

```bat
setup.bat
```

This downloads all JavaScript/CSS libraries into `vendor/` so the app works without an internet connection.

Or download manually — see [Vendor files](#vendor-files) below.

### 2. Enable `php_openads`

Add to `php.ini`:

```ini
extension=php_openads
```

Verify with:

```bat
php -m | findstr ads
```

### 3. Configure Apache

Map a virtual directory or alias to `DA-Web/`:

```apache
Alias /daweb "F:/OpenADS/DA-Web"
<Directory "F:/OpenADS/DA-Web">
    Options Indexes FollowSymLinks
    AllowOverride All
    Require all granted
</Directory>
```

Enable PHP sessions (the default `php.ini` session settings are fine for local use).

### 4. Open in browser

```
http://localhost/daweb/
```

---

## Using the application

### Add a data dictionary

1. **Connection → Add Data Dictionary…**
2. Enter a display name, the path to the `.add` file (or connection string), and an optional default username.
3. Click **Add**. The dictionary appears in the tree as a collapsed root node.

### Connect

- Click the dictionary node in the tree **or** choose **Connection → [name]** from the menu.
- Enter your password in the Connect dialog and click **Connect**.
- The tree node expands to show categories.

### Browse tables

Expand **Tables** under a connected dictionary, then click a table name. A new tab opens with up to 2 000 rows displayed in Tabulator with client-side pagination.

### SQL editor

Press **+** in the tab bar (or **File → New SQL Tab**). Select the target database from the drop-down, type SQL, and press **▶ Execute**.

- `SELECT` / `WITH` → results displayed in a Tabulator grid.
- `INSERT` / `UPDATE` / `DELETE` / DDL → success confirmation shown.

### Disconnect

Right-click the dictionary root node and choose **Disconnect**, or use **Connection → [name]** (shown as ● connected).

---

## Sessions and credentials

Credentials (username + password) are stored only in the **PHP session** (server-side). They are never written to disk or sent to the browser beyond the initial POST. Each API request re-opens a connection using the stored credentials, then closes it.

---

## Vendor files

All libraries must be placed in `vendor/` for offline operation. `setup.bat` handles this automatically. Manual locations:

| File | Source URL |
|---|---|
| `vendor/jquery/jquery.min.js` | https://code.jquery.com/jquery-3.7.1.min.js |
| `vendor/jstree/jstree.min.js` | https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.16/jstree.min.js |
| `vendor/jstree/themes/default/style.min.css` | https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.16/themes/default/style.min.css |
| `vendor/jstree/themes/default/throbber.gif` | https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.16/themes/default/throbber.gif |
| `vendor/jstree/themes/default/32px.png` | https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.16/themes/default/32px.png |
| `vendor/jstree/themes/default/40px.png` | https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.16/themes/default/40px.png |
| `vendor/tabulator/js/tabulator.min.js` | https://unpkg.com/tabulator-tables@6.3.0/dist/js/tabulator.min.js |
| `vendor/tabulator/css/tabulator.min.css` | https://unpkg.com/tabulator-tables@6.3.0/dist/css/tabulator.min.css |
| `vendor/split.js/split.min.js` | https://unpkg.com/split.js@1.6.5/dist/split.min.js |

---

## API reference

### `GET api/dictionaries.php`

Returns the list of configured data dictionaries.

```json
[{ "name": "Northwind", "path": "C:\\Data\\northwind.add", "username": "AdsSysAdmin" }]
```

### `POST api/dictionaries.php`

**Add:**
```json
{ "action": "add", "name": "Northwind", "path": "C:\\Data\\northwind.add", "username": "AdsSysAdmin" }
```

**Remove:**
```json
{ "action": "remove", "name": "Northwind" }
```

---

### `POST api/connect.php`

**Connect:**
```json
{ "action": "connect", "name": "Northwind", "path": "C:\\Data\\northwind.add",
  "username": "AdsSysAdmin", "password": "secret" }
```

**Disconnect:**
```json
{ "action": "disconnect", "name": "Northwind" }
```

---

### `GET api/tree.php`

| Parameter | Values |
|---|---|
| `action` | `roots`, `dd_children`, `category_children`, `table_children` |
| `dd` | dictionary name (for all except `roots`) |
| `cat` | category key: `tables`, `views`, `procs`, `triggers`, `users`, `groups`, `ri`, `links` |
| `table` | table name (for `table_children`) |

Returns jsTree-compatible JSON arrays.

---

### `GET api/table_data.php?dd=NAME&table=TABLE`

Returns `{ "data": [...rows] }` with up to 2 000 rows.

---

### `POST api/execute_sql.php`

```json
{ "dd": "Northwind", "sql": "SELECT * FROM Orders" }
```

SELECT response: `{ "columns": [...], "data": [...] }`  
DML response: `{ "affected": true, "message": "Statement executed successfully" }`

---

### `GET api/session_state.php`

Returns `{ "open": ["Northwind", "Inventory"] }` — names of currently-connected dictionaries.

---

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| Blank page | PHP not running | Verify Apache + mod_php, check error log |
| `php_openads extension not loaded` | Extension not in php.ini | Add `extension=php_openads` to php.ini |
| `Not connected` on tree expand | Session expired | Click the DD node to reconnect |
| Tree shows no children | `system.*` tables not accessible | Verify user has SELECT on system catalog |
| Vendor files 404 | `setup.bat` not run | Run `setup.bat` or download manually |
| Table data empty | Table is empty or TOP 2000 filtered all | Expected; Tabulator shows "(no rows)" |
