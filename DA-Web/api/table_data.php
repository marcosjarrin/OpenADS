<?php
/**
 * api/table_data.php — return up to 2000 rows from a table for Tabulator
 * GET  dd=<name>&table=<tablename>
 */
header('Content-Type: application/json');
session_start();

$ddName = trim($_GET['dd']    ?? '');
$table  = trim($_GET['table'] ?? '');

if ($ddName === '' || $table === '') {
    http_response_code(400);
    echo json_encode(['error' => 'dd and table are required']);
    exit;
}

if (!isset($_SESSION['connections'][$ddName])) {
    http_response_code(401);
    echo json_encode(['error' => "Not connected to '$ddName'"]);
    exit;
}

$c = $_SESSION['connections'][$ddName];

// Whitelist table name to alphanumeric + underscore to prevent SQL injection
if (!preg_match('/^[A-Za-z_][A-Za-z0-9_]*$/', $table)) {
    http_response_code(400);
    echo json_encode(['error' => 'invalid table name']);
    exit;
}

try {
    $opts = ['path' => $c['path']];
    if (($c['username'] ?? '') !== '') $opts['user']     = $c['username'];
    if (($c['password'] ?? '') !== '') $opts['password'] = $c['password'];
    $conn = AdsConnection::connect($opts);

    $stmt = $conn->query("SELECT * FROM " . $table . " LIMIT 2000");
    $rows = $stmt->fetchAll();
    $stmt->close();
    $conn->close();

    $out = json_encode(['data' => $rows],
                       JSON_INVALID_UTF8_SUBSTITUTE | JSON_PARTIAL_OUTPUT_ON_ERROR);
    if ($out === false) {
        http_response_code(500);
        echo json_encode(['error' => 'JSON encoding failed: ' . json_last_error_msg()]);
    } else {
        echo $out;
    }
} catch (Throwable $e) {
    http_response_code(500);
    echo json_encode(['error' => $e->getMessage()]);
}
