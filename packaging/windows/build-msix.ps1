param(
  [string]$PackageRoot = "dist/xnec2c-windows-x64-ucrt64",
  [string]$OutputDir = "dist/msix",
  [string]$IdentityName = $env:XNEC2C_MSIX_IDENTITY_NAME,
  [string]$Publisher = $env:XNEC2C_MSIX_PUBLISHER,
  [string]$PublisherDisplayName = $env:XNEC2C_MSIX_PUBLISHER_DISPLAY_NAME,
  [string]$Version = "4.4.18.0"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($IdentityName)) { $IdentityName = "CharlyGolf.Xnec2c" }
if ([string]::IsNullOrWhiteSpace($Publisher)) { $Publisher = "CN=22DE0707-9E03-4F93-9B58-1F1C7076D4F9" }
if ([string]::IsNullOrWhiteSpace($PublisherDisplayName)) { $PublisherDisplayName = "CharlyGolf" }

$repo = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$packageRootPath = (Resolve-Path (Join-Path $repo $PackageRoot)).Path
$outputDirPath = Join-Path $repo $OutputDir
$layout = Join-Path $repo "dist/msix-layout"

if (Test-Path $layout) { Remove-Item -Recurse -Force $layout }
if (Test-Path $outputDirPath) { Remove-Item -Recurse -Force $outputDirPath }
New-Item -ItemType Directory -Force -Path $layout, $outputDirPath | Out-Null

Copy-Item -Recurse -Force (Join-Path $packageRootPath "*") $layout

$assets = Join-Path $layout "Assets"
New-Item -ItemType Directory -Force -Path $assets | Out-Null

Add-Type -AssemblyName System.Drawing
function New-Xnec2cLogo {
  param([string]$Path, [int]$Width, [int]$Height)
  $sourcePath = Join-Path $repo "files/xnec2c.png"
  if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
    throw "Original Xnec2c icon not found: $sourcePath"
  }

  $source = [System.Drawing.Image]::FromFile($sourcePath)
  $bmp = [System.Drawing.Bitmap]::new($Width, $Height)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  try {
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $size = [Math]::Min($Width, $Height)
    $x = [Math]::Floor(($Width - $size) / 2)
    $y = [Math]::Floor(($Height - $size) / 2)
    $g.DrawImage($source, $x, $y, $size, $size)
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
  } finally {
    $g.Dispose()
    $bmp.Dispose()
    $source.Dispose()
  }
}

New-Xnec2cLogo (Join-Path $assets "Square44x44Logo.png") 44 44
New-Xnec2cLogo (Join-Path $assets "StoreLogo.png") 50 50
New-Xnec2cLogo (Join-Path $assets "Square150x150Logo.png") 150 150
New-Xnec2cLogo (Join-Path $assets "Wide310x150Logo.png") 310 150

$template = Get-Content -Raw (Join-Path $PSScriptRoot "AppxManifest.xml.in")
$manifest = $template.Replace("@IDENTITY_NAME@", $IdentityName)
$manifest = $manifest.Replace("@PUBLISHER@", $Publisher)
$manifest = $manifest.Replace("@PUBLISHER_DISPLAY_NAME@", $PublisherDisplayName)
$manifest = $manifest.Replace("@VERSION@", $Version)
[System.IO.File]::WriteAllText((Join-Path $layout "AppxManifest.xml"), $manifest, [System.Text.UTF8Encoding]::new($false))

$sdkBin = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Directory |
  Where-Object { $_.Name -match '^10\.0\.' } |
  Sort-Object Name -Descending |
  Select-Object -First 1
if (-not $sdkBin) { throw "Windows SDK bin directory not found" }

$makeAppx = Join-Path $sdkBin.FullName "x64\makeappx.exe"
$signTool = Join-Path $sdkBin.FullName "x64\signtool.exe"
if (-not (Test-Path $makeAppx)) { throw "makeappx.exe not found: $makeAppx" }
if (-not (Test-Path $signTool)) { throw "signtool.exe not found: $signTool" }

$msix = Join-Path $outputDirPath "Xnec2c-4.4.18-Windows-x64.msix"
& $makeAppx pack /d $layout /p $msix /o
if ($LASTEXITCODE -ne 0) { throw "makeappx pack failed" }

# For CI/local validation, sign with an ephemeral self-signed certificate whose
# subject exactly matches the development Publisher. Store production builds
# can override identity/publisher with Partner Center values and be Store-signed.
$cert = New-SelfSignedCertificate -Type Custom -Subject $Publisher -KeyUsage DigitalSignature `
  -FriendlyName "Xnec2c MSIX CI Development" -CertStoreLocation "Cert:\CurrentUser\My" `
  -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")
$password = ConvertTo-SecureString -String "xnec2c-ci-msix" -Force -AsPlainText
$pfx = Join-Path $outputDirPath "Xnec2c-MSIX-Development.pfx"
$cer = Join-Path $outputDirPath "Xnec2c-MSIX-Development.cer"
Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $password | Out-Null
Export-Certificate -Cert $cert -FilePath $cer | Out-Null

& $signTool sign /fd SHA256 /f $pfx /p "xnec2c-ci-msix" $msix
if ($LASTEXITCODE -ne 0) { throw "signtool sign failed" }

# A CI self-signed certificate is expected to report an untrusted root.
# Accept only that trust error; all other signature verification errors fail.
$verifyOutput = (& $signTool verify /pa /v $msix 2>&1) | Out-String
Write-Host $verifyOutput
if ($LASTEXITCODE -ne 0 -and $verifyOutput -notmatch "terminated in a root\s+certificate which is not trusted") {
  throw "signtool verify failed"
}

# Do not publish the ephemeral private key or leave it in the certificate store.
Remove-Item -Force $pfx
Remove-Item -Force "Cert:\CurrentUser\My\$($cert.Thumbprint)" -ErrorAction SilentlyContinue

$unpack = Join-Path $repo "dist/msix-unpacked"
if (Test-Path $unpack) { Remove-Item -Recurse -Force $unpack }
& $makeAppx unpack /p $msix /d $unpack /o
if ($LASTEXITCODE -ne 0) { throw "makeappx unpack validation failed" }

Get-FileHash -Algorithm SHA256 $msix | Format-List
Write-Host "MSIX created: $msix"
Write-Host "Development certificate: $cer"
