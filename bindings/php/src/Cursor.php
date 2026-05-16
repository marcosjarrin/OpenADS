<?php
declare(strict_types=1);

namespace OpenADS;

use FFI;
use Iterator;
use OpenADS\Exception\QueryException;
use OpenADS\Ffi\AceLibrary;
use OpenADS\Ffi\AceTypes;

/**
 * @implements Iterator<int, array<string, mixed>>
 */
final class Cursor implements Iterator
{
    private AceLibrary $lib;
    /**
     * Raw field names as returned by the engine (case-preserved).
     * @var list<string>
     */
    private array $fields = [];
    /**
     * Uppercase aliases for the fields — used as the PHP array keys.
     * Real ACE always upper-cases field names; OpenADS preserves the
     * CREATE TABLE case, so we normalise here to match real-ACE behaviour
     * and make column look-ups case-insensitive for callers.
     * @var list<string>
     */
    private array $fieldKeys = [];
    private int $position = 0;
    private bool $closed = false;
    private bool $fetchStarted = false;

    public function __construct(private Connection $conn, private int $handle)
    {
        $this->lib       = $conn->library();
        $this->fields    = $this->readFieldNames();
        $this->fieldKeys = array_map('strtoupper', $this->fields);
    }

    public function count(): int
    {
        $ffi = $this->lib->ffi();
        $out = $ffi->new('UNSIGNED32');
        $rc  = $ffi->AdsGetRecordCount($this->handle, 0, FFI::addr($out));
        $this->check($rc, 'record count');
        return (int) $out->cdata;
    }

    /** @return list<array<string, mixed>> */
    public function fetchAll(): array
    {
        $rows = [];
        foreach ($this as $row) {
            $rows[] = $row;
        }
        return $rows;
    }

    /**
     * Fetch the next row as an associative array (uppercase column
     * keys), or null when the result set is exhausted. Use either
     * fetch*() OR foreach iteration on one Cursor, not both.
     */
    public function fetchAssoc(): ?array
    {
        return $this->fetchRow();
    }

    /** Fetch the next row as a 0-indexed numeric array, or null at end. */
    public function fetchNum(): ?array
    {
        $row = $this->fetchRow();
        return $row === null ? null : array_values($row);
    }

    private function fetchRow(): ?array
    {
        $ffi = $this->lib->ffi();
        if (!$this->fetchStarted) {
            $this->check($ffi->AdsGotoTop($this->handle), 'goto top');
            $this->fetchStarted = true;
        } else {
            $this->check($ffi->AdsSkip($this->handle, 1), 'skip');
        }
        if (!$this->valid()) {
            return null;
        }
        return $this->current();
    }

    // --- Iterator -------------------------------------------------

    public function rewind(): void
    {
        $this->check($this->lib->ffi()->AdsGotoTop($this->handle), 'goto top');
        $this->position = 0;
    }

    public function valid(): bool
    {
        $ffi = $this->lib->ffi();
        $eof = $ffi->new('UNSIGNED16');
        $this->check($ffi->AdsAtEOF($this->handle, FFI::addr($eof)), 'at eof');
        return ((int) $eof->cdata) === 0;
    }

    /** @return array<string, mixed> */
    public function current(): array
    {
        $row = [];
        foreach ($this->fields as $i => $name) {
            $row[$this->fieldKeys[$i]] = self::readField($this->lib, $this->handle, $name);
        }
        return $row;
    }

    public function key(): int
    {
        return $this->position;
    }

    public function next(): void
    {
        $this->check($this->lib->ffi()->AdsSkip($this->handle, 1), 'skip');
        $this->position++;
    }

    // --- internals ------------------------------------------------

    /** @return list<string> */
    private function readFieldNames(): array
    {
        $ffi   = $this->lib->ffi();
        $count = $ffi->new('UNSIGNED16');
        $this->check($ffi->AdsGetNumFields($this->handle, FFI::addr($count)), 'num fields');

        $names = [];
        for ($i = 1; $i <= (int) $count->cdata; $i++) {
            $len = $ffi->new('UNSIGNED16');
            $len->cdata = 128;
            $buf = $ffi->new('UNSIGNED8[128]');
            $this->check(
                $ffi->AdsGetFieldName($this->handle, $i, $buf, FFI::addr($len)),
                'field name'
            );
            $names[] = FFI::string(FFI::cast('char*', FFI::addr($buf[0])));
        }
        return $names;
    }

    /**
     * Read one field at the current record and convert it to a PHP
     * value. Shared by Cursor::current() and Record::get().
     */
    public static function readField(AceLibrary $lib, int $handle, string $field): mixed
    {
        $ffi  = $lib->ffi();
        $type = $ffi->new('UNSIGNED16');
        $ffi->AdsGetFieldType($handle, Connection::cstr($ffi, $field), FFI::addr($type));

        $len = $ffi->new('UNSIGNED32');
        $len->cdata = 8192;
        $buf = $ffi->new('UNSIGNED8[8192]');
        $rc  = $ffi->AdsGetField(
            $handle,
            Connection::cstr($ffi, $field),
            $buf,
            FFI::addr($len),
            0
        );
        if ($rc !== AceTypes::AE_SUCCESS) {
            return null; // not-positioned / blank field
        }
        $raw = rtrim(FFI::string(FFI::cast('char*', FFI::addr($buf[0])), (int) $len->cdata));

        return match ((int) $type->cdata) {
            AceTypes::ADS_INTEGER, AceTypes::ADS_NUMERIC =>
                $raw === '' ? null : (str_contains($raw, '.') ? (float) $raw : (int) $raw),
            AceTypes::ADS_DOUBLE =>
                $raw === '' ? null : (float) $raw,
            AceTypes::ADS_LOGICAL =>
                in_array(strtoupper($raw), ['T', 'Y', '1'], true),
            default => $raw,
        };
    }

    private function check(int $rc, string $what): void
    {
        if ($rc !== AceTypes::AE_SUCCESS) {
            [$code, $text] = $this->lib->lastError();
            throw new QueryException(
                "cursor $what failed: " . AceTypes::errorName($code) . " ($code): $text",
                $code
            );
        }
    }

    public function close(): void
    {
        if ($this->closed) {
            return;
        }
        $this->lib->ffi()->AdsCloseTable($this->handle);
        $this->closed = true;
    }

    public function __destruct()
    {
        $this->close();
    }
}
