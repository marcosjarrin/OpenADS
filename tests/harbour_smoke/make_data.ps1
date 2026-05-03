# Generates a minimal DBF fixture (data.dbf) for the M8.3 smoke test.
#
# Layout:
#   - 32-byte file header (signature 0x03 = dBASE III, 2 records,
#     header_len = 65, record_len = 11)
#   - one 32-byte field descriptor: NAME, type C, length 10
#   - 0x0D field-terminator
#   - 2 records of (delete-flag + 10 chars):
#       ' ' "ALPHA     "
#       ' ' "BETA      "
#   - 0x1A end-of-file marker

param(
    [string]$OutPath = (Join-Path $PSScriptRoot 'data.dbf')
)

$bytes = New-Object System.Collections.Generic.List[byte]

# Header (32 bytes).
$bytes.Add(0x03)              # signature: dBASE III, no memo
$bytes.AddRange([byte[]]@(0,0,0))   # last update yymm dd zeros
$bytes.AddRange([byte[]]@(2,0,0,0)) # record count = 2
$bytes.AddRange([byte[]]@(65,0))    # header length = 32 + 32 + 1
$bytes.AddRange([byte[]]@(11,0))    # record length = 1 + 10
1..20 | ForEach-Object { $bytes.Add([byte]0) }  # padding to 32 bytes

# Field descriptor for NAME (32 bytes).
$nameBytes = [byte[]]@([byte][char]'N',[byte][char]'A',[byte][char]'M',[byte][char]'E')
$bytes.AddRange($nameBytes)
1..7 | ForEach-Object { $bytes.Add([byte]0) }   # zero-padded name (11 bytes total)
$bytes.Add([byte][char]'C')         # field type C
$bytes.AddRange([byte[]]@(0,0,0,0)) # field data address (unused)
$bytes.Add(10)                       # field length = 10
$bytes.Add(0)                        # decimal count
1..14 | ForEach-Object { $bytes.Add([byte]0) }  # padding to 32 bytes

# Field-descriptor terminator.
$bytes.Add(0x0D)

# Records. Each is 1 delete-flag byte + a 10-byte fixed-width NAME field.
function Add-Record([System.Collections.Generic.List[byte]]$bytes, [string]$name) {
    $bytes.Add([byte][char]' ')
    $padded = $name.PadRight(10).Substring(0, 10)
    foreach ($c in $padded.ToCharArray()) { $bytes.Add([byte]$c) }
}
Add-Record $bytes 'ALPHA'
Add-Record $bytes 'BETA'

# End-of-file marker.
$bytes.Add(0x1A)

[System.IO.File]::WriteAllBytes($OutPath, $bytes.ToArray())
Write-Output ("wrote {0} bytes to {1}" -f $bytes.Count, $OutPath)
