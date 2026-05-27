/* DA-Web — main application JS */
(function () {
  'use strict';

  // ── State ──────────────────────────────────────────────────────────────────
  const state = {
    openConnections: new Set(),
    tabs: [],          // { id, title, type, dd, table }
    activeTabId: null,
    nextTabId: 1,
  };

  // ── Helpers ────────────────────────────────────────────────────────────────
  async function apiFetch(url, options = {}) {
    const res = await fetch(url, options);
    const data = await res.json();
    if (!res.ok) throw new Error(data.error || `HTTP ${res.status}`);
    return data;
  }

  function setStatus(msg, type = 'info') {
    const bar = document.getElementById('status-msg');
    if (bar) bar.textContent = msg;
  }

  function showAlert(container, msg, type = 'error') {
    const el = document.createElement('div');
    el.className = `alert alert-${type}`;
    el.textContent = msg;
    container.prepend(el);
    setTimeout(() => el.remove(), 5000);
  }

  // ── Menu bar ───────────────────────────────────────────────────────────────
  document.querySelectorAll('#menubar .menu-item').forEach(item => {
    item.addEventListener('click', e => {
      const wasActive = item.classList.contains('active');
      document.querySelectorAll('#menubar .menu-item').forEach(i => i.classList.remove('active'));
      if (!wasActive) item.classList.add('active');
      e.stopPropagation();
    });
  });

  document.addEventListener('click', () => {
    document.querySelectorAll('#menubar .menu-item').forEach(i => i.classList.remove('active'));
  });

  // Menu action dispatcher
  document.addEventListener('click', e => {
    const item = e.target.closest('.drop-item[data-action]');
    if (!item) return;
    handleMenuAction(item.dataset.action, item);
  });

  function handleMenuAction(action, el) {
    switch (action) {
      case 'add-dd':    openAddDDModal();  break;
      case 'connect':   openConnectModal(el.dataset.dd); break;
      case 'disconnect':disconnectDD(el.dataset.dd); break;
      case 'open-sql':  openSqlTab();     break;
      case 'refresh-tree': refreshTree(); break;
      case 'about':     openAboutModal(); break;
    }
  }

  // ── Split.js ───────────────────────────────────────────────────────────────
  if (typeof Split !== 'undefined') {
    Split(['#tree-pane', '#content-pane'], {
      sizes: [22, 78],
      minSize: [120, 300],
      gutterSize: 4,
      cursor: 'col-resize',
    });
  }

  // ── jsTree ─────────────────────────────────────────────────────────────────
  function initTree() {
    $('#tree-container').jstree({
      core: {
        themes: { name: 'default', dots: true, icons: true },
        data: function (node, cb) {
          const isRoot = node.id === '#';
          if (isRoot) {
            fetch('api/tree.php?action=roots')
              .then(r => r.json()).then(cb).catch(() => cb([]));
            return;
          }
          const a = node.a_attr || {};
          const dd   = a['data-dd']    || '';
          const type = a['data-type']  || '';
          const cat  = a['data-cat']   || '';
          const tbl  = a['data-table'] || '';

          if (type === 'dd') {
            fetch(`api/tree.php?action=dd_children&dd=${encodeURIComponent(dd)}`)
              .then(r => r.json()).then(cb).catch(() => cb([]));
          } else if (type === 'category') {
            fetch(`api/tree.php?action=category_children&dd=${encodeURIComponent(dd)}&cat=${encodeURIComponent(cat)}`)
              .then(r => r.json()).then(cb).catch(() => cb([]));
          } else if (type === 'table') {
            fetch(`api/tree.php?action=table_children&dd=${encodeURIComponent(dd)}&table=${encodeURIComponent(tbl)}`)
              .then(r => r.json()).then(cb).catch(() => cb([]));
          } else {
            cb([]);
          }
        },
      },
      plugins: ['wholerow', 'types'],
    });

    $('#tree-container').on('select_node.jstree', function (e, data) {
      const a    = data.node.a_attr || {};
      const type = a['data-type']   || '';
      const dd   = a['data-dd']     || '';
      const tbl  = a['data-table']  || '';

      if (type === 'dd') {
        const connected = a['data-connected'] === 'true';
        if (!connected) openConnectModal(dd);
      } else if (type === 'table') {
        openTableTab(dd, tbl);
      }
    });

    $('#tree-container').on('dblclick.jstree', function (e) {
      const node = $(e.target).closest('li');
      if (!node.length) return;
      const treeNode = $.jstree.reference(node);
      if (!treeNode) return;
      const sel = treeNode.get_node(node);
      if (!sel) return;
      const a    = sel.a_attr || {};
      const type = a['data-type'] || '';
      const dd   = a['data-dd']   || '';
      const tbl  = a['data-table'] || '';
      if (type === 'table') openTableTab(dd, tbl);
    });
  }

  function refreshTree() {
    $('#tree-container').jstree(true).refresh();
  }

  // ── Tabs ───────────────────────────────────────────────────────────────────
  function openTableTab(dd, table) {
    const existingId = state.tabs.findIndex(t => t.type === 'table' && t.dd === dd && t.table === table);
    if (existingId !== -1) {
      activateTab(state.tabs[existingId].id);
      return;
    }
    const id = 'tab-' + (state.nextTabId++);
    state.tabs.push({ id, title: `${dd}.${table}`, type: 'table', dd, table });
    renderTabs();
    activateTab(id);
    loadTableData(id, dd, table);
  }

  function openSqlTab(dd = null) {
    const id = 'tab-' + (state.nextTabId++);
    state.tabs.push({ id, title: 'SQL', type: 'sql', dd });
    renderTabs();
    activateTab(id);
  }

  function renderTabs() {
    const bar = document.getElementById('tab-bar');
    const content = document.getElementById('tab-content');

    // Remove existing tab buttons
    bar.querySelectorAll('.tab').forEach(el => el.remove());

    // Remove tab panels that no longer exist
    const activeIds = new Set(state.tabs.map(t => t.id));
    content.querySelectorAll('.tab-panel').forEach(p => {
      if (!activeIds.has(p.dataset.tabId)) p.remove();
    });

    state.tabs.forEach(tab => {
      // Tab button
      const btn = document.createElement('div');
      btn.className = 'tab' + (tab.id === state.activeTabId ? ' active' : '');
      btn.dataset.tabId = tab.id;
      btn.innerHTML = `<span>${escHtml(tab.title)}</span><span class="tab-close" title="Close">×</span>`;
      btn.querySelector('span:first-child').addEventListener('click', () => activateTab(tab.id));
      btn.querySelector('.tab-close').addEventListener('click', e => { e.stopPropagation(); closeTab(tab.id); });
      bar.insertBefore(btn, bar.querySelector('.new-tab-btn'));

      // Create panel if needed
      if (!content.querySelector(`[data-tab-id="${tab.id}"]`)) {
        const panel = document.createElement('div');
        panel.className = 'tab-panel' + (tab.id === state.activeTabId ? ' active' : '');
        panel.dataset.tabId = tab.id;
        if (tab.type === 'table') {
          panel.innerHTML = `<div class="data-panel" id="data-${tab.id}"><div class="alert alert-info">Loading…</div></div>`;
        } else if (tab.type === 'sql') {
          panel.innerHTML = buildSqlPanel(tab.id, tab.dd);
          bindSqlPanel(tab.id, tab);
        }
        content.appendChild(panel);
      }
    });
  }

  function activateTab(id) {
    state.activeTabId = id;
    document.querySelectorAll('.tab').forEach(el => {
      el.classList.toggle('active', el.dataset.tabId === id);
    });
    document.querySelectorAll('.tab-panel').forEach(el => {
      el.classList.toggle('active', el.dataset.tabId === id);
    });
  }

  function closeTab(id) {
    state.tabs = state.tabs.filter(t => t.id !== id);
    if (state.activeTabId === id) {
      state.activeTabId = state.tabs.length ? state.tabs[state.tabs.length - 1].id : null;
    }
    renderTabs();
    if (state.activeTabId) activateTab(state.activeTabId);
  }

  document.getElementById('new-tab-btn').addEventListener('click', () => openSqlTab());

  // ── Table data loading ─────────────────────────────────────────────────────
  function loadTableData(tabId, dd, table) {
    const container = document.getElementById('data-' + tabId);
    if (!container) return;

    fetch(`api/table_data.php?dd=${encodeURIComponent(dd)}&table=${encodeURIComponent(table)}`)
      .then(r => r.json())
      .then(resp => {
        if (resp.error) {
          container.innerHTML = `<div class="alert alert-error">${escHtml(resp.error)}</div>`;
          return;
        }
        container.innerHTML = `<div id="tabulator-${tabId}"></div>`;
        /* global Tabulator */
        new Tabulator('#tabulator-' + tabId, {
          data: resp.data,
          autoColumns: true,
          layout: 'fitDataFill',
          pagination: 'local',
          paginationSize: 50,
          paginationSizeSelector: [25, 50, 100, 200],
          movableColumns: true,
          resizableRows: false,
          placeholder: '(no rows)',
          height: '100%',
        });
      })
      .catch(err => {
        if (container) container.innerHTML = `<div class="alert alert-error">${escHtml(err.message)}</div>`;
      });
  }

  // ── SQL panel ──────────────────────────────────────────────────────────────
  function buildSqlPanel(tabId, dd) {
    const ddOptions = Array.from(state.openConnections).map(n =>
      `<option value="${escAttr(n)}" ${n === dd ? 'selected' : ''}>${escHtml(n)}</option>`
    ).join('');
    return `
      <div class="sql-panel">
        <div id="sql-editor-wrap">
          <textarea id="sql-text-${tabId}" placeholder="SELECT * FROM MyTable" spellcheck="false"></textarea>
        </div>
        <div id="sql-toolbar">
          <select id="sql-dd-${tabId}" style="background:#1e1e2e;color:#cdd6f4;border:1px solid #45475a;padding:3px 8px;border-radius:4px;font-size:12px;">
            <option value="">— database —</option>
            ${ddOptions}
          </select>
          <button class="btn btn-primary" id="sql-run-${tabId}">▶ Execute</button>
          <button class="btn" id="sql-clear-${tabId}">Clear</button>
          <span id="sql-msg-${tabId}" style="font-size:11px;color:#a6adc8;margin-left:8px;"></span>
        </div>
        <div id="sql-results-${tabId}" class="sql-results" style="flex:1;overflow:auto;padding:8px;"></div>
      </div>`;
  }

  function bindSqlPanel(tabId, tab) {
    // bind after DOM insertion
    setTimeout(() => {
      const runBtn   = document.getElementById('sql-run-' + tabId);
      const clearBtn = document.getElementById('sql-clear-' + tabId);
      const msgEl    = document.getElementById('sql-msg-' + tabId);
      const resultsEl = document.getElementById('sql-results-' + tabId);
      if (!runBtn) return;

      clearBtn.addEventListener('click', () => {
        document.getElementById('sql-text-' + tabId).value = '';
        resultsEl.innerHTML = '';
        msgEl.textContent = '';
      });

      runBtn.addEventListener('click', async () => {
        const sql  = (document.getElementById('sql-text-' + tabId)?.value ?? '').trim();
        const dd   = (document.getElementById('sql-dd-' + tabId)?.value ?? '').trim();
        if (!sql)  { msgEl.textContent = 'Enter a SQL statement'; return; }
        if (!dd)   { msgEl.textContent = 'Select a database'; return; }

        runBtn.disabled = true;
        msgEl.textContent = 'Running…';
        resultsEl.innerHTML = '';

        try {
          const resp = await apiFetch('api/execute_sql.php', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ dd, sql }),
          });

          if (resp.columns !== undefined) {
            msgEl.textContent = `${resp.data.length} row(s)`;
            resultsEl.innerHTML = `<div id="sql-tab-${tabId}"></div>`;
            new Tabulator('#sql-tab-' + tabId, {
              data: resp.data,
              columns: resp.columns,
              layout: 'fitDataFill',
              pagination: 'local',
              paginationSize: 50,
              placeholder: '(no rows)',
            });
          } else {
            msgEl.textContent = '';
            resultsEl.innerHTML = `<div class="alert alert-success">${escHtml(resp.message)}</div>`;
          }
        } catch (err) {
          msgEl.textContent = '';
          resultsEl.innerHTML = `<div class="alert alert-error">${escHtml(err.message)}</div>`;
        } finally {
          runBtn.disabled = false;
        }
      });
    }, 50);
  }

  // ── Add DD modal ───────────────────────────────────────────────────────────
  function openAddDDModal() {
    const overlay = document.getElementById('modal-add-dd');
    overlay.classList.add('open');
    document.getElementById('add-dd-name').focus();
  }

  document.getElementById('add-dd-cancel').addEventListener('click', () => {
    document.getElementById('modal-add-dd').classList.remove('open');
  });

  document.getElementById('add-dd-save').addEventListener('click', async () => {
    const name     = document.getElementById('add-dd-name').value.trim();
    const path     = document.getElementById('add-dd-path').value.trim();
    const username = document.getElementById('add-dd-user').value.trim();
    const errEl    = document.getElementById('add-dd-err');
    errEl.textContent = '';

    if (!name || !path) { errEl.textContent = 'Name and path are required'; return; }

    try {
      await apiFetch('api/dictionaries.php', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action: 'add', name, path, username }),
      });
      document.getElementById('modal-add-dd').classList.remove('open');
      document.getElementById('add-dd-name').value = '';
      document.getElementById('add-dd-path').value = '';
      document.getElementById('add-dd-user').value = '';
      refreshTree();
      setStatus(`Added dictionary '${name}'`);
    } catch (err) {
      errEl.textContent = err.message;
    }
  });

  // ── Connect modal ──────────────────────────────────────────────────────────
  function openConnectModal(ddName) {
    const overlay = document.getElementById('modal-connect');
    document.getElementById('connect-dd-name').textContent = ddName || '(unknown)';
    overlay.dataset.dd = ddName;
    document.getElementById('connect-password').value = '';
    document.getElementById('connect-err').textContent = '';

    // Pre-fill username from stored dict config if available
    fetch('api/dictionaries.php')
      .then(r => r.json())
      .then(dicts => {
        const d = dicts.find(x => x.name === ddName);
        if (d) document.getElementById('connect-username').value = d.username || '';
      });

    overlay.classList.add('open');
    setTimeout(() => document.getElementById('connect-password').focus(), 50);
  }

  document.getElementById('connect-cancel').addEventListener('click', () => {
    document.getElementById('modal-connect').classList.remove('open');
  });

  document.getElementById('connect-submit').addEventListener('click', async () => {
    const overlay  = document.getElementById('modal-connect');
    const ddName   = overlay.dataset.dd;
    const username = document.getElementById('connect-username').value.trim();
    const password = document.getElementById('connect-password').value;
    const errEl    = document.getElementById('connect-err');
    errEl.textContent = '';

    // Fetch path from stored config
    try {
      const dicts = await apiFetch('api/dictionaries.php');
      const d = dicts.find(x => x.name === ddName);
      if (!d) { errEl.textContent = 'Dictionary not found in config'; return; }

      await apiFetch('api/connect.php', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action: 'connect', name: ddName, path: d.path, username, password }),
      });

      state.openConnections.add(ddName);
      overlay.classList.remove('open');
      refreshTree();
      setStatus(`Connected to '${ddName}'`);
      updateConnectionCount();
    } catch (err) {
      errEl.textContent = err.message;
    }
  });

  // Enter key submits connect form
  document.getElementById('connect-password').addEventListener('keydown', e => {
    if (e.key === 'Enter') document.getElementById('connect-submit').click();
  });

  // ── Disconnect ─────────────────────────────────────────────────────────────
  async function disconnectDD(ddName) {
    if (!ddName) return;
    try {
      await apiFetch('api/connect.php', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action: 'disconnect', name: ddName }),
      });
      state.openConnections.delete(ddName);
      refreshTree();
      setStatus(`Disconnected from '${ddName}'`);
      updateConnectionCount();
    } catch (err) {
      setStatus(`Disconnect failed: ${err.message}`);
    }
  }

  // ── About modal ────────────────────────────────────────────────────────────
  function openAboutModal() {
    document.getElementById('modal-about').classList.add('open');
  }

  document.getElementById('about-close').addEventListener('click', () => {
    document.getElementById('modal-about').classList.remove('open');
  });

  // ── Connection submenu builder ─────────────────────────────────────────────
  async function buildConnectionMenu() {
    const dicts = await apiFetch('api/dictionaries.php').catch(() => []);
    const sessionState = await apiFetch('api/session_state.php').catch(() => ({ open: [] }));
    const openSet = new Set(sessionState.open);
    openSet.forEach(n => state.openConnections.add(n));

    const connMenu = document.getElementById('connection-submenu');
    if (!connMenu) return;

    connMenu.innerHTML = `<div class="drop-item" data-action="add-dd">Add Data Dictionary…</div>
      <div class="drop-separator"></div>`;

    dicts.forEach(d => {
      const connected = openSet.has(d.name);
      const item = document.createElement('div');
      item.className = 'drop-item';
      item.textContent = (connected ? '● ' : '○ ') + d.name;
      item.dataset.action = connected ? 'disconnect' : 'connect';
      item.dataset.dd = d.name;
      connMenu.appendChild(item);
    });

    if (dicts.length === 0) {
      connMenu.innerHTML += `<div class="drop-item disabled">No dictionaries configured</div>`;
    }

    connMenu.innerHTML += `<div class="drop-separator"></div>
      <div class="drop-item" data-action="refresh-tree">Refresh Tree</div>`;

    updateConnectionCount();
  }

  // ── Status helpers ─────────────────────────────────────────────────────────
  function updateConnectionCount() {
    const el = document.getElementById('status-connections');
    if (el) el.textContent = `${state.openConnections.size} connection(s) open`;
  }

  // ── Close modals on overlay click ──────────────────────────────────────────
  document.querySelectorAll('.modal-overlay').forEach(overlay => {
    overlay.addEventListener('click', e => {
      if (e.target === overlay) overlay.classList.remove('open');
    });
  });

  // ── Context menu ───────────────────────────────────────────────────────────
  const ctxMenu = document.getElementById('ctx-menu');

  document.addEventListener('contextmenu', e => {
    const node = e.target.closest('.jstree-anchor[data-type]');
    if (!node) { ctxMenu.style.display = 'none'; return; }
    e.preventDefault();
    const type  = node.dataset.type  || '';
    const dd    = node.dataset.dd    || '';
    const name  = node.dataset.table || node.dataset.name || dd;
    buildCtxMenu(type, dd, name);
    ctxMenu.style.left = e.pageX + 'px';
    ctxMenu.style.top  = e.pageY + 'px';
    ctxMenu.style.display = 'block';
  });

  document.addEventListener('click', () => { ctxMenu.style.display = 'none'; });

  function buildCtxMenu(type, dd, name) {
    ctxMenu.innerHTML = '';
    const items = [];

    if (type === 'dd') {
      const connected = state.openConnections.has(dd);
      if (connected) {
        items.push({ label: 'Disconnect', action: () => disconnectDD(dd) });
        items.push({ label: 'Open SQL Editor', action: () => openSqlTab(dd) });
      } else {
        items.push({ label: 'Connect…', action: () => openConnectModal(dd) });
      }
      items.push({ label: 'Remove from list', action: () => removeDDFromList(dd) });
    } else if (type === 'table') {
      items.push({ label: 'Open Table', action: () => openTableTab(dd, name) });
    }

    items.forEach(item => {
      const el = document.createElement('div');
      el.className = 'ctx-item';
      el.textContent = item.label;
      el.addEventListener('click', item.action);
      ctxMenu.appendChild(el);
    });
  }

  async function removeDDFromList(ddName) {
    try {
      await apiFetch('api/dictionaries.php', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action: 'remove', name: ddName }),
      });
      state.openConnections.delete(ddName);
      refreshTree();
      setStatus(`Removed '${ddName}' from list`);
      updateConnectionCount();
    } catch (err) {
      setStatus(`Remove failed: ${err.message}`);
    }
  }

  // ── Utility ────────────────────────────────────────────────────────────────
  function escHtml(s) {
    return String(s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;')
      .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  function escAttr(s) { return escHtml(s); }

  // ── Boot ───────────────────────────────────────────────────────────────────
  document.addEventListener('DOMContentLoaded', async () => {
    initTree();
    await buildConnectionMenu();
    setStatus('Ready');

    // Rebuild connection submenu each time Connection menu is opened
    const connMenuItem = document.querySelector('#menubar .menu-item[data-menu="connection"]');
    if (connMenuItem) {
      connMenuItem.addEventListener('click', buildConnectionMenu);
    }
  });

}());
