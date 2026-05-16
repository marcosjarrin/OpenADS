<?php
declare(strict_types=1);

namespace OpenADS\Tests\Integration;

use OpenADS\Connection;
use OpenADS\Record;

final class TableTest extends IntegrationTestCase
{
    public function testOpenNavigateAndCount(): void
    {
        $dir  = $this->tempDataDir();
        $conn = new Connection($dir);
        $stmt = $conn->statement();
        $stmt->query('CREATE TABLE items (id INTEGER)');
        $stmt->query('INSERT INTO items (id) VALUES (?)', [10]);
        $stmt->query('INSERT INTO items (id) VALUES (?)', [20]);

        $table = $conn->table('items');
        self::assertSame(2, $table->recordCount());

        $table->gotoTop();
        self::assertInstanceOf(Record::class, $table->record());
        self::assertSame(10, $table->record()->get('ID'));

        $table->skip(1);
        self::assertSame(20, $table->record()->get('ID'));
        self::assertFalse($table->atEof());

        $table->skip(1);
        self::assertTrue($table->atEof());

        $table->close();
        $conn->close();
    }
}
