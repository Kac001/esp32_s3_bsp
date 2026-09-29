#!/usr/bin/env bash
# build_firmware.sh — 编译工程并生成可从 0x0 一键烧录的合并固件（Linux/macOS/CI 版）
#
# 用法（先 source ESP-IDF 的 export.sh）：
#   ./tools/build_firmware.sh examples/bsp_smoke
#
# 产物：<project>/build/merged-firmware.bin
set -euo pipefail

PROJECT="${1:?用法: $0 <工程目录>}"
BUILD="$PROJECT/build"

[ -n "${IDF_PATH:-}" ] || { echo "[错误] 未设置 IDF_PATH，请先 source export.sh"; exit 1; }
command -v idf.py >/dev/null || { echo "[错误] 找不到 idf.py"; exit 1; }

echo "== 工程: $PROJECT"
(cd "$PROJECT" && idf.py build)

FA="$BUILD/flasher_args.json"
[ -f "$FA" ] || { echo "[错误] 缺少 $FA"; exit 1; }

# 提取 offset/file 并按偏移升序合并
mapfile -t LINES < <(python3 - "$FA" "$BUILD" <<'EOF'
import json, sys, os
fa = json.load(open(sys.argv[1], encoding="utf-8"))
build = sys.argv[2]
items = sorted(((int(k, 16), v) for k, v in fa["flash_files"].items()))
for off, rel in items:
    print(f"{off:x} {os.path.join(build, rel)}")
EOF
)

ARGS=(--chip esp32s3 merge_bin -o "$BUILD/merged-firmware.bin" --flash_mode dio --flash_freq 80m --flash_size 16MB)
echo "== 烧录映像清单:"
for line in "${LINES[@]}"; do
  off="${line%% *}"; file="${line#* }"
  [ -f "$file" ] || { echo "[错误] 缺少映像: $file"; exit 1; }
  printf '   0x%-7s %s\n' "$off" "$(basename "$file")"
  ARGS+=("0x$off" "$file")
done

echo "== esptool merge_bin"
python -m esptool "${ARGS[@]}"

ls -l "$BUILD/merged-firmware.bin"
echo "[OK] 合并固件: $BUILD/merged-firmware.bin"
echo "     烧录（需用户确认）: python -m esptool --chip esp32s3 -p <串口> write_flash 0x0 $BUILD/merged-firmware.bin"
