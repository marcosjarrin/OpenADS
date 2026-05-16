<?php
declare(strict_types=1);

namespace OpenADS\Tests\Unit;

use OpenADS\Exception\QueryException;
use OpenADS\Sql\ParameterBinder;
use PHPUnit\Framework\TestCase;

final class ParameterBinderTest extends TestCase
{
    public function testPositionalParams(): void
    {
        $sql = ParameterBinder::bind('SELECT * FROM t WHERE id = ? AND n = ?', [5, 'ab']);
        self::assertSame("SELECT * FROM t WHERE id = 5 AND n = 'ab'", $sql);
    }

    public function testNamedParams(): void
    {
        $sql = ParameterBinder::bind('SELECT * FROM t WHERE id = :id', [':id' => 7]);
        self::assertSame('SELECT * FROM t WHERE id = 7', $sql);
    }

    public function testStringQuoteIsEscaped(): void
    {
        $sql = ParameterBinder::bind('WHERE name = ?', ["O'Brien"]);
        self::assertSame("WHERE name = 'O''Brien'", $sql);
    }

    public function testNullBoolAndFloat(): void
    {
        $sql = ParameterBinder::bind('VALUES (?, ?, ?)', [null, true, 1.5]);
        self::assertSame('VALUES (NULL, .T., 1.5)', $sql);
    }

    public function testDateTimeBecomesAceLiteral(): void
    {
        $dt  = new \DateTimeImmutable('2026-05-16');
        $sql = ParameterBinder::bind('WHERE d = ?', [$dt]);
        self::assertSame("WHERE d = '2026-05-16'", $sql);
    }

    public function testWrongPositionalCountThrows(): void
    {
        $this->expectException(QueryException::class);
        ParameterBinder::bind('WHERE id = ? AND n = ?', [1]);
    }

    public function testUnsupportedTypeThrows(): void
    {
        $this->expectException(QueryException::class);
        ParameterBinder::bind('WHERE id = ?', [new \stdClass()]);
    }
}
