#!/usr/bin/env python3
"""
DeskPet Qt - 生成示例皮肤

读取 assets/ 下的默认 PNG 精灵，为每个颜色预设生成一套着色皮肤，
输出到 assets/skins/<name>/。GIF 直接复制（保持动画可用）。

用法:
    python generate_skins.py
"""
import os
import shutil
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "assets")

# 颜色预设: 目录名 -> RGBA 着色
COLORS = {
    "pink": (255, 90, 160, 255),
    "blue": (90, 150, 255, 255),
    "gold": (255, 180, 60, 255),
}

PNG_SPRITES = [
    "sprite_idle.png",
    "sprite_blink.png",
    "sprite_walk1.png",
    "sprite_walk2.png",
    "sprite_sit.png",
]

GIF_SPRITES = ["stand.gif", "creep.gif"]


def tint(img: Image.Image, color, strength=0.55) -> Image.Image:
    img = img.convert("RGBA")
    alpha = img.split()[3]
    solid = Image.new("RGBA", img.size, color)
    silhouette = Image.composite(solid, Image.new("RGBA", img.size, (0, 0, 0, 0)), alpha)
    return Image.blend(img, silhouette, strength)


def main():
    for name, color in COLORS.items():
        out = os.path.join(ASSETS, "skins", name)
        os.makedirs(out, exist_ok=True)

        for sprite in PNG_SPRITES:
            src = os.path.join(ASSETS, sprite)
            if os.path.exists(src):
                tint(Image.open(src), color).save(os.path.join(out, sprite))
                print(f"  {name}/{sprite}")

        for gif in GIF_SPRITES:
            src = os.path.join(ASSETS, gif)
            if os.path.exists(src):
                shutil.copyfile(src, os.path.join(out, gif))
                print(f"  {name}/{gif} (copied)")

        # 触发特效目录
        trig_src = os.path.join(ASSETS, "triggers")
        trig_out = os.path.join(out, "triggers")
        if os.path.isdir(trig_src):
            os.makedirs(trig_out, exist_ok=True)
            for f in sorted(os.listdir(trig_src)):
                p = os.path.join(trig_src, f)
                if f.lower().endswith(".png"):
                    tint(Image.open(p), color).save(os.path.join(trig_out, f))
                    print(f"  {name}/triggers/{f}")
                elif f.lower().endswith(".gif"):
                    shutil.copyfile(p, os.path.join(trig_out, f))

    print("Done. 皮肤目录:", [n for n in COLORS])


if __name__ == "__main__":
    main()
