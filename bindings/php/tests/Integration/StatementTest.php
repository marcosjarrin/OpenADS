<?php
declare(strict_types=1);

namespace OpenADS\Tests\Integration;

use OpenADS\Connection;
use OpenADS\Cursor;

final class StatementTest extends IntegrationTestCase
{
    public function testCreateInsertReturnsRowCountAndSelectReturnsCursor(): void
    {
        $conn = new Connection($this->tempDataDir());
        $stmt = $conn->statement();

        $stmt->query('CREATE TABLE people (id INTEGER, name CHAR(20))');
        $affected = $stmt->query(
            'INSERT INTO people (id, name) VALUES (?, ?)',
            [1, "O'Brien"]
        );
        self::assertSame(1, $affected);

        $result = $stmt->query('SELECT * FROM people WHERE id = :id', [':id' => 1]);
        self::assertInstanceOf(Cursor::class, $result);
        self::assertSame(1, $result->count());

        $conn->close();
    }
}
