<?php
declare(strict_types=1);

namespace OpenADS\Tests\Integration;

use OpenADS\Ffi\AceLibrary;

final class AceLibraryTest extends IntegrationTestCase
{
    public function testLoadsAndExposesFfi(): void
    {
        $lib = AceLibrary::load(openads_test_lib());
        self::assertInstanceOf(\FFI::class, $lib->ffi());
    }

    public function testOutHandleHelperRoundTrips(): void
    {
        $lib = AceLibrary::load(openads_test_lib());
        $h   = $lib->newHandle();      // ADSHANDLE* out-parameter
        self::assertSame(0, $lib->handleValue($h));
    }
}
