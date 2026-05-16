<?php
declare(strict_types=1);

namespace OpenADS\Tests\Unit;

use OpenADS\Exception\ConnectionException;
use OpenADS\Exception\OpenAdsException;
use OpenADS\Exception\QueryException;
use PHPUnit\Framework\TestCase;

final class ExceptionTest extends TestCase
{
    public function testFromAceBuildsNamedMessage(): void
    {
        $e = OpenAdsException::fromAce(5026, 'table not positioned');
        self::assertSame(5026, $e->aceCode());
        self::assertStringContainsString('AE_NO_CURRENT_RECORD', $e->getMessage());
        self::assertStringContainsString('table not positioned', $e->getMessage());
    }

    public function testSubclassesExtendBase(): void
    {
        self::assertInstanceOf(OpenAdsException::class, new ConnectionException('x'));
        self::assertInstanceOf(OpenAdsException::class, new QueryException('x'));
    }
}
