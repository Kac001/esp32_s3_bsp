#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_font_coverage.py — 中文界面字体覆盖检查工具
==================================================

用途：
    在“重新设计应用界面”之后、编译固件之前，自动检查 UI 源码里用到的
    每一个字符（重点是中文）是否都被所选 LVGL 字体覆盖，避免界面上出现
    “豆腐块”（缺字方框）。

能力：
    1. 解析 lv_font_conv 生成的 LVGL 格式字体 C 文件（*.c）的 cmap 表，
       还原字体实际覆盖的 Unicode 码点集合；
    2. 扫描 C/C++ 源码中的字符串字面量（自动跳过注释、解码 \\xNN / \\uXXXX
       等转义），收集界面文案实际用到的字符；
    3. 对比并输出缺失字符清单（U+XXXX、出现次数、示例文件:行号）；
    4. `--emit-symbols` 输出去重字符集，可直接喂给 lv_font_conv 的
       `--symbols` 参数生成“刚好覆盖”的子集字体，节省 Flash；
    5. `--range` 支持不解析字体文件、直接按 Unicode 区间做规划性检查。

典型工作流（中文 UI 字体三步法）：
    第一步 提取字符集：
        python tools/check_font_coverage.py --src app/ --emit-symbols symbols.txt
    第二步 用字符集生成字体（示例）：
        lv_font_conv --bpp 4 --size 20 --no-compress \
            --font NotoSansSC-Regular.otf --symbols @symbols.txt \
            --format lvgl -o app_fonts.c
    第三步 校验覆盖：
        python tools/check_font_coverage.py --font app_fonts.c --src app/

退出码：
    0 = 全部覆盖；1 = 有缺失字符；2 = 参数或解析错误。
"""

import argparse
import json
import os
import re
import sys

C_EXTS = {".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp"}

# ---------------------------------------------------------------------------
# 字体解析：lv_font_conv --format lvgl 生成的 C 文件
# ---------------------------------------------------------------------------

_CMAP_BLOCK_RE = re.compile(r"\{([^{}]*)\}", re.S)
_UNICODE_LIST_RE = re.compile(
    r"static\s+const\s+uint16_t\s+(unicode_list_\d+)\s*\[\s*\]\s*=\s*\{([^}]*)\}", re.S
)


def _ints_in(text):
    out = []
    for m in re.finditer(r"(-?0[xX][0-9a-fA-F]+|-?\d+)", text or ""):
        s = m.group(1)
        out.append(int(s, 16) if s.lower().lstrip("-").startswith("0x") else int(s, 10))
    return out


def parse_lvgl_font(path):
    """解析 LVGL 字体 .c，返回 {"codepoints": set, "cmaps": n, "glyphs": n}"""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()

    lists = {name: _ints_in(body) for name, body in _UNICODE_LIST_RE.findall(text)}

    # 定位 cmaps[] 数组体，避免误匹配其他数组
    m = re.search(r"lv_font_fmt_txt_cmap_t\s+cmaps\s*\[\s*\]\s*=\s*\{(.*?)\n\s*\};", text, re.S)
    body = m.group(1) if m else text

    codepoints = set()
    cmap_count = 0
    for block in _CMAP_BLOCK_RE.findall(body):
        if ".range_start" not in block:
            continue
        cmap_count += 1
        f_start = re.search(r"\.range_start\s*=\s*(\d+)", block)
        f_len = re.search(r"\.range_length\s*=\s*(\d+)", block)
        f_list = re.search(r"\.unicode_list\s*=\s*(NULL|unicode_list_\d+)", block)
        f_cnt = re.search(r"\.list_length\s*=\s*(\d+)", block)
        if not (f_start and f_len):
            continue
        start = int(f_start.group(1))
        length = int(f_len.group(1))
        list_name = f_list.group(1) if f_list else "NULL"
        list_len = int(f_cnt.group(1)) if f_cnt else 0

        if list_name == "NULL" or list_len == 0:
            # FORMAT0_*：连续区间全覆盖
            codepoints.update(range(start, start + length))
        else:
            # SPARSE_*：unicode_list 存的是相对 range_start 的偏移
            values = lists.get(list_name, [])[:list_len]
            for v in values:
                cp = start + v if v < length else v
                codepoints.update(range(start, start + length)) if False else codepoints.add(cp)

    gm = re.search(r"glyph_id_start\s*=\s*(\d+)", text)
    return {
        "path": path,
        "codepoints": codepoints,
        "cmaps": cmap_count,
        "lists": len(lists),
        "note": "" if cmap_count else "未找到 lv_font_fmt_txt_cmap_t cmaps[]（是否为 LVGL 字体格式？）",
    }


def parse_ranges(spec):
    """解析 '0x4E00-0x9FFF,32-126,0x3002' 形式的区间/码点列表"""
    out = set()
    for part in re.split(r"[,\s]+", spec.strip()):
        if not part:
            continue
        if "-" in part:
            lo, hi = part.split("-", 1)
            out.update(range(int(lo, 0), int(hi, 0) + 1))
        else:
            out.add(int(part, 0))
    return out


# ---------------------------------------------------------------------------
# 源码扫描：提取字符串字面量中的字符
# ---------------------------------------------------------------------------

def extract_string_chars(path):
    """返回 [(lineno, text), ...]：源码中所有普通字符串字面量的内容"""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        src = f.read()

    out, buf = [], []
    i, n, line = 0, len(src), 1
    in_str, in_chr, in_line, in_block = False, False, False, False
    start_line = 1

    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if c == "\n":
            line += 1
            if in_line:
                in_line = False
            if in_str and not in_block:  # 未闭合字符串，容错结束
                out.append((start_line, "".join(buf)))
                buf, in_str = [], False
            i += 1
            continue
        if in_line:
            i += 1
            continue
        if in_block:
            if c == "*" and nxt == "/":
                in_block = False
                i += 2
                continue
            i += 1
            continue
        if in_str or in_chr:
            if c == "\\":
                # 转义序列
                if nxt in ("x", "X"):
                    m = re.match(r"\\x([0-9a-fA-F]{1,2})", src[i:])
                    if m:
                        if in_str:
                            buf.append(chr(int(m.group(1), 16)))
                        i += m.end()
                        continue
                if nxt in ("u", "U"):
                    width = 4 if nxt == "u" else 8
                    hexpart = src[i + 2:i + 2 + width]
                    if len(hexpart) == width and re.fullmatch(r"[0-9a-fA-F]+", hexpart):
                        if in_str:
                            buf.append(chr(int(hexpart, 16)))
                        i += 2 + width
                        continue
                if nxt.isdigit():
                    m = re.match(r"\\([0-7]{1,3})", src[i:])
                    if m:
                        if in_str:
                            buf.append(chr(int(m.group(1), 8)))
                        i += m.end()
                        continue
                simple = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "\\": "\\",
                          "'": "'", '"': '"', "a": "\a", "b": "\b", "f": "\f", "v": "\v"}
                if nxt in simple and in_str:
                    buf.append(simple[nxt])
                i += 2
                continue
            if c == '"':
                if in_str:
                    out.append((start_line, "".join(buf)))
                    buf, in_str = [], False
                else:
                    in_str, start_line = True, line
                i += 1
                continue
            if c == "'":
                in_chr = not in_chr
                i += 1
                continue
            if in_str:
                buf.append(c)
            i += 1
            continue
        # 不在字符串/字符/注释中
        if c == "/" and nxt == "/":
            in_line = True
            i += 2
            continue
        if c == "/" and nxt == "*":
            in_block = True
            i += 2
            continue
        if c == '"':
            in_str, start_line = True, line
            i += 1
            continue
        if c == "'":
            in_chr = True
            i += 1
            continue
        i += 1

    return out


def collect_sources(paths, exts):
    files = []
    for p in paths:
        if os.path.isfile(p):
            files.append(p)
        elif os.path.isdir(p):
            for root, _dirs, names in os.walk(p):
                for name in sorted(names):
                    if os.path.splitext(name)[1].lower() in exts:
                        files.append(os.path.join(root, name))
        else:
            print(f"[警告] 路径不存在，已跳过：{p}", file=sys.stderr)
    return files


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(
        description="中文界面字体覆盖检查：对比 LVGL 字体覆盖范围与源码实际用字",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--font", action="append", default=[],
                    help="lv_font_conv 生成的 LVGL 字体 .c 文件（可多次）")
    ap.add_argument("--range", dest="ranges", default=None,
                    help="不解析字体文件，按 Unicode 区间检查，如 '0x4E00-0x9FFF,32-126'")
    ap.add_argument("--src", action="append", default=[],
                    help="待扫描的源码文件或目录（可多次）")
    ap.add_argument("--ext", default=None,
                    help="扫描的扩展名，逗号分隔（默认 %s）" % ",".join(sorted(C_EXTS)))
    ap.add_argument("--strings-file", action="append", default=[],
                    help="额外的纯文本字符来源（可多次）")
    ap.add_argument("--chars", default=None, help="额外的字面字符")
    ap.add_argument("--emit-symbols", nargs="?", const="-", default=None, metavar="FILE",
                    help="输出去重字符集（供 lv_font_conv --symbols 使用），默认打印到 stdout")
    ap.add_argument("--report", default=None, help="把检查报告写成 JSON 文件")
    ap.add_argument("--skip-ascii", action="store_true",
                    help="只检查非 ASCII 字符（中文等），跳过 ASCII")
    ap.add_argument("--quiet", action="store_true", help="只输出结论")
    args = ap.parse_args(argv)

    # 仅“提取字符集”时不需要字体；做覆盖检查才需要
    if not args.font and not args.ranges and args.emit_symbols is None:
        ap.error("需要 --font 或 --range 指定字体覆盖范围（或用 --emit-symbols 仅提取字符集）")

    # 1. 字体覆盖集合
    covered = set()
    font_infos = []
    if args.ranges:
        covered |= parse_ranges(args.ranges)
        font_infos.append({"path": "<--range>", "cmaps": "-", "codepoints": len(covered)})
    for fp in args.font:
        if not os.path.isfile(fp):
            print(f"[错误] 字体文件不存在：{fp}", file=sys.stderr)
            return 2
        info = parse_lvgl_font(fp)
        if info["note"]:
            print(f"[错误] {fp}：{info['note']}", file=sys.stderr)
            return 2
        covered |= info["codepoints"]
        font_infos.append({"path": fp, "cmaps": info["cmaps"], "codepoints": len(info["codepoints"])})

    # 2. 源码用字
    exts = set(e.strip().lower() for e in args.ext.split(",")) if args.ext else C_EXTS
    used = {}          # char -> [(file, line), ...]
    files = collect_sources(args.src, exts)

    def add_char(ch, where):
        if not ch or ch == "\0":
            return
        if args.skip_ascii and ord(ch) < 128:
            return
        used.setdefault(ch, []).append(where)

    for fp in files:
        for lineno, text in extract_string_chars(fp):
            for ch in text:
                add_char(ch, (fp, lineno))

    for fp in args.strings_file:
        with open(fp, "r", encoding="utf-8", errors="replace") as f:
            for lineno, text in enumerate(f, 1):
                for ch in text.rstrip("\n"):
                    add_char(ch, (fp, lineno))

    for ch in (args.chars or ""):
        add_char(ch, ("<--chars>", 0))

    if not used and not (args.emit_symbols is not None):
        print("[警告] 未从源码提取到任何字符（--src 是否为空？）", file=sys.stderr)

    # 3. 生成字符集输出（lv_font_conv --symbols）
    if args.emit_symbols is not None:
        symbols = "".join(sorted(used))
        if args.emit_symbols == "-":
            sys.stdout.write(symbols + "\n")
        else:
            with open(args.emit_symbols, "w", encoding="utf-8") as f:
                f.write(symbols + "\n")
            print(f"[OK] 已写入字符集（{len(used)} 字符）：{args.emit_symbols}")

    # 仅提取字符集模式：不做覆盖检查
    if not args.font and not args.ranges:
        return 0

    # 4. 覆盖对比
    missing = {ch: locs for ch, locs in used.items() if ord(ch) not in covered}
    cn_used = sum(1 for ch in used if 0x4E00 <= ord(ch) <= 0x9FFF)

    if not args.quiet:
        print("—— 字体覆盖检查 ——")
        for fi in font_infos:
            print(f"  字体：{fi['path']}  cmap={fi['cmaps']}  覆盖码点={fi['codepoints']}")
        print(f"  源码：{len(files)} 个文件，用到 {len(used)} 个不同字符（其中中文 {cn_used} 个）")
        if missing:
            print(f"  结果：缺 {len(missing)} 个字符 ——")
            for ch in sorted(missing, key=ord):
                locs = missing[ch]
                where = ", ".join(f"{os.path.basename(f)}:{ln}" for f, ln in locs[:3])
                more = f" 等{len(locs)}处" if len(locs) > 3 else ""
                visible = ch if ch.isprintable() and ch not in "\n\r\t" else ""
                print(f"    U+{ord(ch):04X} {visible}  ×{len(locs)}  ({where}{more})")
        else:
            print("  结果：全部字符均被字体覆盖 ✓")

    if args.report:
        payload = {
            "fonts": [{"path": fi["path"], "cmaps": fi["cmaps"], "codepoints": fi["codepoints"]}
                      for fi in font_infos],
            "scanned_files": files,
            "used_chars": len(used),
            "cjk_chars": cn_used,
            "missing": [
                {"char": ch, "codepoint": f"U+{ord(ch):04X}", "count": len(locs),
                 "locations": [f"{f}:{ln}" for f, ln in locs[:10]]}
                for ch, locs in sorted(missing.items(), key=lambda kv: ord(kv[0]))
            ],
        }
        with open(args.report, "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False, indent=2)
        print(f"[OK] 报告已写入：{args.report}")

    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
