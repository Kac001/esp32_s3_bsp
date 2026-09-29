# build_firmware.ps1 — 编译工程并生成可从 0x0 一键烧录的合并固件
#
# 用法（在 ESP-IDF 环境中，或已设置 IDF_PATH）：
#   powershell -ExecutionPolicy Bypass -File tools\build_firmware.ps1 -Project examples\bsp_smoke
#
# 产物：
#   <project>\build\merged-firmware.bin   （0x0 起连续镜像：bootloader+分区表+boot_app0+app+数据分区）
#   <project>\build\flash_args.txt        （分段烧录清单，备用）
#
# 说明：合并偏移取自构建产物 build/flasher_args.json，自动适配任意分区表。
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$Out = ""
)

$ErrorActionPreference = "Stop"

function Fail($msg) { Write-Host "[错误] $msg" -ForegroundColor Red; exit 1 }

# ---- 1. 定位 ESP-IDF ----
$projDir = (Resolve-Path $Project).Path
$buildDir = Join-Path $projDir "build"
Write-Host "== 工程: $projDir"

if (-not $env:IDF_PATH) {
    Fail "未设置 IDF_PATH。请先运行 ESP-IDF 的 export.ps1（如 . C:\Espressif\frameworks\esp-idf-v5.1.4\export.ps1）"
}
$python = Join-Path $env:IDF_PATH "tools\idf_tools.py"
if (-not (Test-Path $python)) { Fail "IDF_PATH 无效: $env:IDF_PATH" }

# idf.py 通常在 IDF_PATH\tools 下（export 之后已加入 PATH）
$idfPy = Get-Command idf.py -ErrorAction SilentlyContinue
if (-not $idfPy) {
    $idfPyPath = Join-Path $env:IDF_PATH "tools\idf.py"
    if (Test-Path $idfPyPath) { $idfPy = $idfPyPath } else { Fail "找不到 idf.py" }
} else { $idfPy = $idfPy.Source }

# ---- 2. 编译 ----
Write-Host "== idf.py build"
Push-Location $projDir
& $idfPy build
if ($LASTEXITCODE -ne 0) { Pop-Location; Fail "编译失败" }
Pop-Location

# ---- 3. 解析 flasher_args.json ----
$faPath = Join-Path $buildDir "flasher_args.json"
if (-not (Test-Path $faPath)) { Fail "缺少 $faPath（构建是否成功？）" }
$fa = Get-Content $faPath -Raw | ConvertFrom-Json

$pairs = @()
foreach ($p in $fa.flash_files.PSObject.Properties) {
    $offset = [Convert]::ToInt32($p.Name, 16)   # 按偏移升序合并
    $pairs += [pscustomobject]@{ Offset = $offset; File = Join-Path $buildDir $p.Value }
}
$pairs = $pairs | Sort-Object Offset

Write-Host "== 烧录映像清单:"
foreach ($p in $pairs) {
    if (-not (Test-Path $p.File)) { Fail "缺少映像文件: $($p.File)" }
    Write-Host ("   0x{0:x}  {1}" -f $p.Offset, (Split-Path $p.File -Leaf))
}

# ---- 4. 合并 ----
$merged = if ($Out) { $Out } else { Join-Path $buildDir "merged-firmware.bin" }
$mergeArgs = @("--chip", "esp32s3", "merge_bin", "-o", $merged,
               "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "16MB")
foreach ($p in $pairs) {
    $mergeArgs += ("0x{0:x}" -f $p.Offset)
    $mergeArgs += $p.File
}

Write-Host "== esptool merge_bin -> $merged"
python -m esptool @mergeArgs
if ($LASTEXITCODE -ne 0) { Fail "合并失败（python -m esptool 不可用？）" }

# ---- 5. 备用：分段烧录清单 ----
$flashArgs = Join-Path $buildDir "flash_args.txt"
$lines = foreach ($p in $pairs) { ("0x{0:x} {1}" -f $p.Offset, $p.File) }
Set-Content -Path $flashArgs -Value $lines -Encoding UTF8

$size = (Get-Item $merged).Length
Write-Host ""
Write-Host ("[OK] 合并固件: {0}  ({1:N0} 字节)" -f $merged, $size) -ForegroundColor Green
Write-Host "     烧录命令: powershell -File tools\flash_firmware.ps1 -Port <串口> -Firmware `"$merged`""
Write-Host "     （烧录前须经用户确认，未确认请勿烧录）"
