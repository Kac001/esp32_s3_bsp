#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_lvgl_font.py — 从 TrueType 字体生成 LVGL 8 字体子集（C 源文件）
=================================================================

用途：
    为中文界面生成“刚好覆盖用字”的 LVGL 字体，配合
    tools/check_font_coverage.py 形成闭环：
        源码用字 → 生成字体 → 校验覆盖 → 编译固件

    相比 lv_font_conv：不需要 Node 环境，直接用系统里的 .ttf/.ttc 渲染；
    生成的是标准 lv_font_fmt_txt 结构（bpp=4 抗锯齿），与
    lv_font_conv --format lvgl 的产物 ABI 一致。

依赖：
    Python 3.8+ 与 Pillow（本机自带的 bundled Python 已含）。

用法示例：
    # 1) 先用 check_font_coverage.py 提取用字
    python tools/check_font_coverage.py --src app/ --emit-symbols symbols.txt
    # 2) 生成字体（默认微软雅黑，可指定任意 TTF/TTC）
    python tools/gen_lvgl_font.py --chars-file symbols.txt \
        --font C:\\Windows\\Fonts\\msyh.ttc --size 20 --name font_ui_cn_20 \
        -o app/font_ui_cn_20.c
    # 3) 校验生成的字体确实覆盖全部用字
    python tools/check_font_coverage.py --font app/font_ui_cn_20.c --src app/

注意：
    * 字体版权随所选 TTF 而定，商用请使用可嵌入的字体（如思源黑体、
      阿里巴巴普惠体、OPPO Sans 等）；
    * bpp=4、字形宽度自动补齐到偶数，规避 LVGL 位流行对齐问题。
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PIL import Image, ImageDraw, ImageFont  # noqa: E402

from check_font_coverage import C_EXTS, collect_sources, extract_string_chars  # noqa: E402

DEFAULT_FONTS = [
    r"C:\Windows\Fonts\msyh.ttc",
    r"C:\Windows\Fonts\msyhbd.ttc",
    r"C:\Windows\Fonts\simhei.ttf",
    r"C:\Windows\Fonts\Deng.ttf",
    r"C:\Windows\Fonts\simsun.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
]


def find_default_font():
    for p in DEFAULT_FONTS:
        if os.path.isfile(p):
            return p
    return None


def render_glyph(font, ch, bpp=4):
    """渲染单个字形，返回 (bitmap_bytes, box_w, box_h, ofs_x, ofs_y, adv_w_16)"""
    levels = (1 << bpp) - 1

    probe = ImageDraw.Draw(Image.new("L", (1, 1)))
    try:
        x0, y0, x1, y1 = probe.textbbox((0, 0), ch, font=font, anchor="ls")
    except (ValueError, TypeError):
        x0 = y0 = x1 = y1 = 0

    box_w = max(0, x1 - x0)
    box_h = max(0, y1 - y0)
    adv_w = int(round(font.getlength(ch) * 16))

    if box_w == 0 or box_h == 0:
        return b"", 0, 0, 0, 0, adv_w

    # bpp=4：行宽补齐为偶数，保证每行字节对齐（LVGL 位流连续按行取模时无歧义）
    draw_w = box_w + (box_w % 2)

    img = Image.new("L", (draw_w, box_h), 0)
    draw = ImageDraw.Draw(img)
    draw.text((-x0, -y0), ch, font=font, fill=255, anchor="ls")

    # 灰度 -> bpp 级别，位流 MSB（高半字节）在前，行间连续
    pixels = list(img.getdata())
    vals = [(v * levels + 127) // 255 for v in pixels]

    out = bytearray()
    acc, nbits = 0, 0
    for v in vals:
        acc = (acc << bpp) | v
        nbits += bpp
        if nbits >= 8:
            out.append((acc >> (nbits - 8)) & 0xFF)
            nbits -= 8
            acc &= (1 << nbits) - 1
    if nbits:
        out.append((acc << (8 - nbits)) & 0xFF)

    ofs_x = x0
    ofs_y = -y1          # LVGL：字框底边相对基线的偏移
    return bytes(out), draw_w, box_h, ofs_x, ofs_y, adv_w


def build_cmap_entries(codepoints):
    """把码点集合切成连续段，生成 FORMAT0_TINY cmap（返回 [(start, length, glyph_id_start)]）"""
    entries = []
    cps = sorted(codepoints)
    i = 0
    while i < len(cps):
        start = cps[i]
        end = start
        j = i + 1
        while j < len(cps) and cps[j] == cps[j - 1] + 1:
            end = cps[j]
            j += 1
        length = end - start + 1
        entries.append((start, length, i + 1))   # glyph 0 = notdef
        i = j
    return entries


def fmt_bytes(data, indent="    "):
    lines = []
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append(indent + ", ".join("0x%02x" % b for b in chunk) + ",")
    return "\n".join(lines) if lines else indent + "/* 空 */"


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="从 TTF/TTC 生成 LVGL 8 字体子集（C 源文件，bpp=4 抗锯齿）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--font", default=None, help="TTF/TTC 字体路径（默认自动探测系统中文字体）")
    ap.add_argument("--size", type=int, default=20, help="字号（默认 20）")
    ap.add_argument("--name", default=None, help="字体符号名（默认 font_ui_cn_<size>）")
    ap.add_argument("--chars", default=None, help="字面字符集")
    ap.add_argument("--chars-file", default=None, help="字符集文件（check_font_coverage.py --emit-symbols 的输出）")
    ap.add_argument("--src", action="append", default=[], help="扫描源码目录/文件提取用字（可多次）")
    ap.add_argument("--extra-ascii", action="store_true",
                    help="附加常用 ASCII（空格~波浪号），方便数字/英文混排")
    ap.add_argument("-o", "--out", required=True, help="输出 C 文件路径")
    args = ap.parse_args(argv)

    font_path = args.font or find_default_font()
    if not font_path or not os.path.isfile(font_path):
        print("[错误] 找不到可用的 TTF 字体，请用 --font 指定", file=sys.stderr)
        return 2

    chars = set()
    for ch in (args.chars or ""):
        chars.add(ch)
    if args.chars_file:
        with open(args.chars_file, "r", encoding="utf-8") as f:
            for ch in f.read():
                if ch != "\n":
                    chars.add(ch)
    for path in collect_sources(args.src, C_EXTS):
        for _ln, text in extract_string_chars(path):
            for ch in text:
                chars.add(ch)
    if args.extra_ascii:
        chars.update(chr(c) for c in range(0x20, 0x7F))

    chars.discard("\0")
    chars = {c for c in chars if c.isprintable() or c == " "}
    if not chars:
        print("[错误] 字符集为空", file=sys.stderr)
        return 2

    font = ImageFont.truetype(font_path, args.size)
    ascent, descent = font.getmetrics()
    line_height = ascent + descent
    base_line = descent

    name = args.name or f"font_ui_cn_{args.size}"
    macro = name.upper()

    # 渲染全部字形（按码点排序；glyph 0 为 notdef）
    ordered = sorted(chars)
    glyph_bitmaps = []
    glyph_dsc = []
    offset = 0
    # notdef
    glyph_bitmaps.append(b"")
    glyph_dsc.append((offset, args.size * 8, 0, 0, 0, 0))
    for ch in ordered:
        bmp, w, h, ox, oy, adv = render_glyph(font, ch)
        glyph_dsc.append((offset, adv, w, h, ox, oy))
        glyph_bitmaps.append(bmp)
        offset += len(bmp)

    cmaps = build_cmap_entries([ord(c) for c in ordered])

    out_lines = []
    out_lines.append(f"""/*******************************************************************************
 * LVGL 8 字体子集（由 tools/gen_lvgl_font.py 生成，请勿手改）
 *   字体: {os.path.basename(font_path)}
 *   字号: {args.size}px    bpp: 4    字形数: {len(ordered)} + notdef
 *   用字: 见文末清单
 ******************************************************************************/
#include "lvgl.h"

#ifndef {macro}
#define {macro} 1
#endif

#if {macro}

/*-----------------
 *    BITMAPS
 *----------------*/""")

    out_lines.append("/*Store the image of the glyphs*/")
    out_lines.append("static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {")
    for ch, bmp in zip(ordered, glyph_bitmaps[1:]):
        try:
            label = ch if ch.isprintable() and ch != "\0" else ""
        except Exception:
            label = ""
        out_lines.append(f'    /* U+{ord(ch):04X} "{label}" */')
        out_lines.append(fmt_bytes(bmp) if bmp else "    /* 空 */")
        out_lines.append("")
    out_lines.append("};")
    out_lines.append("")

    out_lines.append("""/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/""")
    out_lines.append("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {")
    out_lines.append("    /* notdef */")
    _off, adv, w, h, ox, oy = glyph_dsc[0]
    out_lines.append(f"        {{ .bitmap_index = 0, .adv_w = {adv}, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0 }},")
    for ch, (_off, adv, w, h, ox, oy) in zip(ordered, glyph_dsc[1:]):
        out_lines.append(f'    /* U+{ord(ch):04X} */')
        out_lines.append(f"        {{ .bitmap_index = {_off}, .adv_w = {adv}, .box_w = {w}, .box_h = {h}, .ofs_x = {ox}, .ofs_y = {oy} }},")
    out_lines.append("};")
    out_lines.append("")

    out_lines.append("""/*-----------------
 *  CMAP DESCRIPTION
 *--------------------*/""")
    out_lines.append("/*Collect the unicode lists and glyph_id offsets*/")
    out_lines.append("static const lv_font_fmt_txt_cmap_t cmaps[] = {")
    for idx, (start, length, gid) in enumerate(cmaps):
        out_lines.append(f"    {{ .range_start = {start}, .range_length = {length}, .glyph_id_start = {gid},")
        out_lines.append("      .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY },")
    out_lines.append("};")
    out_lines.append("")

    out_lines.append(f"""/*--------------------*
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {{
#else
static lv_font_fmt_txt_dsc_t font_dsc = {{
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = {len(cmaps)},
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
}};

/*-----------------
 *  PUBLIC FONT
 *----------------*/

#if LVGL_VERSION_MAJOR >= 8
const lv_font_t {name} = {{
#else
lv_font_t {name} = {{
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {line_height},
    .base_line = {base_line},
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
}};

#endif /*#if {macro}*/

/* 用字清单（{len(ordered)} 字符）：
{"".join(ordered)}
*/""")

    with open(args.out, "w", encoding="utf-8") as f:
        f.write("\n".join(out_lines) + "\n")

    total = offset
    print(f"[OK] 生成 {args.out}")
    print(f"     字体 {os.path.basename(font_path)} {args.size}px，字形 {len(ordered)} 个，位图 {total} 字节，cmap {len(cmaps)} 段")
    return 0


if __name__ == "__main__":
    sys.exit(main())
