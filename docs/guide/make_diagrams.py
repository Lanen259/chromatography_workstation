# -*- coding: utf-8 -*-
"""生成《色谱工作站·架构与开发指南》的三张架构图（PIL 绘制）。"""
import os
from PIL import Image, ImageDraw, ImageFont

OUT = r"E:/My_project/QT/chromatography_workstation/docs/guide"
os.makedirs(OUT, exist_ok=True)

F_HEI = "C:/Windows/Fonts/simhei.ttf"   # 黑体（粗）
F_YH  = "C:/Windows/Fonts/msyh.ttc"     # 雅黑（常规）
F_YHB = "C:/Windows/Fonts/msyhbd.ttc"   # 雅黑（粗）

def font(path, size):
    return ImageFont.truetype(path, size)

def box(d, xy, text, fill, outline, text_fill=(30,30,30), radius=16, font_obj=None, lines=None, line_font=None, line_gap=6, text_center=False, align='center'):
    d.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=3)
    x0, y0, x1, y1 = xy
    cx = (x0 + x1) / 2
    # 主标题
    if font_obj and text:
        bbox = d.textbbox((0, 0), text, font=font_obj)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        ty = (y0 + y1) / 2 - th / 2 - (len(lines) * (line_gap) if lines else 0)
        d.text((cx - tw / 2, ty), text, font=font_obj, fill=text_fill)
    # 副标题行（居中，垂直堆叠在标题下方）
    if lines and line_font:
        th_main = 0
        if font_obj:
            b = d.textbbox((0, 0), text, font=font_obj)
            th_main = b[3] - b[1]
        top = (y0 + y1) / 2 + (th_main / 2) + 4
        for ln in lines:
            b = d.textbbox((0, 0), ln, font=line_font)
            w, h = b[2] - b[0], b[3] - b[1]
            d.text((cx - w / 2, top), ln, font=line_font, fill=(70, 70, 70))
            top += h + line_gap
    return xy

def arrow_down(d, x, y0, y1, color=(90, 90, 90), width=3):
    d.line([(x, y0), (x, y1)], fill=color, width=width)
    # 箭头
    d.polygon([(x, y1), (x - 9, y1 - 14), (x + 9, y1 - 14)], fill=color)

def arrow_right(d, x0, y0, x1, y1, color=(90, 90, 90), width=3):
    d.line([(x0, y0), (x1, y1)], fill=color, width=width)
    d.polygon([(x1, y1), (x1 - 14, y1 - 9), (x1 - 14, y1 + 9)], fill=color)

# ============================================================ 图1 项目架构总览（思维导图式）
W, H = 1700, 1010
img = Image.new("RGB", (W, H), "white")
d = ImageDraw.Draw(img)

f_root  = font(F_YHB, 40)
f_leaf  = font(F_YHB, 32)
f_sub   = font(F_YH, 20)
f_note  = font(F_YH, 24)

# 根节点
root_xy = (60, 470, 480, 570)
box(d, root_xy, "色谱工作站 CDS 工程", (232, 240, 254), (41, 98, 173), (255,255,255), 20, f_root,
    lines=["一个 git 仓库 · 一个根 CMakeLists", "7 个独立模块（=7 条独立开发线）"], line_font=f_sub, line_gap=4)

# 脊柱线
spine_x = 640
d.line([(480, 520), (spine_x, 520)], fill=(41, 98, 173), width=4)

leaves = [
    ("core_model",   "领域模型 · 地基", "零依赖 · 纯 QtCore", (230, 255, 230), (34, 139, 34)),
    ("core_processing","处理引擎",      "滤波/峰检测/积分/定量", (238, 232, 255), (94, 66, 173)),
    ("acq",          "采集 · 实时",     "HAL 抽象 · 环形缓冲", (255, 243, 224), (190, 105, 0)),
    ("io",           "导入导出",        "CSV/CDF 等格式转换器", (255, 240, 245), (196, 63, 98)),
    ("report",       "报告",            "CSV/PDF/Excel 生成", (255, 244, 255), (150, 80, 150)),
    ("ui",           "界面层",          "曲线/峰表/方法编辑", (222, 240, 255), (30, 100, 180)),
    ("app",          "装配 · 可运行",   "唯一能出 exe 的模块", (255, 245, 225), (160, 100, 0)),
]
# 纵向排布
ys = [70, 185, 300, 415, 530, 645, 760]
# 脊柱
d.line([(spine_x, 70), (spine_x, 800)], fill=(41, 98, 173), width=4)
for (name, duty, dep, fill, edge), y in zip(leaves, ys):
    box(d, (700, y, 1320, y + 92), name, fill, edge, (30,30,30), 16, f_leaf,
        lines=[duty, dep], line_font=f_sub, line_gap=3)
    d.line([(spine_x, y + 46), (700, y + 46)], fill=(41, 98, 173), width=3)

# 右侧说明
box(d, (1420, 120, 1660, 720), "每个模块", (250, 250, 250), (180, 180, 180), (60,60,60), 14,
    lines=["= 独立 CMake 库", "+ 独立测试", "", "可单独编译", "可单独 ctest", "", "互不阻塞", "最后全部合并", "到主分支成整机"],
    line_font=f_sub, line_gap=8)

# 底部说明
box(d, (700, 850, 1660, 950), "并行开发方式", (250, 250, 250), (180, 180, 180), (60,60,60), 14,
    lines=["每模块一个 git 分支（worktree 独立目录）→ 各自验证 → 合并主分支",
           "合并顺序：core_model → core_processing → acq → io → report → ui → app"],
    line_font=f_note, line_gap=8)

img.save(os.path.join(OUT, "fig1_project_overview.png"))
print("fig1 saved")

# ============================================================ 图2 软件架构分层
W, H = 1700, 1010
img = Image.new("RGB", (W, H), "white")
d = ImageDraw.Draw(img)

f_band  = font(F_YHB, 34)
f_sub   = font(F_YH, 20)
f_warn  = font(F_YH, 22)

# 左侧依赖箭头（贯穿）
arrow_down(d, 80, 200, 800, color=(41, 98, 173), width=5)

bands = [
    ((120, 60, 1560, 180), "app · 装配层", "把各模块链接成唯一可运行 exe", (255, 245, 225), (160, 100, 0)),
    ((120, 230, 1560, 350), "ui · 界面层（Qt Widgets）", "曲线视图 / 峰表 / 方法编辑 / 选区桥接 —— 唯一允许 GUI 的模块", (222, 240, 255), (30, 100, 180)),
]
for xy, t1, t2, fill, edge in bands:
    box(d, xy, t1, fill, edge, (30,30,30), 16, f_band, lines=[t2], line_font=f_sub, line_gap=4)

# 三个周边模块并排
sx0, sx1 = 120, 500
for xy, t1, t2, fill, edge in [
    ((sx0, 400, sx1, 520), "acq · 采集", "HAL 接口 + 环形缓冲 + 采集线程", (255, 243, 224), (190, 105, 0)),
    ((sx0 + 400, 400, sx1 + 400, 520), "io · 导入导出", "IChromatogramImporter/Exporter 转换器", (255, 240, 245), (196, 63, 98)),
    ((sx0 + 800, 400, sx1 + 800, 520), "report · 报告", "IReporter + CSV/PDF/Excel 实现", (255, 244, 255), (150, 80, 150)),
]:
    box(d, xy, t1, fill, edge, (30,30,30), 16, f_band, lines=[t2], line_font=f_sub, line_gap=4)

box(d, (120, 570, 1560, 690), "core_processing · 处理引擎（纯 QtCore）",
    (238, 232, 255), (94, 66, 173), (30,30,30), 16, f_band,
    lines=["算法接口 + 注册表 + 管线执行器；SG 平滑 / 一阶导数峰检测 / 梯形积分 / 校准定量"],
    line_font=f_sub, line_gap=4)

box(d, (120, 740, 1560, 860), "core_model · 领域模型 · 地基（纯 QtCore）",
    (230, 255, 230), (34, 139, 34), (30,30,30), 16, f_band,
    lines=["Signal / Chromatogram / Peak / Method / Selection —— 全项目底层，零依赖"],
    line_font=f_sub, line_gap=4)

# 警告框
box(d, (120, 910, 1560, 990), "依赖铁律",
    (255, 235, 235), (196, 63, 63), (120, 20, 20), 14, font(F_YHB, 24),
    lines=["只准向下依赖；core / acq / io / report 禁止 include <QtWidgets>；acq 实时路径不经过分析管线；UI 只通过接口 + signals 读写数据"],
    line_font=f_sub, line_gap=4)

# 箭头标注
d.text((60, 260), "依赖", font=f_sub, fill=(41, 98, 173))
d.text((60, 300), "方向", font=f_sub, fill=(41, 98, 173))

img.save(os.path.join(OUT, "fig2_software_layers.png"))
print("fig2 saved")

# ============================================================ 图3 如何加入新功能
W, H = 1700, 640
img = Image.new("RGB", (W, H), "white")
d = ImageDraw.Draw(img)

f_flow  = font(F_YHB, 28)
f_slot  = font(F_YHB, 26)
f_sub   = font(F_YH, 20)

# 第一步
box(d, (40, 220, 320, 380), "想到一个新功能", (232, 240, 254), (41, 98, 173), (30,30,30), 16, f_flow,
    lines=["比如：新的峰检测算法", "新的文件格式 / 新硬件 / 新报告"], line_font=f_sub, line_gap=6)
arrow_right(d, 320, 300, 400, 300)

# 第二步：四个插槽
slots = [
    ("算法", "实现 IPeakDetector / IFilter / IIntegrator / IQuantifier", (238, 232, 255), (94, 66, 173)),
    ("文件格式", "实现 IChromatogramImporter / Exporter", (255, 240, 245), (196, 63, 98)),
    ("新设备", "实现 IDevice（HAL 驱动）", (255, 243, 224), (190, 105, 0)),
    ("报告格式", "实现 IReporter", (255, 244, 255), (150, 80, 150)),
]
x = 400
for name, desc, fill, edge in slots:
    box(d, (x, 120, x + 280, 470), "插槽：" + name, fill, edge, (30,30,30), 16, f_slot,
        lines=["", desc], line_font=f_sub, line_gap=8)
    arrow_right(d, x + 280, 300, x + 330, 300)
    x += 330
arrow_right(d, x, 300, x + 80, 300)

# 第三步：实现+注册
box(d, (x + 80, 220, x + 400, 380), "实现接口 + 注册", (250, 250, 250), (180, 180, 180), (60,60,60), 16, f_flow,
    lines=["新类写在 src/，在注册表登记 id", "不动任何已有代码（开闭原则）"], line_font=f_sub, line_gap=6)
arrow_right(d, x + 400, 300, x + 480, 300)

# 第四步：测试+合并
box(d, (x + 480, 220, x + 820, 380), "写测试 · ctest 全绿", (230, 255, 230), (34, 139, 34), (30,30,30), 16, f_flow,
    lines=["独立验证本功能没问题", "然后合并回主分支"], line_font=f_sub, line_gap=6)

img.save(os.path.join(OUT, "fig3_add_feature.png"))
print("fig3 saved")
