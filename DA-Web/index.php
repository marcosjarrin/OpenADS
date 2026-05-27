<?php
/**
 * DA-Web — Data Architect for OpenADS
 * Main shell: renders the full-page application frame.
 */
?>
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>DA-Web — OpenADS Data Architect</title>

  <!-- Local vendor: jQuery (required by jsTree) -->
  <script src="vendor/jquery/jquery.min.js"></script>

  <!-- Local vendor: jsTree -->
  <link rel="stylesheet" href="vendor/jstree/themes/default/style.min.css">
  <script src="vendor/jstree/jstree.min.js"></script>

  <!-- Local vendor: Tabulator -->
  <link rel="stylesheet" href="vendor/tabulator/css/tabulator.min.css">
  <script src="vendor/tabulator/js/tabulator.min.js"></script>

  <!-- Local vendor: Split.js -->
  <script src="vendor/split.js/split.min.js"></script>

  <!-- Application styles -->
  <link rel="stylesheet" href="css/app.css">
</head>
<body>
<div id="app">

  <!-- ── Menu bar ─────────────────────────────────────────────────────────── -->
  <div id="menubar">

    <div class="menu-item" data-menu="file">
      File
      <div class="menu-dropdown">
        <div class="drop-item" data-action="open-sql">New SQL Tab</div>
        <div class="drop-separator"></div>
        <div class="drop-item" data-action="exit">Exit</div>
      </div>
    </div>

    <div class="menu-item" data-menu="connection">
      Connection
      <div class="menu-dropdown" id="connection-submenu">
        <div class="drop-item" data-action="add-dd">Add Data Dictionary…</div>
        <div class="drop-separator"></div>
        <div class="drop-item disabled">Loading…</div>
      </div>
    </div>

    <div class="menu-item" data-menu="tools">
      Tools
      <div class="menu-dropdown">
        <div class="drop-item" data-action="refresh-tree">Refresh Tree</div>
      </div>
    </div>

    <div class="menu-item" data-menu="sql">
      SQL
      <div class="menu-dropdown">
        <div class="drop-item" data-action="open-sql">Open SQL Editor</div>
      </div>
    </div>

    <div class="menu-item" data-menu="window">
      Window
      <div class="menu-dropdown">
        <div class="drop-item disabled" id="window-list-placeholder">(no open tabs)</div>
      </div>
    </div>

    <div class="menu-item" data-menu="help">
      Help
      <div class="menu-dropdown">
        <div class="drop-item" data-action="about">About DA-Web</div>
      </div>
    </div>

  </div><!-- #menubar -->

  <!-- ── Workspace ────────────────────────────────────────────────────────── -->
  <div id="workspace">

    <!-- Tree pane -->
    <div id="tree-pane">
      <div class="pane-header">Data Dictionaries</div>
      <div id="tree-container"></div>
    </div>

    <!-- Content pane -->
    <div id="content-pane">

      <!-- Tab bar -->
      <div id="tab-bar">
        <div class="new-tab-btn" id="new-tab-btn" title="New SQL Tab">+</div>
      </div>

      <!-- Tab panels -->
      <div id="tab-content">
        <!-- Panels are injected dynamically by app.js -->
        <div style="display:flex;align-items:center;justify-content:center;height:100%;color:#45475a;font-size:20px;">
          Select a table from the tree to open it, or press + for a SQL editor.
        </div>
      </div>

    </div><!-- #content-pane -->

  </div><!-- #workspace -->

  <!-- ── Status bar ───────────────────────────────────────────────────────── -->
  <div id="statusbar">
    <span id="status-msg">Ready</span>
    <span style="flex:1"></span>
    <span id="status-connections">0 connection(s) open</span>
  </div>

</div><!-- #app -->

<!-- ── Context menu ──────────────────────────────────────────────────────── -->
<div id="ctx-menu"></div>

<!-- ── Modal: Add Data Dictionary ───────────────────────────────────────── -->
<div class="modal-overlay" id="modal-add-dd" role="dialog" aria-modal="true" aria-labelledby="add-dd-title">
  <div class="modal">
    <div class="modal-header">
      <span id="add-dd-title">Add Data Dictionary</span>
      <span class="modal-close" onclick="document.getElementById('modal-add-dd').classList.remove('open')">&times;</span>
    </div>
    <div class="modal-body">
      <div id="add-dd-err" style="color:#f38ba8;font-size:12px;min-height:16px;margin-bottom:6px;"></div>
      <div class="form-group">
        <label for="add-dd-name">Name <span style="color:#f38ba8">*</span></label>
        <input type="text" id="add-dd-name" placeholder="e.g. Northwind" autocomplete="off">
      </div>
      <div class="form-group">
        <label for="add-dd-path">Path to .add / connection string <span style="color:#f38ba8">*</span></label>
        <input type="text" id="add-dd-path" placeholder="C:\Data\mydict.add" autocomplete="off">
      </div>
      <div class="form-group">
        <label for="add-dd-user">Default username</label>
        <input type="text" id="add-dd-user" placeholder="AdsSysAdmin" autocomplete="off">
      </div>
    </div>
    <div class="modal-footer">
      <button class="btn" id="add-dd-cancel">Cancel</button>
      <button class="btn btn-primary" id="add-dd-save">Add</button>
    </div>
  </div>
</div>

<!-- ── Modal: Connect ────────────────────────────────────────────────────── -->
<div class="modal-overlay" id="modal-connect" role="dialog" aria-modal="true" aria-labelledby="connect-title">
  <div class="modal">
    <div class="modal-header">
      <span id="connect-title">Connect to <span id="connect-dd-name"></span></span>
      <span class="modal-close" onclick="document.getElementById('modal-connect').classList.remove('open')">&times;</span>
    </div>
    <div class="modal-body">
      <div id="connect-err" style="color:#f38ba8;font-size:12px;min-height:16px;margin-bottom:6px;"></div>
      <div class="form-group">
        <label for="connect-username">Username</label>
        <input type="text" id="connect-username" value="AdsSysAdmin" autocomplete="off">
      </div>
      <div class="form-group">
        <label for="connect-password">Password</label>
        <input type="password" id="connect-password" autocomplete="current-password">
      </div>
    </div>
    <div class="modal-footer">
      <button class="btn" id="connect-cancel">Cancel</button>
      <button class="btn btn-primary" id="connect-submit">Connect</button>
    </div>
  </div>
</div>

<!-- ── Modal: About ──────────────────────────────────────────────────────── -->
<div class="modal-overlay" id="modal-about" role="dialog" aria-modal="true">
  <div class="modal">
    <div class="modal-header">
      <span>About DA-Web</span>
      <span class="modal-close" id="about-close">&times;</span>
    </div>
    <div class="modal-body" style="line-height:1.7;font-size:13px;">
      <p><strong>DA-Web</strong> — OpenADS Data Architect</p>
      <p style="margin-top:8px;color:#a6adc8;">Web-based replacement for SAP Data Architect.<br>
      Backend: PHP + php_openads native extension.<br>
      Frontend: jsTree · Tabulator · Split.js · jQuery.</p>
      <p style="margin-top:12px;color:#45475a;font-size:11px;">
        Part of the <a href="https://github.com/FiveTechSoft/OpenADS" style="color:#89b4fa">OpenADS</a> project.
      </p>
    </div>
    <div class="modal-footer">
      <button class="btn btn-primary" id="about-ok" onclick="document.getElementById('modal-about').classList.remove('open')">OK</button>
    </div>
  </div>
</div>

<!-- Application JS (loaded last so DOM is ready) -->
<script src="js/app.js"></script>
</body>
</html>
