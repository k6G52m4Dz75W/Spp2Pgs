# Regression check for the built binaries.
#
# The samples draw a plain rectangle with the ASS drawing commands instead of
# text, so the result does not depend on which font build happens to be
# installed and the same input gives the same stream on any machine.
#
# Assertions are relative on purpose: nothing here compares against a hash
# recorded on someone else's computer, which would break the first time a
# toolchain patch arrived.

$ErrorActionPreference = 'Stop'

$exe = '.\bin\Spp2Pgs64.exe'
$dll = '.\bin\xy-VSSppf64.dll'
foreach ($f in @($exe, $dll)) {
    if (-not (Test-Path $f)) { throw "missing $f" }
}

Remove-Item -Recurse -Force .tmp -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path .tmp | Out-Null

function Convert-Sub {
    param([string]$In, [string]$Out, [string[]]$Extra = @())

    $argList = @('-i', $In, '-s', '1080', '-r', '23') + $Extra + @($Out)
    $saved = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = & $exe @argList 2>&1 | ForEach-Object { "$_" }
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $saved
    }
    $text = $lines -join "`n"

    # A successful conversion has to leave 0 behind, which is what any caller
    # in a pipeline branches on. 1 is a bad command line, 2 a failed
    # conversion, 3 an exception that escaped.
    if ($code -ne 0) {
        throw "conversion of $In failed with exit code ${code}:`n$text"
    }
    if ($text -notmatch 'Encoding successfully completed') {
        throw "conversion of $In did not report success:`n$text"
    }
    if ($text -match '(\d+) frames in total') {
        return [int]$Matches[1]
    }
    throw "no frame count in the output of ${In}:`n$text"
}

function Get-Md5([string]$Path) {
    return (Get-FileHash -Algorithm MD5 -Path $Path).Hash
}

Write-Host 'sdr, trailing line break'
$frames = Convert-Sub -In 'test\solid.ass' -Out '.tmp\solid.sup'
Write-Host "  frames: $frames"

Write-Host 'sdr, no trailing line break'
$framesNoEol = Convert-Sub -In 'test\solid-noeol.ass' -Out '.tmp\solid-noeol.sup'
Write-Host "  frames: $framesNoEol"

Write-Host 'hdr, trailing line break'
$framesHdr = Convert-Sub -In 'test\solid.ass' -Out '.tmp\solid-hdr.sup' -Extra @('-h')
Write-Host "  frames: $framesHdr"

$sdr = Get-Md5 '.tmp\solid.sup'
$noeol = Get-Md5 '.tmp\solid-noeol.sup'
$hdr = Get-Md5 '.tmp\solid-hdr.sup'
$size = (Get-Item '.tmp\solid.sup').Length
$sizeNoEol = (Get-Item '.tmp\solid-noeol.sup').Length
$sizeHdr = (Get-Item '.tmp\solid-hdr.sup').Length

Write-Host ''
Write-Host ("sdr   {0}  {1} bytes" -f $sdr, $size)
Write-Host ("noeol {0}  {1} bytes" -f $noeol, $sizeNoEol)
Write-Host ("hdr   {0}  {1} bytes" -f $hdr, $sizeHdr)
Write-Host ''

$failures = @()

# The exit code is part of the interface: a caller in a pipeline or a CI step
# branches on it, so a bad command line must not be indistinguishable from a
# run that worked, and -help must not look like a failure.
$saved = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try {
    & $exe -i 'test\solid.ass' -s 9999 -r 23 '.tmp\useless.sup' 2>&1 | Out-Null
    $usageCode = $LASTEXITCODE
    & $exe -help 2>&1 | Out-Null
    $helpCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $saved
}
Write-Host "exit codes: usage $usageCode, help $helpCode"
if ($usageCode -eq 0) { $failures += 'a bad command line exited with 0, which reads as success' }
if ($helpCode -ne 0) { $failures += "-help exited with $helpCode instead of 0" }

if ($frames -ne 120) { $failures += "expected 120 frames, got $frames" }
if ($framesNoEol -ne $frames) { $failures += 'a missing trailing line break changed the frame count' }
if ($noeol -ne $sdr) { $failures += 'a missing trailing line break changed the stream (issue #6)' }
if ($size -lt 3000) { $failures += "the sdr stream is only $size bytes, so no graphics were encoded" }
if ($hdr -eq $sdr) { $failures += '-h produced the same stream as the sdr build, so the hdr path is not reached' }
if ($sizeHdr -lt 3000) { $failures += "the hdr stream is only $sizeHdr bytes" }

if ($failures.Count -gt 0) {
    foreach ($f in $failures) { Write-Host "FAIL: $f" -ForegroundColor Red }
    exit 1
}

Write-Host 'regression: all checks passed' -ForegroundColor Green
exit 0
