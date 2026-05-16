<?php
declare(strict_types=1);

namespace OpenADS\Tests\Integration;

use OpenADS\Connection;

final class RecordTest extends IntegrationTestCase
{
    public function testAppendSetReadAndDelete(): void
    {
        $dir  = $this->tempDataDir();
        $conn = new Connection($dir);
        $conn->statement()->query('CREATE TABLE c (id INTEGER, name CHAR(20))');

        $table = $conn->table('c');
        $rec   = $table->record();
        $rec->append();
        $rec->set('ID', 42);
        $rec->set('NAME', 'Reinaldo');
        $rec->save();

        $table->gotoTop();
        self::assertSame(42, $table->record()->get('ID'));
        self::assertSame('Reinaldo', trim((string) $table->record()->get('NAME')));

        $table->record()->delete();
        self::assertSame(1, $table->recordCount()); // still present, flagged deleted

        $table->close();
        $conn->close();
    }
}
