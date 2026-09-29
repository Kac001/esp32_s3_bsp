# ui-font.md — 界面设计与中文字体覆盖检查

## 界面设计原则（重新设计，不照搬 demo）

- 320x240 横屏，触摸操作；demo（14-handheld 的六宫格菜单）只可参考交互；
- 布局/配色/文案按本次需求重新设计；建议先在纸上/文档里定好页面结构再写 LVGL 代码；
- 中文界面的硬性要求：**编译前必须做字体覆盖检查**（见下），否则界面上会出现
  “豆腐块”（缺字方框）；
- 文案统一 UTF-8 源文件；日期/数字格式注意中英混排间距。

## 为什么会有“豆腐块”

LVGL 字体是**按用字子集编译**进固件的（全字库几百 KB~几 MB，320x240 屏 +
16MB Flash 也应省着用）。UI 文案里任何没编进字体的字符都会显示为方框。
因此必须“源码用字 ⊆ 字体覆盖”。

## 三步法（闭环）

```bash
# ① 从源码提取界面用字（去重字符集）
#    ★ --src 要覆盖所有提供 UI 字符串的目录：应用源码 + bsp/ 等组件
#      （漏扫的字符串会变成“豆腐块”，如板名里的数字）
#    ★ 强烈建议 --extra-ascii 兜底数字/英文，避免动态拼接文本缺字
python tools/check_font_coverage.py --src <应用源码目录> --src bsp/include \
     --emit-symbols symbols.txt

# ② 生成“刚好覆盖”的 LVGL 字体（也可用 lv_font_conv 自备字体）
python tools/gen_lvgl_font.py --chars-file symbols.txt --extra-ascii --size 20 \
     --name font_ui_cn_20 -o <应用目录>/font_ui_cn_20.c

# ③ 校验：生成的字体确实覆盖全部用字（必须 exit 0）
python tools/check_font_coverage.py --font <应用目录>/font_ui_cn_20.c \
     --src <应用源码目录> --src bsp/include
```

输出形如：

```
—— 字体覆盖检查 ——
  字体：app/font_ui_cn_20.c  cmap=35  覆盖码点=129
  源码：2 个文件，用到 67 个不同字符（其中中文 33 个）
  结果：全部字符均被字体覆盖 ✓
```

**验收标准**：最后一条命令输出“全部字符均被字体覆盖 ✓”（exit code 0）。
CI 或交付检查直接跑这条命令即可。

## 工具说明

### tools/check_font_coverage.py

- `--font <c文件>`：解析 lv_font_conv / gen_lvgl_font 生成的 LVGL 字体，
  还原覆盖码点集；可多次；
- `--range '0x4E00-0x9FFF,32-126'`：不带字体时按区间做规划性检查；
- `--src <路径>`：扫描 C/C++ 源码字符串字面量（自动跳过注释、解码转义）；
- `--emit-symbols [文件]`：输出去重字符集（喂给 lv_font_conv `--symbols`）；
- `--report report.json`：输出机器可读报告；
- `--skip-ascii`：只检查中文等非 ASCII 字符；
- 退出码：0 全覆盖 / 1 有缺失 / 2 参数错误。

### tools/gen_lvgl_font.py

- 用系统 TTF/TTC 渲染字形（默认自动找微软雅黑），输出标准 LVGL 8 字体
  （bpp=4 抗锯齿、cmap 分段、含 notdef），与 lv_font_conv 产物 ABI 一致；
- `--size` 字号；`--name` 字体符号名（用 `LV_FONT_DECLARE(名称)` 引用）；
- `--extra-ascii` 附带常用 ASCII，方便数字/英文混排；
- 字形宽度自动补齐偶数，规避 LVGL 位流对齐问题。

### 用 lv_font_conv 也可

```bash
lv_font_conv --bpp 4 --size 20 --no-compress \
    --font AlibabaPuHuiTi-3-45-Light.ttf --symbols @symbols.txt \
    --format lvgl -o font_ui_cn_20.c
```

之后同样用 check_font_coverage.py 验收。

## 字体选择与版权

| 字体 | 获取 | 说明 |
|---|---|---|
| 微软雅黑 msyh.ttc | Windows 自带 | 仅限 Windows 机器生成；商用需注意授权 |
| 阿里巴巴普惠体 | 官网免费下载 | demo 使用（font_alipuhui20.c），可商用 |
| 思源黑体 / Noto Sans CJK | 开源 | SIL OFL，可商用 |
| OPPO Sans / HarmonyOS Sans | 官网免费 | 可商用，注意各自条款 |

原则：**字体内嵌进固件 = 分发字形数据**，商用项目请选允许嵌入的字体。

## 用字变化时

改了任何 UI 文案后，重新跑三步法的 ③（覆盖检查）。缺字就重跑 ①②重新生成。
建议把 ③ 加进构建脚本或 CI：

```powershell
python tools/check_font_coverage.py --font app/font_ui_cn_20.c --src app/ ; if ($LASTEXITCODE -ne 0) { exit 1 }
```

## 字号建议（320x240）

- 正文 20px（中文可读下限）；标题 24~28px；辅助说明 16~18px；
- 每种字号生成一个字体文件（`font_ui_cn_18/20/24`），按需引用；
- 只生成用得到的字号，避免 Flash 浪费。
