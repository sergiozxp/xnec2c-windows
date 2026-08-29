[CmdletBinding()]
param(
    [string] $CompilerPath
)

$ErrorActionPreference = 'Stop'

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $scriptDirectory '..\..')).Path
$portableDirectory = Join-Path $repositoryRoot 'dist\xnec2c-windows-x64-ucrt64'
$installerScript = Join-Path $scriptDirectory 'xnec2c.iss'
$expectedSetup = Join-Path $repositoryRoot 'dist\installer\Xnec2c-4.4.18-Windows-x64-Setup.exe'

$requiredFiles = @(
    (Join-Path $portableDirectory 'xnec2c-launcher.exe'),
    (Join-Path $portableDirectory 'bin\xnec2c.exe'),
    (Join-Path $portableDirectory 'BUILDINFO.txt'),
    (Join-Path $portableDirectory 'SHA256SUMS')
)

foreach ($requiredFile in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Portable package is incomplete; missing: $requiredFile"
    }
}

if ([string]::IsNullOrWhiteSpace($CompilerPath)) {
    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        $CompilerPath = $command.Source
    }
}

if ([string]::IsNullOrWhiteSpace($CompilerPath)) {
    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 7\ISCC.exe')
    )
    $CompilerPath = $candidates |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_ -PathType Leaf) } |
        Select-Object -First 1
}

if ([string]::IsNullOrWhiteSpace($CompilerPath) -or
    -not (Test-Path -LiteralPath $CompilerPath -PathType Leaf)) {
    throw 'ISCC.exe was not found. Install Inno Setup 6.3 or newer, or pass -CompilerPath.'
}

Write-Host "Using Inno Setup compiler: $CompilerPath"
& $CompilerPath $installerScript
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compilation failed with exit code $LASTEXITCODE"
}

if (-not (Test-Path -LiteralPath $expectedSetup -PathType Leaf)) {
    throw "Expected installer was not produced: $expectedSetup"
}

$setupFile = Get-Item -LiteralPath $expectedSetup
$setupHash = Get-FileHash -Algorithm SHA256 -LiteralPath $expectedSetup
Write-Host "Installer created: $($setupFile.FullName)"
Write-Host "Installer size: $($setupFile.Length) bytes"
Write-Host "Installer SHA-256: $($setupHash.Hash.ToLowerInvariant())"
