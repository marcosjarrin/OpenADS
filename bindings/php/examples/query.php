<?php
declare(strict_types=1);

require __DIR__ . '/../vendor/autoload.php';

use OpenADS\Connection;

// Local: pass a data-directory path. Remote: 'tcp://host:port/dir'.
$conn = new Connection($argv[1] ?? __DIR__ . '/data');
$stmt = $conn->statement();

$cursor = $stmt->query(
    'SELECT id, name FROM people WHERE id > :min',
    [':min' => 0]
);

foreach ($cursor as $row) {
    printf("%d  %s\n", $row['ID'], trim((string) $row['NAME']));
}

$conn->close();
