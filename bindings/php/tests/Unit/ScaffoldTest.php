<?php
declare(strict_types=1);

namespace OpenADS\Tests\Unit;

use PHPUnit\Framework\TestCase;

final class ScaffoldTest extends TestCase
{
    public function testAutoloadAndFfiExtensionPresent(): void
    {
        self::assertTrue(extension_loaded('ffi'), 'ext-ffi must be enabled');
    }
}
