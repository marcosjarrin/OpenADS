<?php
declare(strict_types=1);

namespace OpenADS;

use OpenADS\Exception\OpenAdsException;
use OpenADS\Ffi\AceTypes;

/**
 * The current record of a Table. Field reads reuse
 * Cursor::readField so type conversion stays in one place.
 * Field names are resolved case-insensitively via Table::resolveField.
 */
final class Record
{
    public function __construct(private Table $table)
    {
    }

    public function get(string $field): mixed
    {
        return Cursor::readField(
            $this->table->library(),
            $this->table->handle(),
            $this->table->resolveField($field)
        );
    }

    public function set(string $field, mixed $value): void
    {
        $ffi  = $this->table->library()->ffi();
        $real = $this->table->resolveField($field);
        $str  = $this->stringify($value);
        $this->check(
            $ffi->AdsSetField(
                $this->table->handle(),
                Connection::cstr($ffi, $real),
                Connection::cstr($ffi, $str),
                strlen($str)
            ),
            "set field '$field'"
        );
    }

    public function append(): void
    {
        $this->check(
            $this->table->library()->ffi()->AdsAppendRecord($this->table->handle()),
            'append record'
        );
    }

    /** Flush a pending append/set to disk. */
    public function save(): void
    {
        $this->check(
            $this->table->library()->ffi()->AdsWriteRecord($this->table->handle()),
            'write record'
        );
    }

    public function delete(): void
    {
        $this->check(
            $this->table->library()->ffi()->AdsDeleteRecord($this->table->handle()),
            'delete record'
        );
    }

    public function recall(): void
    {
        $this->check(
            $this->table->library()->ffi()->AdsRecallRecord($this->table->handle()),
            'recall record'
        );
    }

    public function lock(int $recno): bool
    {
        $rc = $this->table->library()->ffi()
            ->AdsLockRecord($this->table->handle(), $recno);
        return $rc === AceTypes::AE_SUCCESS;
    }

    public function unlock(int $recno): void
    {
        $this->check(
            $this->table->library()->ffi()
                ->AdsUnlockRecord($this->table->handle(), $recno),
            'unlock record'
        );
    }

    private function stringify(mixed $value): string
    {
        if ($value === null) {
            return '';
        }
        if (is_bool($value)) {
            return $value ? 'T' : 'F';
        }
        if ($value instanceof \DateTimeInterface) {
            return $value->format('Y-m-d');
        }
        return (string) $value;
    }

    private function check(int $rc, string $what): void
    {
        if ($rc !== AceTypes::AE_SUCCESS) {
            [$code, $text] = $this->table->library()->lastError();
            throw new OpenAdsException(
                "record $what failed: " . AceTypes::errorName($code) . " ($code): $text",
                $code
            );
        }
    }
}
