<?php
declare(strict_types=1);

namespace OpenADS\Tests\Integration;

use PHPUnit\Framework\TestCase;

abstract class IntegrationTestCase extends TestCase
{
    protected function setUp(): void
    {
        if (openads_test_lib() === null) {
            self::markTestSkipped('OPENADS_ACE_LIB not set; skipping integration test');
        }
    }

    /** A fresh empty directory usable as a local-mode data dir. */
    protected function tempDataDir(): string
    {
        $dir = sys_get_temp_dir() . '/openads_php_' . bin2hex(random_bytes(6));
        mkdir($dir);
        return $dir;
    }
}
