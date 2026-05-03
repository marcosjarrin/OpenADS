# Generates a multi-field DBF fixture (data.dbf) for the M8.5 smoke test.
#
# Schema:
#   NAME    C(10)   - character
#   AGE     N(3,0)  - numeric, 3 digits, 0 decimals
#   ACTIVE  L(1)    - logical
#   BORN    D(8)    - date (stored as YYYYMMDD ASCII)
#
# Records:
#   ALPHA       30   T   1990-01-01
#   BETA       125   F   2000-06-15
#   GAMMA       77   T   2025-12-31
#
# Layout:
#   - 32-byte header (signature 0x03, 3 records, header_len = 161,
#     record_len = 23)
#   - 4 x 32-byte field descriptors
#   - 0x0D field-terminator
#   - 3 x 23-byte records (delete-flag + 10 NAME + 3 AGE + 1 ACTIVE + 8 BORN)
#   - 0x1A end-of-file marker

param(
    [string]$OutPath = (Join-Path $PSScriptRoot 'data.dbf')
)

$bytes = New-Object System.Collections.Generic.List[byte]

# Header (32 bytes).
$bytes.Add(0x03)
$bytes.AddRange([byte[]]@(0,0,0))           # last update yymm dd zeros
$bytes.AddRange([byte[]]@(3,0,0,0))         # record count = 3
$bytes.AddRange([byte[]]@(161,0))           # header length = 32 + 4*32 + 1
$bytes.AddRange([byte[]]@(23,0))            # record length = 1 + 10 + 3 + 1 + 8
1..20 | ForEach-Object { $bytes.Add([byte]0) }

function Add-FieldDescriptor {
    param(
        [System.Collections.Generic.List[byte]]$bytes,
        [string]$Name,
        [char]$TypeChar,
        [byte]$Length,
        [byte]$Decimals = 0
    )
    $nameBytes = [System.Text.Encoding]::ASCII.GetBytes($Name)
    foreach ($b in $nameBytes) { $bytes.Add($b) }
    $remaining = 11 - $nameBytes.Length
    1..$remaining | ForEach-Object { $bytes.Add([byte]0) }
    $bytes.Add([byte]$TypeChar)
    $bytes.AddRange([byte[]]@(0,0,0,0))     # field data address
    $bytes.Add($Length)
    $bytes.Add($Decimals)
    1..14 | ForEach-Object { $bytes.Add([byte]0) }
}

Add-FieldDescriptor $bytes 'NAME'   'C' 10 0
Add-FieldDescriptor $bytes 'AGE'    'N'  3 0
Add-FieldDescriptor $bytes 'ACTIVE' 'L'  1 0
Add-FieldDescriptor $bytes 'BORN'   'D'  8 0

# Field-descriptor terminator.
$bytes.Add(0x0D)

# Records: delete-flag + NAME(10) + AGE(3) + ACTIVE(1) + BORN(8).
function Add-Record {
    param(
        [System.Collections.Generic.List[byte]]$bytes,
        [string]$Name,
        [int]$Age,
        [bool]$Active,
        [string]$Born   # YYYYMMDD
    )
    $bytes.Add([byte][char]' ')                     # not deleted
    foreach ($c in $Name.PadRight(10).Substring(0,10).ToCharArray()) {
        $bytes.Add([byte]$c)
    }
    $ageStr = ([string]$Age).PadLeft(3)
    foreach ($c in $ageStr.Substring(0,3).ToCharArray()) {
        $bytes.Add([byte]$c)
    }
    $bytes.Add([byte][char]($(if ($Active) {'T'} else {'F'})))
    foreach ($c in $Born.PadRight(8).Substring(0,8).ToCharArray()) {
        $bytes.Add([byte]$c)
    }
}

Add-Record $bytes 'ALPHA'  30 $true  '19900101'
Add-Record $bytes 'BETA'  125 $false '20000615'
Add-Record $bytes 'GAMMA'  77 $true  '20251231'

# End-of-file marker.
$bytes.Add(0x1A)

[System.IO.File]::WriteAllBytes($OutPath, $bytes.ToArray())
Write-Output ("wrote {0} bytes to {1}" -f $bytes.Count, $OutPath)
