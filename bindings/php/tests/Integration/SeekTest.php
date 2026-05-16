<?php
declare(strict_types=1);

namespace OpenADS\Tests\Integration;

use OpenADS\Connection;

/**
 * Integration tests for Table::seek().
 *
 * Discovered engine behaviour:
 *  - SQL CREATE INDEX syntax: CREATE INDEX <tag> ON <table> (<expr>)
 *  - AdsGetIndexHandle expects the tag name exactly as given in the SQL
 *    (case-sensitive match against stored tag_name).
 *  - AdsOpenTable auto-attaches a sidecar .cdx when it exists, so the
 *    Table must be opened AFTER the CREATE INDEX SQL has run.
 *  - CdxIndex::seek_key() pads the supplied key to the field width with
 *    trailing spaces before B+tree comparison, so passing an exact field
 *    value (e.g. "alpha" for CHAR(5)) finds the record without manual
 *    padding.
 */
final class SeekTest extends IntegrationTestCase
{
    public function testSeekHitReturnsTrueAndPositionsRecord(): void
    {
        $dir  = $this->tempDataDir();
        $conn = new Connection($dir);
        $stmt = $conn->statement();

        // Create table and populate rows.
        $stmt->query('CREATE TABLE items (name CHAR(5))');
        $stmt->query("INSERT INTO items (name) VALUES ('alpha')");
        $stmt->query("INSERT INTO items (name) VALUES ('beta ')");
        $stmt->query("INSERT INTO items (name) VALUES ('gamma')");

        // Build the index via SQL — creates items.cdx with tag NAMORD.
        $stmt->query('CREATE INDEX NAMORD ON items (name)');

        // Open a fresh Table handle — AdsOpenTable auto-attaches items.cdx.
        $table = $conn->table('items');

        // Hard seek for an existing key: should return true.
        $found = $table->seek('NAMORD', 'alpha');
        self::assertTrue($found, 'seek("alpha") should return true (exact hit)');

        // After a hit the table must be positioned on that record.
        $rec = $table->record();
        self::assertSame('alpha', trim((string) $rec->get('name')));

        $table->close();
        $conn->close();
    }

    public function testSeekMissReturnsFalse(): void
    {
        $dir  = $this->tempDataDir();
        $conn = new Connection($dir);
        $stmt = $conn->statement();

        $stmt->query('CREATE TABLE items (name CHAR(5))');
        $stmt->query("INSERT INTO items (name) VALUES ('alpha')");
        $stmt->query("INSERT INTO items (name) VALUES ('gamma')");

        $stmt->query('CREATE INDEX NAMORD ON items (name)');

        $table = $conn->table('items');

        // Hard seek for a key that does not exist.
        $found = $table->seek('NAMORD', 'delta');
        self::assertFalse($found, 'seek("delta") should return false (no match)');

        $table->close();
        $conn->close();
    }
}
