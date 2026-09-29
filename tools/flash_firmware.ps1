# flash_firmware.ps1 — 烧录合并固件（0x0 起）
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File tools\flash_firmware.ps1 -Port COM5 -Firmware <path>\merged-firmware.bin
#
# 纪律（同样写在 AGENTS.md）：
#   ★ 未经用户明确确认，禁止烧录！本脚本会强制要求输入 yes 二次确认。
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [Parameter(Mandatory = $true)][string]$Firmware,
    [int]$Baud = 460800
)

$ErrorActionPreference = "Stop"

function Fail($msg) { Write-Host "[错误] $msg" -ForegroundColor Red; exit 1 }

if (-not (Test-Path $Firmware)) { Fail "固件不存在: $Firmware" }
$fw = (Resolve-Path $Firmware).Path
$size = (Get-Item $fw).Length

Write-Host "================================================"
Write-Host " 目标串口 : $Port"
Write-Host " 固件     : $fw"
Write-Host " 大小     : $size 字节"
Write-Host " 烧录地址 : 0x0（整片合并镜像）"
Write-Host "================================================"
Write-Host "即将烧录。烧录会覆盖设备现有固件！" -ForegroundColor Yellow

$answer = Read-Host "确认烧录请输入 yes（其他任意输入取消）"
if ($answer -ne "yes") {
    Write-Host "已取消，未烧录。" -ForegroundColor Yellow
    exit 3
}

python -m esptool --chip esp32s3 -p $Port -b $Baud write_flash 0x0 $fw
if ($LASTEXITCODE -ne 0) { Fail "烧录失败" }

Write-Host "[OK] 烧录完成。可复位查看串口日志。" -ForegroundColor Green
