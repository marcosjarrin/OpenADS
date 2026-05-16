<?php
declare(strict_types=1);

namespace OpenADS;

use FFI;
use OpenADS\Exception\OpenAdsException;
use OpenADS\Ffi\AceLibrary;
use OpenADS\Ffi\AceTypes;

final class Table
{
    private AceLibrary $lib;
    private int $handle = 0;
    private bool $open = false;
    /** @var list<string>|null  Engine field names, lazily read. */
    private ?array $fieldNames = null;

    public function __construct(private Connection $conn, string $name)
    {
        $this->lib = $conn->library();
        $ffi       = $this->lib->ffi();
        $out       = $this->lib->newHandle();
        // usTableType=0 (auto), usCharType=1 (ANSI), usLockType=1 (compat),
        // usCheckRights=0, usMode=0 (read/write shared).
        $rc = $ffi->AdsOpenTable(
            $conn->handle(),
            Connection::cstr($ffi, $name),
            null,
            0, 1, 1, 0, 0,
            FFI::addr($out)
        );
        if ($rc !== AceTypes::AE_SUCCESS) {
            throw $this->fail($rc, "open table '$name'");
        }
        $this->handle = $this->lib->handleValue($out);
        $this->open   = true;
    }

    public function handle(): int
    {
        return $this->handle;
    }

    public function library(): AceLibrary
    {
        return $this->lib;
    }

    /**
     * Map a user-supplied field name to the engine's stored name,
     * case-insensitively. Throws if the field does not exist.
     */
    public function resolveField(string $name): string
    {
        if ($this->fieldNames === null) {
            $this->fieldNames = $this->readFieldNames();
        }
        $upper = strtoupper($name);
        foreach ($this->fieldNames as $real) {
            if (strtoupper($real) === $upper) {
                return $real;
            }
        }
        throw new OpenAdsException("unknown field '$name'");
    }

    public function recordCount(): int
    {
        $out = $this->lib->ffi()->new('UNSIGNED32');
        $this->check(
            $this->lib->ffi()->AdsGetRecordCount($this->handle, 0, FFI::addr($out)),
            'record count'
        );
        return (int) $out->cdata;
    }

    public function gotoTop(): void
    {
        $this->check($this->lib->ffi()->AdsGotoTop($this->handle), 'goto top');
    }

    public function gotoBottom(): void
    {
        $this->check($this->lib->ffi()->AdsGotoBottom($this->handle), 'goto bottom');
    }

    public function gotoRecord(int $recno): void
    {
        $this->check(
            $this->lib->ffi()->AdsGotoRecord($this->handle, $recno),
            "goto record $recno"
        );
    }

    public function skip(int $rows): void
    {
        $this->check($this->lib->ffi()->AdsSkip($this->handle, $rows), 'skip');
    }

    public function atEof(): bool
    {
        $eof = $this->lib->ffi()->new('UNSIGNED16');
        $this->check(
            $this->lib->ffi()->AdsAtEOF($this->handle, FFI::addr($eof)),
            'at eof'
        );
        return ((int) $eof->cdata) !== 0;
    }

    public function atBof(): bool
    {
        $bof = $this->lib->ffi()->new('UNSIGNED16');
        $this->check(
            $this->lib->ffi()->AdsAtBOF($this->handle, FFI::addr($bof)),
            'at bof'
        );
        return ((int) $bof->cdata) !== 0;
    }

    public function setAof(string $expr): void
    {
        $ffi = $this->lib->ffi();
        $this->check(
            $ffi->AdsSetAOF($this->handle, Connection::cstr($ffi, $expr), 0),
            'set AOF'
        );
    }

    public function clearAof(): void
    {
        $this->check($this->lib->ffi()->AdsClearAOF($this->handle), 'clear AOF');
    }

    /**
     * Seek a key on an open index tag. Returns true if an exact
     * match was found; on a hit the table is positioned on that
     * record. With $soft = true a near-match positions on the
     * next key >= $key and the method still reports whether the
     * match was exact.
     */
    public function seek(string $indexTag, string $key, bool $soft = false): bool
    {
        $ffi    = $this->lib->ffi();
        $idxOut = $this->lib->newHandle();
        $rc = $ffi->AdsGetIndexHandle(
            $this->handle,
            Connection::cstr($ffi, $indexTag),
            FFI::addr($idxOut)
        );
        if ($rc !== AceTypes::AE_SUCCESS) {
            throw $this->fail($rc, "get index handle '$indexTag'");
        }
        $hIndex = $this->lib->handleValue($idxOut);

        $found = $ffi->new('UNSIGNED16');
        $rc = $ffi->AdsSeek(
            $hIndex,
            Connection::cstr($ffi, $key),
            strlen($key),
            AceTypes::ADS_STRINGKEY,
            $soft ? AceTypes::ADS_SOFTSEEK : AceTypes::ADS_HARDSEEK,
            FFI::addr($found)
        );
        if ($rc !== AceTypes::AE_SUCCESS) {
            throw $this->fail($rc, "seek '$key' on index '$indexTag'");
        }
        return ((int) $found->cdata) !== 0;
    }

    public function record(): Record
    {
        return new Record($this);
    }

    public function close(): void
    {
        if (!$this->open) {
            return;
        }
        $this->lib->ffi()->AdsCloseTable($this->handle);
        $this->open = false;
    }

    public function __destruct()
    {
        $this->close();
    }

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

    private function check(int $rc, string $what): void
    {
        if ($rc !== AceTypes::AE_SUCCESS) {
            throw $this->fail($rc, $what);
        }
    }

    private function fail(int $rc, string $what): OpenAdsException
    {
        [$code, $text] = $this->lib->lastError();
        return new OpenAdsException(
            "table $what failed: " . AceTypes::errorName($code) . " ($code): $text",
            $code
        );
    }
}
