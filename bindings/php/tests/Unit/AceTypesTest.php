<?php
declare(strict_types=1);

namespace OpenADS\Tests\Unit;

use OpenADS\Ffi\AceTypes;
use PHPUnit\Framework\TestCase;

final class AceTypesTest extends TestCase
{
    public function testServerTypeConstants(): void
    {
        self::assertSame(1, AceTypes::ADS_LOCAL_SERVER);
        self::assertSame(2, AceTypes::ADS_REMOTE_SERVER);
    }

    public function testSuccessCodeIsZero(): void
    {
        self::assertSame(0, AceTypes::AE_SUCCESS);
    }

    public function testErrorNameKnownCode(): void
    {
        self::assertSame('AE_NO_CURRENT_RECORD', AceTypes::errorName(5026));
    }

    public function testErrorNameUnknownCode(): void
    {
        self::assertSame('AE_UNKNOWN(9999)', AceTypes::errorName(9999));
    }
}
