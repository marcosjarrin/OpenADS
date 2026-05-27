<?php
/**
 * api/tree.php — jsTree lazy-load provider
 *
 * Actions:
 *   roots               → all configured DDs (connected/disconnected)
 *   dd_children         → category nodes for a connected DD
 *   category_children   → leaf nodes for a category (tables, users, etc.)
 *   table_children      → table sub-nodes (fields, indexes)
 */
header('Content-Type: application/json');
session_start();

$configFile = __DIR__ . '/../config/dictionaries.json';

function loadDicts(string $file): array {
    if (!file_exists($file)) return [];
    return json_decode(file_get_contents($file), true) ?? [];
}

function getConn(string $ddName): ?AdsConnection {
    if (!isset($_SESSION['connections'][$ddName])) return null;
    $c = $_SESSION['connections'][$ddName];
    $conn = new AdsConnection();
    $conn->connect($c['path'], $c['username'], $c['password']);
    return $conn;
}

$action = $_GET['action'] ?? '';
$ddName = $_GET['dd']       ?? '';
$cat    = $_GET['cat']      ?? '';
$table  = $_GET['table']    ?? '';

// ─── roots ───────────────────────────────────────────────────────────────────
if ($action === 'roots') {
    $dicts = loadDicts($configFile);
    $open  = array_keys($_SESSION['connections'] ?? []);
    $nodes = [];
    foreach ($dicts as $d) {
        $connected = in_array($d['name'], $open, true);
        $nodes[] = [
            'id'       => 'dd_' . $d['name'],
            'text'     => $d['name'],
            'icon'     => $connected ? 'jstree-icon-dd-open' : 'jstree-icon-dd-closed',
            'children' => $connected,
            'state'    => ['opened' => false],
            'li_attr'  => ['data-dd' => $d['name'], 'data-type' => 'dd'],
            'a_attr'   => ['data-dd' => $d['name'], 'data-type' => 'dd',
                           'data-connected' => $connected ? 'true' : 'false'],
        ];
    }
    echo json_encode($nodes);
    exit;
}

// ─── dd_children ─────────────────────────────────────────────────────────────
if ($action === 'dd_children') {
    if ($ddName === '') { echo json_encode([]); exit; }
    $categories = [
        ['id' => "cat_{$ddName}_tables",     'text' => 'Tables',            'icon' => 'jstree-icon-tables'],
        ['id' => "cat_{$ddName}_views",      'text' => 'Views',             'icon' => 'jstree-icon-views'],
        ['id' => "cat_{$ddName}_procs",      'text' => 'Stored Procedures', 'icon' => 'jstree-icon-procs'],
        ['id' => "cat_{$ddName}_triggers",   'text' => 'Triggers',          'icon' => 'jstree-icon-triggers'],
        ['id' => "cat_{$ddName}_users",      'text' => 'Users',             'icon' => 'jstree-icon-users'],
        ['id' => "cat_{$ddName}_groups",     'text' => 'Groups',            'icon' => 'jstree-icon-groups'],
        ['id' => "cat_{$ddName}_ri",         'text' => 'RI Objects',        'icon' => 'jstree-icon-ri'],
        ['id' => "cat_{$ddName}_links",      'text' => 'Links',             'icon' => 'jstree-icon-links'],
    ];
    foreach ($categories as &$c) {
        $c['children'] = true;
        $c['a_attr']   = ['data-dd' => $ddName, 'data-type' => 'category',
                          'data-cat' => substr($c['id'], strlen("cat_{$ddName}_"))];
    }
    unset($c);
    echo json_encode($categories);
    exit;
}

// ─── category_children ───────────────────────────────────────────────────────
if ($action === 'category_children') {
    if ($ddName === '' || $cat === '') { echo json_encode([]); exit; }

    $conn = getConn($ddName);
    if ($conn === null) { echo json_encode([]); exit; }

    $nodes = [];
    try {
        switch ($cat) {
            case 'tables':
                $stmt = $conn->query("SELECT Name FROM system.tables ORDER BY Name");
                while ($row = $stmt->fetchAssoc()) {
                    $t = $row['Name'];
                    $nodes[] = [
                        'id'      => "tbl_{$ddName}_{$t}",
                        'text'    => $t,
                        'icon'    => 'jstree-icon-table',
                        'children'=> true,
                        'a_attr'  => ['data-dd' => $ddName, 'data-type' => 'table', 'data-table' => $t],
                    ];
                }
                break;

            case 'views':
                $stmt = $conn->query("SELECT Name FROM system.views ORDER BY Name");
                while ($row = $stmt->fetchAssoc()) {
                    $v = $row['Name'];
                    $nodes[] = ['id' => "view_{$ddName}_{$v}", 'text' => $v,
                                'icon' => 'jstree-icon-view', 'children' => false,
                                'a_attr' => ['data-dd' => $ddName, 'data-type' => 'view', 'data-name' => $v]];
                }
                break;

            case 'procs':
                $stmt = $conn->query("SELECT Name FROM system.storedprocedures ORDER BY Name");
                while ($row = $stmt->fetchAssoc()) {
                    $p = $row['Name'];
                    $nodes[] = ['id' => "proc_{$ddName}_{$p}", 'text' => $p,
                                'icon' => 'jstree-icon-proc', 'children' => false,
                                'a_attr' => ['data-dd' => $ddName, 'data-type' => 'proc', 'data-name' => $p]];
                }
                break;

            case 'triggers':
                $stmt = $conn->query("SELECT Name FROM system.triggers ORDER BY Name");
                while ($row = $stmt->fetchAssoc()) {
                    $tr = $row['Name'];
                    $nodes[] = ['id' => "trg_{$ddName}_{$tr}", 'text' => $tr,
                                'icon' => 'jstree-icon-trigger', 'children' => false,
                                'a_attr' => ['data-dd' => $ddName, 'data-type' => 'trigger', 'data-name' => $tr]];
                }
                break;

            case 'users':
                $stmt = $conn->query("SELECT Name FROM system.users ORDER BY Name");
                while ($row = $stmt->fetchAssoc()) {
                    $u = $row['Name'];
                    $nodes[] = ['id' => "usr_{$ddName}_{$u}", 'text' => $u,
                                'icon' => 'jstree-icon-user', 'children' => false,
                                'a_attr' => ['data-dd' => $ddName, 'data-type' => 'user', 'data-name' => $u]];
                }
                break;

            case 'groups':
                $stmt = $conn->query("SELECT Name FROM system.usergroups ORDER BY Name");
                while ($row = $stmt->fetchAssoc()) {
                    $g = $row['Name'];
                    $nodes[] = ['id' => "grp_{$ddName}_{$g}", 'text' => $g,
                                'icon' => 'jstree-icon-group', 'children' => false,
                                'a_attr' => ['data-dd' => $ddName, 'data-type' => 'group', 'data-name' => $g]];
                }
                break;

            case 'ri':
                $stmt = $conn->query("SELECT Name FROM system.referentialintegrity ORDER BY Name");
                while ($row = $stmt->fetchAssoc()) {
                    $r = $row['Name'];
                    $nodes[] = ['id' => "ri_{$ddName}_{$r}", 'text' => $r,
                                'icon' => 'jstree-icon-ri', 'children' => false,
                                'a_attr' => ['data-dd' => $ddName, 'data-type' => 'ri', 'data-name' => $r]];
                }
                break;

            case 'links':
                $stmt = $conn->query("SELECT Name FROM system.links ORDER BY Name");
                while ($row = $stmt->fetchAssoc()) {
                    $l = $row['Name'];
                    $nodes[] = ['id' => "lnk_{$ddName}_{$l}", 'text' => $l,
                                'icon' => 'jstree-icon-link', 'children' => false,
                                'a_attr' => ['data-dd' => $ddName, 'data-type' => 'link', 'data-name' => $l]];
                }
                break;
        }
    } catch (AdsException $e) {
        // return empty on error — tree just shows no children
    } finally {
        $conn->close();
    }

    echo json_encode($nodes);
    exit;
}

// ─── table_children ──────────────────────────────────────────────────────────
if ($action === 'table_children') {
    if ($ddName === '' || $table === '') { echo json_encode([]); exit; }

    $conn = getConn($ddName);
    if ($conn === null) { echo json_encode([]); exit; }

    $nodes = [];
    try {
        // Fields sub-node
        $nodes[] = [
            'id'       => "fields_{$ddName}_{$table}",
            'text'     => 'Fields',
            'icon'     => 'jstree-icon-fields',
            'children' => true,
            'a_attr'   => ['data-dd' => $ddName, 'data-type' => 'fields', 'data-table' => $table],
        ];
        // Indexes sub-node
        $nodes[] = [
            'id'       => "indexes_{$ddName}_{$table}",
            'text'     => 'Indexes',
            'icon'     => 'jstree-icon-indexes',
            'children' => true,
            'a_attr'   => ['data-dd' => $ddName, 'data-type' => 'indexes', 'data-table' => $table],
        ];
    } finally {
        $conn->close();
    }

    echo json_encode($nodes);
    exit;
}

echo json_encode([]);
