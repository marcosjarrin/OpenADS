<?php
declare(strict_types=1);

namespace OpenADS\Tests\Integration;

use OpenADS\Connection;

final class CursorTest extends IntegrationTestCase
{
    public function testIterateRowsAsAssoc(): void
    {
        $conn = new Connection($this->tempDataDir());
        $stmt = $conn->statement();
        $stmt->query('CREATE TABLE n (id INTEGER, label CHAR(10))');
        $stmt->query('INSERT INTO n (id, label) VALUES (?, ?)', [1, 'one']);
        $stmt->query('INSERT INTO n (id, label) VALUES (?, ?)', [2, 'two']);

        $cursor = $stmt->query('SELECT id, label FROM n');
        $rows   = [];
        foreach ($cursor as $row) {
            $rows[] = $row;
        }

        self::assertCount(2, $rows);
        self::assertSame(1, $rows[0]['ID']);
        self::assertSame('one', trim((string) $rows[0]['LABEL']));
        $conn->close();
    }

    public function testFetchAll(): void
    {
        $conn = new Connection($this->tempDataDir());
        $stmt = $conn->statement();
        $stmt->query('CREATE TABLE n (id INTEGER)');
        $stmt->query('INSERT INTO n (id) VALUES (?)', [7]);

        $rows = $stmt->query('SELECT id FROM n')->fetchAll();
        self::assertSame([['ID' => 7]], $rows);
        $conn->close();
    }
}
