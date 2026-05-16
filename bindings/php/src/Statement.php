<?php
declare(strict_types=1);

namespace OpenADS;

use FFI;
use OpenADS\Exception\QueryException;
use OpenADS\Ffi\AceTypes;
use OpenADS\Sql\ParameterBinder;

final class Statement
{
    private int $handle = 0;
    private bool $open = false;
    private ?string $prepared = null;

    public function __construct(private Connection $conn)
    {
        $ffi = $conn->library()->ffi();
        $out = $conn->library()->newHandle();
        $rc  = $ffi->AdsCreateSQLStatement($conn->handle(), FFI::addr($out));
        if ($rc !== AceTypes::AE_SUCCESS) {
            throw $this->fail($rc, 'create statement');
        }
        $this->handle = $conn->library()->handleValue($out);
        $this->open   = true;
    }

    /** Store SQL for later execute() calls. */
    public function prepare(string $sql): self
    {
        $this->prepared = $sql;
        return $this;
    }

    /** Run a previously prepared statement. */
    public function execute(array $params = []): Cursor|int
    {
        if ($this->prepared === null) {
            throw new QueryException('execute() called without prepare()');
        }
        return $this->run($this->prepared, $params);
    }

    /** Bind params and run $sql immediately. */
    public function query(string $sql, array $params = []): Cursor|int
    {
        return $this->run($sql, $params);
    }

    private function run(string $sql, array $params): Cursor|int
    {
        $bound = ParameterBinder::bind($sql, $params);
        $ffi   = $this->conn->library()->ffi();
        $out   = $this->conn->library()->newHandle();
        $rc    = $ffi->AdsExecuteSQLDirect(
            $this->handle,
            Connection::cstr($ffi, $bound),
            FFI::addr($out)
        );
        if ($rc !== AceTypes::AE_SUCCESS) {
            throw $this->fail($rc, "execute: $bound");
        }
        $cursorHandle = $this->conn->library()->handleValue($out);
        return $cursorHandle === 0
            ? 1 // non-SELECT: one statement applied
            : new Cursor($this->conn, $cursorHandle);
    }

    public function close(): void
    {
        if (!$this->open) {
            return;
        }
        $this->conn->library()->ffi()->AdsCloseSQLStatement($this->handle);
        $this->open = false;
    }

    public function __destruct()
    {
        $this->close();
    }

    private function fail(int $rc, string $what): QueryException
    {
        [$code, $text] = $this->conn->library()->lastError();
        return new QueryException(
            "$what failed: " . AceTypes::errorName($code) . " ($code): $text",
            $code
        );
    }
}
