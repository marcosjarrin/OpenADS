<?php
declare(strict_types=1);

namespace OpenADS\Ffi;

/**
 * ACE numeric constants and error-code names. Values mirror
 * include/openads/ace.h and include/openads/error.h. Names only —
 * no proprietary help text.
 */
final class AceTypes
{
    public const ADS_LOCAL_SERVER  = 1;
    public const ADS_REMOTE_SERVER = 2;

    public const AE_SUCCESS = 0;

    /** ACE field-type codes returned by AdsGetFieldType. */
    public const ADS_STRING    = 1;
    public const ADS_NUMERIC   = 2;
    public const ADS_DATE      = 3;
    public const ADS_LOGICAL   = 4;
    public const ADS_MEMO      = 5;
    public const ADS_DOUBLE    = 10;
    public const ADS_INTEGER   = 11;
    public const ADS_TIMESTAMP = 13;

    /** Subset of AE_* codes worth naming in error messages. */
    private const ERROR_NAMES = [
        0    => 'AE_SUCCESS',
        5000 => 'AE_INTERNAL_ERROR',
        5004 => 'AE_FUNCTION_NOT_AVAILABLE',
        5026 => 'AE_NO_CURRENT_RECORD',
    ];

    public static function errorName(int $code): string
    {
        return self::ERROR_NAMES[$code] ?? "AE_UNKNOWN($code)";
    }
}
