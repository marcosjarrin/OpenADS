#pragma once

#if defined(OPENADS_WITH_HTTP)

namespace openads::studio {

// studio.web.0.1 — single-page admin UI embedded as a C++ raw
// string literal so the openads_serverd binary stays self-
// contained (no separate `static/` directory to ship).
//
// First cut: vanilla JS + plain DOM, no framework. Two panes:
// left = table list; right = SQL editor + result grid. Connects
// to the REST API on the same origin, no CORS gymnastics.
inline constexpr const char* kSpaIndexHtml = R"OPENADS_SPA(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>OpenADS Studio</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
  body { font-family: -apple-system, system-ui, Segoe UI, Roboto, sans-serif;
         margin: 0; height: 100vh; display: flex; flex-direction: column;
         background: #1e1e1e; color: #ddd; }
  header { background: #0d6efd; color: white; padding: 8px 14px;
           display: flex; align-items: center; justify-content: space-between; }
  header h1 { font-size: 16px; margin: 0; font-weight: 500; }
  header .status { font-size: 12px; opacity: 0.8; }
  main { flex: 1; display: flex; min-height: 0; }
  aside { width: 220px; background: #252526; border-right: 1px solid #333;
          overflow-y: auto; }
  aside h2 { font-size: 11px; text-transform: uppercase; opacity: 0.6;
             margin: 12px 14px 6px; }
  aside ul { list-style: none; padding: 0; margin: 0; }
  aside li { padding: 6px 14px; cursor: pointer; font-size: 13px; }
  aside li:hover { background: #2d2d30; }
  aside li.active { background: #094771; color: white; }
  section.work { flex: 1; display: flex; flex-direction: column; min-width: 0; }
  .editor { background: #1e1e1e; border-bottom: 1px solid #333;
            display: flex; flex-direction: column; }
  .editor textarea { flex: 1; background: #1e1e1e; color: #ddd;
                     border: 0; outline: none; padding: 10px;
                     font: 13px Consolas, Monaco, monospace; resize: none;
                     min-height: 110px; }
  .editor .toolbar { background: #2d2d30; padding: 6px 10px;
                     display: flex; gap: 8px; align-items: center; }
  .editor button { background: #0d6efd; color: white; border: 0;
                   padding: 5px 14px; cursor: pointer; border-radius: 3px;
                   font-size: 12px; }
  .editor button:hover { background: #0b5ed7; }
  .editor .err { color: #f48771; font-size: 12px; margin-left: 10px; }
  .result { flex: 1; overflow: auto; padding: 0; min-height: 0; }
  table { border-collapse: collapse; width: 100%; font-size: 12px; }
  th, td { padding: 5px 10px; border: 1px solid #333; text-align: left;
           white-space: nowrap; }
  th { background: #2d2d30; font-weight: 500; position: sticky; top: 0; }
  .empty { padding: 20px; opacity: 0.5; }
</style>
</head>
<body>
<header>
  <h1>OpenADS Studio</h1>
  <div class="status" id="status">connecting…</div>
</header>
<main>
  <aside>
    <h2>Tables</h2>
    <ul id="tables"></ul>
  </aside>
  <section class="work">
    <div class="editor">
      <div class="toolbar">
        <button id="run">Run</button>
        <span class="err" id="err"></span>
      </div>
      <textarea id="sql"
                placeholder="SELECT * FROM yourtable.dbf"></textarea>
    </div>
    <div class="result" id="result">
      <div class="empty">Click a table on the left or run a query.</div>
    </div>
  </section>
</main>
<script>
const $ = id => document.getElementById(id);
function setStatus(s) { $("status").textContent = s; }
function showErr(s) { $("err").textContent = s || ""; }
async function fetchJson(url, opts) {
  showErr("");
  const r = await fetch(url, opts);
  if (!r.ok) {
    const t = await r.text().catch(() => "");
    throw new Error(`${r.status} ${r.statusText}${t ? ": " + t : ""}`);
  }
  return r.json();
}
function renderTable(rows) {
  const root = $("result");
  if (!rows || !rows.rows || rows.rows.length === 0) {
    root.innerHTML = `<div class="empty">No rows.</div>`;
    return;
  }
  const cols = rows.cols;
  const tbody = rows.rows.map(r =>
    "<tr>" + r.map(v => `<td>${(""+v)
      .replace(/&/g,"&amp;")
      .replace(/</g,"&lt;")}</td>`).join("") + "</tr>").join("");
  root.innerHTML = `<table>
    <thead><tr>${cols.map(c => `<th>${c}</th>`).join("")}</tr></thead>
    <tbody>${tbody}</tbody>
  </table>`;
}
async function loadTables() {
  try {
    const data = await fetchJson("/api/tables");
    setStatus(`${data.tables.length} tables · ${data.data_dir || ""}`);
    $("tables").innerHTML = data.tables.map(t =>
      `<li data-name="${t}">${t}</li>`).join("");
    document.querySelectorAll("aside li").forEach(li =>
      li.addEventListener("click", () => openTable(li.dataset.name)));
  } catch (e) { setStatus("offline"); showErr(e.message); }
}
async function openTable(name) {
  document.querySelectorAll("aside li").forEach(li =>
    li.classList.toggle("active", li.dataset.name === name));
  $("sql").value = `SELECT * FROM ${name}`;
  await runSql();
}
async function runSql() {
  const sql = $("sql").value.trim();
  if (!sql) return;
  try {
    const data = await fetchJson("/api/sql", {
      method: "POST",
      headers: {"content-type": "application/json"},
      body: JSON.stringify({sql, limit: 200})});
    renderTable(data);
  } catch (e) { showErr(e.message); }
}
$("run").addEventListener("click", runSql);
$("sql").addEventListener("keydown", e => {
  if ((e.ctrlKey || e.metaKey) && e.key === "Enter") { runSql(); }
});
loadTables();
</script>
</body>
</html>
)OPENADS_SPA";

} // namespace openads::studio

#endif // OPENADS_WITH_HTTP
