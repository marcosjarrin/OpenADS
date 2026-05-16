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

    public function testFetchAssocReturnsRowsThenNull(): void
    {
        $conn = new Connection($this->tempDataDir());
        $stmt = $conn->statement();
        $stmt->query('CREATE TABLE fa (id INTEGER, label CHAR(5))');
        $stmt->query('INSERT INTO fa (id, label) VALUES (?, ?)', [1, 'alpha']);
        $stmt->query('INSERT INTO fa (id, label) VALUES (?, ?)', [2, 'beta']);

        $cursor = $stmt->query('SELECT id, label FROM fa');

        $row1 = $cursor->fetchAssoc();
        self::assertIsArray($row1);
        self::assertSame(1, $row1['ID']);
        self::assertSame('alpha', trim((string) $row1['LABEL']));

        $row2 = $cursor->fetchAssoc();
        self::assertIsArray($row2);
        self::assertSame(2, $row2['ID']);
        self::assertSame('beta', trim((string) $row2['LABEL']));

        $row3 = $cursor->fetchAssoc();
        self::assertNull($row3);

        $conn->close();
    }

    public function testFetchNumReturnsZeroIndexedRow(): void
    {
        $conn = new Connection($this->tempDataDir());
        $stmt = $conn->statement();
        $stmt->query('CREATE TABLE fn (id INTEGER, label CHAR(5))');
        $stmt->query('INSERT INTO fn (id, label) VALUES (?, ?)', [42, 'hello']);

        $cursor = $stmt->query('SELECT id, label FROM fn');

        $row = $cursor->fetchNum();
        self::assertIsArray($row);
        self::assertSame(42, $row[0]);
        self::assertSame('hello', trim((string) $row[1]));
        self::assertArrayHasKey(0, $row);
        self::assertArrayHasKey(1, $row);
        // Numeric array: no string keys
        self::assertSame([0, 1], array_keys($row));

        $end = $cursor->fetchNum();
        self::assertNull($end);

        $conn->close();
    }
}
