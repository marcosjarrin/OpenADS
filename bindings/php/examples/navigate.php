<?php
declare(strict_types=1);

require __DIR__ . '/../vendor/autoload.php';

use OpenADS\Connection;

$conn  = new Connection($argv[1] ?? __DIR__ . '/data');
$table = $conn->table('people');

printf("records: %d\n", $table->recordCount());

for ($table->gotoTop(); !$table->atEof(); $table->skip(1)) {
    $rec = $table->record();
    printf("%d  %s\n", $rec->get('ID'), trim((string) $rec->get('NAME')));
}

$table->close();
$conn->close();
