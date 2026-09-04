[CmdletBinding()]
param(
    [string] $PortableDirectory,
    [string] $OutputDirectory,
    [string] $IdentityFile,
    [string] $MakeAppxPath,
    [string] $Version = '4.4.18.0'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = (Resolve-Path -LiteralPath (Join-Path $scriptDirectory '..\..')).Path

if ([string]::IsNullOrWhiteSpace($PortableDirectory)) {
    $PortableDirectory = Join-Path $repositoryRoot 'dist\xnec2c-windows-x64-ucrt64'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'dist\msix'
}
if ([string]::IsNullOrWhiteSpace($IdentityFile)) {
    $IdentityFile = Join-Path $scriptDirectory 'msix\StoreIdentity.json'
}

$manifestTemplate = Join-Path $scriptDirectory 'msix\AppxManifest.xml.in'
$sourceLogo = Join-Path $repositoryRoot 'files\xnec2c.png'
$requiredFiles = @(
    (Join-Path $PortableDirectory 'xnec2c-launcher.exe'),
    (Join-Path $PortableDirectory 'bin\xnec2c.exe'),
    $manifestTemplate,
    $sourceLogo,
    $IdentityFile
)
foreach ($requiredFile in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "MSIX input is missing: $requiredFile"
    }
}

$identity = Get-Content -LiteralPath $IdentityFile -Raw | ConvertFrom-Json
if ($identity.PackageName -notmatch '^[A-Za-z0-9.-]{3,50}$') {
    throw 'PackageName must contain 3-50 letters, digits, periods, or hyphens.'
}
if ([string]::IsNullOrWhiteSpace($identity.Publisher) -or
    [string]::IsNullOrWhiteSpace($identity.PublisherDisplayName)) {
    throw 'Publisher and PublisherDisplayName are required.'
}
if ($Version -notmatch '^\d+\.\d+\.\d+\.\d+$') {
    throw 'MSIX Version must contain four numeric components.'
}
if (-not $identity.StoreIdentityConfigured) {
    Write-Warning 'Using development identity. Reserve the app in Partner Center and replace StoreIdentity.json before Store submission.'
}

if ([string]::IsNullOrWhiteSpace($MakeAppxPath)) {
    $kitsBin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $MakeAppxPath = Get-ChildItem -LiteralPath $kitsBin -Filter MakeAppx.exe -Recurse |
        Where-Object { $_.FullName -match '\\x64\\MakeAppx\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -ExpandProperty FullName -First 1
}
if ([string]::IsNullOrWhiteSpace($MakeAppxPath) -or
    -not (Test-Path -LiteralPath $MakeAppxPath -PathType Leaf)) {
    throw 'MakeAppx.exe was not found in the Windows 10/11 SDK.'
}

$stagingDirectory = Join-Path $env:RUNNER_TEMP 'xnec2c-msix-stage'
$verificationDirectory = Join-Path $env:RUNNER_TEMP 'xnec2c-msix-verify'
Remove-Item -LiteralPath $stagingDirectory -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $verificationDirectory -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stagingDirectory | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stagingDirectory 'Assets') | Out-Null
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
Copy-Item -Path (Join-Path $PortableDirectory '*') -Destination $stagingDirectory -Recurse -Force

Add-Type -AssemblyName System.Drawing
function New-AppLogo {
    param(
        [Parameter(Mandatory)] [string] $InputPath,
        [Parameter(Mandatory)] [string] $OutputPath,
        [Parameter(Mandatory)] [int] $Width,
        [Parameter(Mandatory)] [int] $Height
    )
    $source = [System.Drawing.Image]::FromFile($InputPath)
    $canvas = New-Object System.Drawing.Bitmap($Width, $Height)
    $graphics = [System.Drawing.Graphics]::FromImage($canvas)
    try {
        $graphics.Clear([System.Drawing.Color]::FromArgb(255, 245, 247, 250))
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $margin = [Math]::Max(2, [Math]::Floor([Math]::Min($Width, $Height) * 0.12))
        $scale = [Math]::Min(($Width - 2 * $margin) / $source.Width, ($Height - 2 * $margin) / $source.Height)
        $drawWidth = [Math]::Max(1, [int][Math]::Round($source.Width * $scale))
        $drawHeight = [Math]::Max(1, [int][Math]::Round($source.Height * $scale))
        $left = [int](($Width - $drawWidth) / 2)
        $top = [int](($Height - $drawHeight) / 2)
        $graphics.DrawImage($source, $left, $top, $drawWidth, $drawHeight)
        $canvas.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $canvas.Dispose()
        $source.Dispose()
    }
}

$assets = Join-Path $stagingDirectory 'Assets'
New-AppLogo $sourceLogo (Join-Path $assets 'StoreLogo.png') 50 50
New-AppLogo $sourceLogo (Join-Path $assets 'Square44x44Logo.png') 44 44
New-AppLogo $sourceLogo (Join-Path $assets 'Square150x150Logo.png') 150 150
New-AppLogo $sourceLogo (Join-Path $assets 'Wide310x150Logo.png') 310 150
New-AppLogo $sourceLogo (Join-Path $assets 'Square310x310Logo.png') 310 310

$manifest = Get-Content -LiteralPath $manifestTemplate -Raw
$manifest = $manifest.Replace('@@PACKAGE_NAME@@', [System.Security.SecurityElement]::Escape([string]$identity.PackageName))
$manifest = $manifest.Replace('@@PUBLISHER@@', [System.Security.SecurityElement]::Escape([string]$identity.Publisher))
$manifest = $manifest.Replace('@@PUBLISHER_DISPLAY_NAME@@', [System.Security.SecurityElement]::Escape([string]$identity.PublisherDisplayName))
$manifest = $manifest.Replace('@@VERSION@@', $Version)
$manifestPath = Join-Path $stagingDirectory 'AppxManifest.xml'
$manifest | Set-Content -LiteralPath $manifestPath -Encoding utf8NoBOM
[void]([xml](Get-Content -LiteralPath $manifestPath -Raw))

$outputPackage = Join-Path $OutputDirectory "Xnec2c-$($Version.Substring(0, $Version.LastIndexOf('.')))-Windows-x64.msix"
Remove-Item -LiteralPath $outputPackage -Force -ErrorAction SilentlyContinue
Write-Host "Using MakeAppx: $MakeAppxPath"
& $MakeAppxPath pack /d $stagingDirectory /p $outputPackage /o
if ($LASTEXITCODE -ne 0) {
    throw "MakeAppx pack failed with exit code $LASTEXITCODE"
}

& $MakeAppxPath unpack /p $outputPackage /d $verificationDirectory /o
if ($LASTEXITCODE -ne 0) {
    throw "MakeAppx verification unpack failed with exit code $LASTEXITCODE"
}
$unpackedManifest = Join-Path $verificationDirectory 'AppxManifest.xml'
[void]([xml](Get-Content -LiteralPath $unpackedManifest -Raw))

$package = Get-Item -LiteralPath $outputPackage
$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $outputPackage
Write-Host "MSIX created: $($package.FullName)"
Write-Host "MSIX size: $($package.Length) bytes"
Write-Host "MSIX SHA-256: $($hash.Hash.ToLowerInvariant())"
