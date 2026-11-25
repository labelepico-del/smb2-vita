param(
    [string]$WorkDir = (Join-Path $env:TEMP "smb2-vita-build")
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$commit = "b73415e1953d7fdffd4957d5fb43fa7a13844c1c"
$url = "https://codeload.github.com/Xkeeper0/smb2/tar.gz/$commit"
$prg0 = "47ba60fad332fdea5ae44b7979fe1ee78de1d316ee027fea2ad5fe3c0d86f25a"
$tar = Join-Path $WorkDir "smb2.tar.gz"
$src = Join-Path $WorkDir "smb2-$commit"

if (Test-Path $WorkDir) {
    Remove-Item -Recurse -Force $WorkDir
}
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

Write-Host "Downloading $url ..."
Invoke-WebRequest -Uri $url -OutFile $tar
Write-Host "Extracting ..."
tar -xzf $tar -C $WorkDir

if (-not (Test-Path $src)) {
    throw "Expected source directory not found: $src"
}

Push-Location $src
try {
    New-Item -ItemType Directory -Force -Path bin | Out-Null
    Write-Host "Assembling ..."
    & ".\tools\asm6f.exe" smb2.asm -n -c -L bin/smb2.nes
    if ($LASTEXITCODE -ne 0) {
        throw "asm6f failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

$rom = Join-Path $src "bin\smb2.nes"
$actual = (Get-FileHash $rom -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "SHA-256: $actual"
if ($actual -ne $prg0) {
    throw "Built ROM does not match SMB2 PRG0"
}

$outRomDir = Join-Path $root "rom"
New-Item -ItemType Directory -Force -Path $outRomDir | Out-Null
$outRom = Join-Path $outRomDir "smb2.nes"
Copy-Item $rom $outRom -Force

& (Join-Path $PSScriptRoot "embed_rom.ps1") -Rom $outRom -Output (Join-Path $root "src\rom_data.cpp")
Write-Host "Done. Build the VPK with: cmake -S `"$root`" -B `"$root\build`" ; cmake --build `"$root\build`""
