<?php
declare(strict_types=1);

require __DIR__ . '/../vendor/autoload.php';

/**
 * Resolve the ACE library path for integration tests.
 * Returns null when no usable library is configured, so
 * IntegrationTestCase can skip rather than fail.
 */
function openads_test_lib(): ?string
{
    $path = getenv('OPENADS_ACE_LIB');
    if ($path === false || $path === '') {
        return null;
    }
    return is_file($path) ? $path : null;
}
