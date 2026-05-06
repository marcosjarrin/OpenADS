#pragma once

#if defined(OPENADS_WITH_HTTP)

namespace openads::studio {

// studio.web.0.2 — phpMyAdmin-style admin UI:
//
//   - Left tree: list of *.dbf files; click selects a table.
//   - Top tabs (right pane): Browse | Structure | Insert | SQL | Server.
//   - Browse: paginated row grid with edit/delete buttons per row.
//   - Structure: column metadata + record count.
//   - Insert: empty form for a new row.
//   - SQL: free-form editor + result grid.
//   - Server: engine version + listed tables.
//
// All UI state lives in the SPA; the server is stateless. Each
// REST round-trip opens a short-lived ABI connection.

inline constexpr const char kSpaIndexHtml[] = R"OPENADS_SPA(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>OpenADS Studio</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
  * { box-sizing: border-box; }
  body { font-family: -apple-system, system-ui, Segoe UI, Roboto, sans-serif;
         margin: 0; height: 100vh; display: flex; flex-direction: column;
         background: #1e1e1e; color: #ddd; font-size: 16px; }
  header { background: #0d6efd; color: white; padding: 12px 18px;
           display: flex; align-items: center; justify-content: space-between; }
  header h1 { font-size: 20px; margin: 0; font-weight: 500; }
  header .status { font-size: 14px; opacity: 0.85; }
  main { flex: 1; display: flex; min-height: 0; }
  aside { width: 270px; background: #252526; border-right: 1px solid #333;
          overflow-y: auto; flex-shrink: 0; }
  aside h2 { font-size: 13px; text-transform: uppercase; opacity: 0.6;
             margin: 14px 16px 8px; }
  aside ul { list-style: none; padding: 0; margin: 0; }
  aside li { padding: 8px 16px; cursor: pointer; font-size: 15px; }
  aside li:hover { background: #2d2d30; }
  aside li.active { background: #094771; color: white; }
  section.work { flex: 1; display: flex; flex-direction: column; min-width: 0; }
  nav.tabs { background: #2d2d30; padding: 0 10px; display: flex;
             gap: 1px; border-bottom: 1px solid #094771; }
  nav.tabs button { background: #2d2d30; color: #aaa; border: 0;
                    padding: 10px 22px; cursor: pointer; font-size: 14px; }
  nav.tabs button:hover { background: #3a3a3d; color: white; }
  nav.tabs button.active { background: #094771; color: white; }
  .pane { flex: 1; overflow: auto; padding: 16px; min-height: 0; }
  .pane.hidden { display: none; }
  .toolbar { display: flex; gap: 10px; align-items: center;
             margin-bottom: 12px; flex-wrap: wrap; }
  .toolbar button, .btn { background: #0d6efd; color: white; border: 0;
                          padding: 7px 18px; cursor: pointer;
                          border-radius: 3px; font-size: 14px; }
  .toolbar button:hover, .btn:hover { background: #0b5ed7; }
  .btn-danger { background: #d9534f; }
  .btn-danger:hover { background: #c9302c; }
  .btn-secondary { background: #444; }
  .btn-secondary:hover { background: #555; }
  .err { color: #f48771; font-size: 14px; }
  .ok  { color: #6cc24a; font-size: 14px; }
  table { border-collapse: collapse; width: 100%; }
  th, td { padding: 6px 10px; border: 1px solid #333; text-align: left;
           white-space: nowrap; vertical-align: top; font-size: 14px; }
  th { background: #2d2d30; font-weight: 500; position: sticky; top: 0;
       z-index: 1; }
  tr.deleted td { opacity: 0.4; text-decoration: line-through; }
  .editor textarea { background: #1e1e1e; color: #ddd; border: 1px solid #333;
                     outline: none; padding: 12px;
                     font: 15px Consolas, Monaco, monospace; resize: vertical;
                     width: 100%; min-height: 140px; }
  .form-row { display: flex; align-items: center; margin-bottom: 8px;
              gap: 12px; }
  .form-row label { width: 170px; font-size: 14px; opacity: 0.85; }
  .form-row input { flex: 1; background: #2d2d30; color: #ddd;
                    border: 1px solid #444; padding: 6px 10px;
                    font: 14px Consolas, monospace; border-radius: 2px; }
  .empty { padding: 24px; opacity: 0.5; font-size: 15px; }
  .pager { margin-top: 12px; display: flex; gap: 10px; align-items: center; }
  .pager span { font-size: 14px; opacity: 0.85; }
  .kv { display: grid; grid-template-columns: 170px 1fr; gap: 6px 18px;
        font-size: 14px; max-width: 700px; }
  .kv > div:nth-child(odd) { opacity: 0.7; }
</style>
</head>
<body>
<header>
  <h1>OpenADS Studio</h1>
  <div class="status" id="status">…</div>
</header>
<main>
  <aside>
    <h2>Tables</h2>
    <ul id="tables"></ul>
    <h2>Server</h2>
    <ul><li id="server-link">Info</li></ul>
  </aside>
  <section class="work">
    <nav class="tabs" id="tabs">
      <button data-tab="browse" class="active">Browse</button>
      <button data-tab="structure">Structure</button>
      <button data-tab="insert">Insert</button>
      <button data-tab="sql">SQL</button>
      <button data-tab="server">Server</button>
    </nav>

    <div id="pane-browse" class="pane">
      <div class="toolbar">
        <span id="browse-title" class="err"></span>
      </div>
      <div id="browse-grid" class="empty">Select a table on the left.</div>
      <div class="pager" id="browse-pager"></div>
    </div>

    <div id="pane-structure" class="pane hidden">
      <div id="structure-body" class="empty">Select a table.</div>
    </div>

    <div id="pane-insert" class="pane hidden">
      <div id="insert-body" class="empty">Select a table.</div>
    </div>

    <div id="pane-sql" class="pane hidden">
      <div class="editor">
        <textarea id="sql"
                  placeholder="SELECT * FROM yourtable.dbf"></textarea>
      </div>
      <div class="toolbar">
        <button id="sql-run">Run (Ctrl+Enter)</button>
        <button id="sql-export" class="btn-secondary">Export CSV</button>
        <span id="sql-status"></span>
      </div>
      <div id="sql-result" class="empty">Result will appear here.</div>
    </div>

    <div id="pane-server" class="pane hidden">
      <div id="server-body" class="empty">Loading…</div>
    </div>
  </section>
</main>
)OPENADS_SPA"
R"OPENADS_SPA(<script>
const $ = id => document.getElementById(id);
const esc = s => (""+s)
  .replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;")
  .replace(/"/g,"&quot;");

let state = { table: null, schema: null,
              browseOffset: 0, browseLimit: 50 };

function setStatus(s, klass) {
  const el = $("status"); el.textContent = s;
  el.className = "status" + (klass ? " " + klass : "");
}
async function api(url, opts) {
  const r = await fetch(url, opts);
  const txt = await r.text();
  let body = null;
  try { body = JSON.parse(txt); } catch {}
  if (!r.ok || (body && body.error)) {
    throw new Error((body && body.error) || `${r.status} ${r.statusText}`);
  }
  return body;
}

function showTab(tab) {
  ["browse","structure","insert","sql","server"].forEach(t => {
    $("pane-" + t).classList.toggle("hidden", t !== tab);
  });
  document.querySelectorAll("nav.tabs button").forEach(b =>
    b.classList.toggle("active", b.dataset.tab === tab));
  if (tab === "structure" && state.table) loadStructure();
  if (tab === "insert"    && state.table) loadInsertForm();
  if (tab === "browse"    && state.table) loadBrowse();
  if (tab === "server")                   loadServerInfo();
}

async function loadTables() {
  try {
    const data = await api("/api/tables");
    setStatus(`${data.tables.length} tables · ${data.data_dir}`);
    $("tables").innerHTML = data.tables.map(t =>
      `<li data-name="${esc(t)}">${esc(t)}</li>`).join("");
    document.querySelectorAll("aside li[data-name]").forEach(li =>
      li.addEventListener("click", () => selectTable(li.dataset.name)));
  } catch (e) { setStatus("offline: " + e.message); }
}

function selectTable(name) {
  state.table = name;
  state.browseOffset = 0;
  document.querySelectorAll("aside li").forEach(li =>
    li.classList.toggle("active", li.dataset.name === name));
  showTab("browse");
}

async function loadBrowse() {
  const t = state.table;
  $("browse-title").textContent = `Table: ${t}`;
  try {
    const data = await api(`/api/tables/${encodeURIComponent(t)}` +
      `/rows?offset=${state.browseOffset}&limit=${state.browseLimit}`);
    if (!data.rows || data.rows.length === 0) {
      $("browse-grid").innerHTML = `<div class="empty">No rows.</div>`;
    } else {
      const cols = data.cols;
      const tbody = data.rows.map(r => {
        const meta = r[0];
        const cells = r.slice(1);
        const cls = meta._deleted ? "deleted" : "";
        return `<tr class="${cls}">
          <td>${meta._recno}</td>
          ${cells.map(v => `<td>${esc(v)}</td>`).join("")}
          <td><button class="btn btn-secondary" data-edit="${meta._recno}">Edit</button>
              <button class="btn btn-danger" data-delete="${meta._recno}"
                      data-deleted="${meta._deleted}">${
                meta._deleted ? "Recall" : "Delete"
              }</button></td>
        </tr>`;
      }).join("");
      $("browse-grid").innerHTML = `<table>
        <thead><tr><th>recno</th>${cols.map(c => `<th>${esc(c)}</th>`).join("")}<th>actions</th></tr></thead>
        <tbody>${tbody}</tbody>
      </table>`;
      document.querySelectorAll("[data-edit]").forEach(b =>
        b.addEventListener("click", () => editRow(+b.dataset.edit)));
      document.querySelectorAll("[data-delete]").forEach(b =>
        b.addEventListener("click", () =>
          deleteRow(+b.dataset.delete, b.dataset.deleted === "true")));
    }
    $("browse-pager").innerHTML = `
      <button class="btn btn-secondary" id="prev"
              ${state.browseOffset === 0 ? "disabled" : ""}>‹ Prev</button>
      <span>${state.browseOffset + 1}–${state.browseOffset + data.rows.length}
            of ${data.total}</span>
      <button class="btn btn-secondary" id="next"
              ${state.browseOffset + data.rows.length >= data.total ?
                "disabled" : ""}>Next ›</button>`;
    $("prev")?.addEventListener("click", () => {
      state.browseOffset = Math.max(0, state.browseOffset - state.browseLimit);
      loadBrowse();
    });
    $("next")?.addEventListener("click", () => {
      state.browseOffset += state.browseLimit; loadBrowse();
    });
  } catch (e) {
    $("browse-grid").innerHTML = `<div class="err">${esc(e.message)}</div>`;
  }
}

async function ensureSchema() {
  if (state.schema && state.schema.table === state.table) return state.schema;
  state.schema = await api(
    `/api/tables/${encodeURIComponent(state.table)}/schema`);
  return state.schema;
}

async function loadStructure() {
  try {
    const s = await ensureSchema();
    $("structure-body").innerHTML = `
      <div class="kv">
        <div>Table</div><div>${esc(s.table)}</div>
        <div>Records</div><div>${s.record_count}</div>
        <div>File size</div><div>${s.file_bytes} bytes</div>
      </div>
      <h3 style="margin-top:18px;font-size:13px;opacity:0.85">Columns</h3>
      <table><thead><tr><th>#</th><th>Name</th><th>Type</th><th>Length</th><th>Decimals</th></tr></thead>
      <tbody>${s.columns.map((c,i) => `<tr>
        <td>${i+1}</td><td>${esc(c.name)}</td><td>${c.type}</td>
        <td>${c.length}</td><td>${c.decimals}</td></tr>`).join("")}</tbody></table>`;
  } catch (e) {
    $("structure-body").innerHTML = `<div class="err">${esc(e.message)}</div>`;
  }
}

async function loadInsertForm() {
  try {
    const s = await ensureSchema();
    const body = `<form id="insert-form">
      ${s.columns.map(c =>
        `<div class="form-row">
          <label>${esc(c.name)} <small>(${c.type}/${c.length})</small></label>
          <input name="${esc(c.name)}" type="text">
        </div>`).join("")}
      <div class="toolbar" style="margin-top:14px">
        <button type="submit" class="btn">Append record</button>
        <span id="insert-status"></span>
      </div></form>`;
    $("insert-body").innerHTML = body;
    $("insert-form").addEventListener("submit", async e => {
      e.preventDefault();
      const fd = new FormData(e.target);
      const obj = {};
      for (const [k,v] of fd.entries()) obj[k] = v;
      try {
        const r = await api(
          `/api/tables/${encodeURIComponent(state.table)}/insert`,
          { method: "POST",
            headers: {"content-type":"application/json"},
            body: JSON.stringify(obj) });
        $("insert-status").textContent = `appended recno ${r.recno}`;
        $("insert-status").className = "ok";
        e.target.reset();
        state.schema = null;
      } catch (err) {
        $("insert-status").textContent = err.message;
        $("insert-status").className = "err";
      }
    });
  } catch (e) {
    $("insert-body").innerHTML = `<div class="err">${esc(e.message)}</div>`;
  }
}

async function editRow(recno) {
  try {
    const s = await ensureSchema();
    const data = await api(
      `/api/tables/${encodeURIComponent(state.table)}` +
      `/rows?offset=${recno - 1}&limit=1`);
    const row = data.rows[0];
    const cells = row.slice(1);
    const lines = s.columns.map((c, i) =>
      `<div class="form-row">
        <label>${esc(c.name)} <small>(${c.type}/${c.length})</small></label>
        <input name="${esc(c.name)}" type="text"
               value="${esc(cells[i] || "")}"></div>`).join("");
    const html = `<h3 style="font-size:13px;margin:0 0 10px">
      Edit recno ${recno} of ${esc(state.table)}</h3>
      <form id="edit-form">${lines}
      <div class="toolbar" style="margin-top:14px">
        <button type="submit" class="btn">Save</button>
        <button type="button" class="btn btn-secondary" id="edit-cancel">Cancel</button>
        <span id="edit-status"></span>
      </div></form>`;
    $("browse-grid").innerHTML = html;
    $("edit-cancel").addEventListener("click", loadBrowse);
    $("edit-form").addEventListener("submit", async e => {
      e.preventDefault();
      const fd = new FormData(e.target);
      const obj = {};
      for (const [k,v] of fd.entries()) obj[k] = v;
      try {
        await api(`/api/tables/${encodeURIComponent(state.table)}` +
          `/update?recno=${recno}`,
          { method: "POST",
            headers: {"content-type":"application/json"},
            body: JSON.stringify(obj) });
        loadBrowse();
      } catch (err) {
        $("edit-status").textContent = err.message;
        $("edit-status").className = "err";
      }
    });
  } catch (e) {
    $("browse-grid").innerHTML = `<div class="err">${esc(e.message)}</div>`;
  }
}

async function deleteRow(recno, currentlyDeleted) {
  const verb = currentlyDeleted ? "recall" : "delete";
  if (!confirm(`${verb} recno ${recno}?`)) return;
  try {
    await api(`/api/tables/${encodeURIComponent(state.table)}` +
      `/delete?recno=${recno}&recall=${currentlyDeleted ? "1" : "0"}`,
      { method: "POST" });
    loadBrowse();
  } catch (e) { alert(e.message); }
}

async function runSql() {
  const sql = $("sql").value.trim();
  if (!sql) return;
  $("sql-status").textContent = "running…"; $("sql-status").className = "";
  try {
    const data = await api("/api/sql", {
      method: "POST",
      headers: {"content-type": "application/json"},
      body: JSON.stringify({sql, limit: 500})});
    if (!data.rows || data.rows.length === 0) {
      $("sql-result").innerHTML = `<div class="empty">no rows / OK</div>`;
    } else {
      $("sql-result").innerHTML = `<table>
        <thead><tr>${data.cols.map(c => `<th>${esc(c)}</th>`).join("")}</tr></thead>
        <tbody>${data.rows.map(r => `<tr>${
          r.map(v => `<td>${esc(v)}</td>`).join("")}</tr>`).join("")}</tbody></table>`;
    }
    $("sql-status").textContent = `${data.rows_returned} rows`;
    $("sql-status").className = "ok";
    state.lastSqlResult = data;
  } catch (e) {
    $("sql-status").textContent = e.message;
    $("sql-status").className = "err";
  }
}

function exportCsv() {
  const data = state.lastSqlResult;
  if (!data || !data.rows) return alert("Run a query first.");
  const csv = [data.cols.join(","),
    ...data.rows.map(r => r.map(v => {
      const s = (""+v).replace(/"/g,'""');
      return /[",\n]/.test(s) ? `"${s}"` : s;
    }).join(","))].join("\n");
  const blob = new Blob([csv], {type:"text/csv"});
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "openads-result.csv"; a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}

async function loadServerInfo() {
  try {
    const data = await api("/api/server/info");
    $("server-body").innerHTML = `<div class="kv">
      <div>Engine</div><div>${esc(data.engine)} ${esc(data.version || "")}</div>
      <div>HTTP module</div><div>${esc(data.http || "")}</div>
      <div>Server name</div><div>${esc(data.server_name || "")}</div>
      <div>Data dir</div><div>${esc(data.data_dir)}</div>
      <div>Tables</div><div>${data.tables.length}</div>
      </div>`;
  } catch (e) {
    $("server-body").innerHTML = `<div class="err">${esc(e.message)}</div>`;
  }
}

document.querySelectorAll("nav.tabs button").forEach(b =>
  b.addEventListener("click", () => showTab(b.dataset.tab)));
$("server-link").addEventListener("click", () => showTab("server"));
$("sql-run").addEventListener("click", runSql);
$("sql-export").addEventListener("click", exportCsv);
$("sql").addEventListener("keydown", e => {
  if ((e.ctrlKey || e.metaKey) && e.key === "Enter") runSql();
});
loadTables();
</script>
</body>
</html>
)OPENADS_SPA";

} // namespace openads::studio

#endif // OPENADS_WITH_HTTP
