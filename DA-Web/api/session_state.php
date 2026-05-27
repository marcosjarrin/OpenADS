<?php
/**
 * api/session_state.php — return names of currently open DD connections
 */
header('Content-Type: application/json');
session_start();

$open = array_keys($_SESSION['connections'] ?? []);
echo json_encode(['open' => $open]);
