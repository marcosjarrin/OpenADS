<?php
declare(strict_types=1);

namespace OpenADS\Tests\Integration;

use OpenADS\Connection;
use OpenADS\Exception\ConnectionException;

final class ConnectionTest extends IntegrationTestCase
{
    public function testConnectLocalDirAndClose(): void
    {
        $conn = new Connection($this->tempDataDir());
        self::assertTrue($conn->isOpen());
        $conn->close();
        self::assertFalse($conn->isOpen());
    }

    public function testCloseIsIdempotent(): void
    {
        $conn = new Connection($this->tempDataDir());
        $conn->close();
        $conn->close(); // must not throw
        self::assertFalse($conn->isOpen());
    }

    public function testBadRemoteUriThrowsConnectionException(): void
    {
        $this->expectException(ConnectionException::class);
        new Connection('tcp://127.0.0.1:1/nope');
    }
}
