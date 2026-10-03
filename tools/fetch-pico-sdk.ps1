$ErrorActionPreference = "Stop"

$SdkTag = "2.3.1"
$SdkCommit = "079c6f39023649b154152db30f1d781e884879bc"
$TinyUsbCommit = "86ad6e56c1700e85f1c5678607a762cfe3aa2f47"
$Cyw43Commit = "055d64274b014dd7b1c2fc94d26e8a18face7124"
$BtstackCommit = "eb0bb8b5ea6d234ccb940313b47f7a5c3b4e20ec"

$RootDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$SdkDir = if ($env:PICO_SDK_PATH) {
    $env:PICO_SDK_PATH
} else {
    Join-Path $RootDir ".deps\pico-sdk"
}

$Parent = Split-Path -Parent $SdkDir
New-Item -ItemType Directory -Force -Path $Parent | Out-Null

if ((Test-Path $SdkDir) -and -not (Test-Path (Join-Path $SdkDir ".git"))) {
    throw "$SdkDir exists but is not a git checkout"
}

if (-not (Test-Path (Join-Path $SdkDir ".git"))) {
    git clone --branch $SdkTag --depth 1 https://github.com/raspberrypi/pico-sdk.git $SdkDir
    if ($LASTEXITCODE -ne 0) { throw "git clone failed" }
} else {
    $Refspec = "refs/tags/$SdkTag" + ":refs/tags/$SdkTag"
    git -C $SdkDir fetch --depth 1 origin $Refspec
    if ($LASTEXITCODE -ne 0) { throw "git fetch failed" }
}

git -C $SdkDir checkout --detach $SdkCommit
if ($LASTEXITCODE -ne 0) { throw "git checkout failed" }

git -C $SdkDir submodule sync --recursive
if ($LASTEXITCODE -ne 0) { throw "git submodule sync failed" }

git -C $SdkDir submodule update --init --recursive --depth 1
if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }

function Assert-Commit([string]$Path, [string]$Expected) {
    $Actual = (git -C $Path rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw "git rev-parse failed for $Path" }
    if ($Actual -ne $Expected) {
        throw "$Path is $Actual, expected $Expected"
    }
}

Assert-Commit $SdkDir $SdkCommit
Assert-Commit (Join-Path $SdkDir "lib\tinyusb") $TinyUsbCommit
Assert-Commit (Join-Path $SdkDir "lib\cyw43-driver") $Cyw43Commit
Assert-Commit (Join-Path $SdkDir "lib\btstack") $BtstackCommit

Write-Host "Pico SDK ready: $SdkDir"
Write-Host "  pico-sdk:     $SdkCommit"
Write-Host "  tinyusb:      $TinyUsbCommit"
Write-Host "  cyw43-driver: $Cyw43Commit"
Write-Host "  btstack:      $BtstackCommit"
