<?php
declare(strict_types=1);

namespace OpenADS\Exception;

use OpenADS\Ffi\AceTypes;
use RuntimeException;

class OpenAdsException extends RuntimeException
{
    private int $aceCode;

    public function __construct(string $message, int $aceCode = 0)
    {
        parent::__construct($message);
        $this->aceCode = $aceCode;
    }

    public static function fromAce(int $code, string $detail): self
    {
        $name = AceTypes::errorName($code);
        return new self("$name ($code): $detail", $code);
    }

    public function aceCode(): int
    {
        return $this->aceCode;
    }
}
