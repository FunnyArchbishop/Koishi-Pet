#!/usr/bin/env python3
"""
DeskPet Qt - 东方幻存神签 爬取脚本

从 thbwiki 的「东方幻存神签/御神签速览」页面解析所有角色，
再逐一下载每个角色的御神签图片到 assets/omikuji/，并生成 manifest.txt。

用法:
    python crawl_omikuji.py
"""
import os
import re
import html
import time
import urllib.request

BASE = "https://thbwiki.cc"
OVERVIEW_URL = ("https://thbwiki.cc/%E4%B8%9C%E6%96%B9%E5%B9%BB%E5%AD%98%E7%A5%9E%E7%AD%BE/"
                "%E5%BE%A1%E7%A5%9E%E7%AD%BE%E9%80%9F%E8%A7%88")
HERE = os.path.dirname(os.path.abspath(__file__))
OVERVIEW = os.path.join(HERE, "omikuji_page.html")  # 可选本地缓存
OUT_DIR = os.path.join(HERE, "assets", "omikuji")
os.makedirs(OUT_DIR, exist_ok=True)

HEADERS = {"User-Agent": "Mozilla/5.0 (DeskPet omikuji crawler; contact: local)"}


def fetch(url):
    req = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


def fetch_text(url):
    return fetch(url).decode("utf-8", "ignore")


def strip_tags(s):
    s = re.sub(r"<[^>]+>", "", s)
    return html.unescape(s).strip()


def main():
    if os.path.exists(OVERVIEW):
        overview = open(OVERVIEW, encoding="utf-8").read()
    else:
        overview = fetch_text(OVERVIEW_URL)
    m = re.search(r'<table[^>]*class="[^"]*wikitable[^"]*".*?</table>', overview, re.S)
    if not m:
        print("未找到表格")
        return
    rows = re.findall(r"<tr[^>]*>(.*?)</tr>", m.group(0), re.S)

    entries = []
    for row in rows[1:]:
        cells = re.findall(r"<td[^>]*>(.*?)</td>", row, re.S)
        if len(cells) < 6:
            continue
        character = strip_tags(cells[0])
        number = strip_tags(cells[1])
        fortune = strip_tags(cells[2])
        page_href = ""
        # 御神签页单元格 (第 6 列) 中唯一的 <a> 链接即为角色御神签子页
        mm = re.search(r'href="([^"]+)"', cells[5])
        if mm:
            page_href = html.unescape(mm.group(1))
        if not page_href:
            continue
        entries.append((number, character, fortune, page_href))

    print(f"共 {len(entries)} 个角色")

    manifest = []
    for i, (number, character, fortune, href) in enumerate(entries):
        try:
            page = fetch_text(BASE + href)
            thumb = ""
            mm = re.search(r'property="og:image"[^>]*content="([^"]+)"', page)
            if mm:
                thumb = html.unescape(mm.group(1))
            if not thumb:
                mm = re.search(r'src="([^"]*\u4e1c\u65b9\u5e7b\u5b58\u795e\u7b7e[^"]*\.(?:png|jpg|jpeg|webp))"', page)
                if mm:
                    thumb = html.unescape(mm.group(1))

            if not thumb:
                print(f"  [{number}] {character}: 无图片")
                continue

            # thumb: .../thumb/H1/H2/NAME.ext/NNNpx-NAME.ext -> full: .../H1/H2/NAME.ext
            full = thumb.replace("/thumb/", "/", 1)
            full = re.sub(r"/[0-9]+px-[^/]+$", "", full)

            ext = os.path.splitext(full.split("?")[0])[1].lower() or ".png"
            out_name = f"{number}{ext}"
            out_path = os.path.join(OUT_DIR, out_name)

            # 请求 600px 宽缩略图 (足够弹窗显示, 远小于原图)
            WIDTH = 600
            if re.search(r"/[0-9]+px-", thumb):
                thumb_large = re.sub(r"/[0-9]+px-", f"/{WIDTH}px-", thumb)
            else:
                thumb_large = thumb

            data = None
            try:
                data = fetch(thumb_large)
            except Exception:
                data = fetch(thumb)

            with open(out_path, "wb") as f:
                f.write(data)
            manifest.append(f"{out_name}|{character}|{fortune}")
            print(f"  [{i+1}/{len(entries)}] {number} {character} ({fortune}) -> {out_name} ({len(data)}B)")
        except Exception as e:
            print(f"  [{number}] {character}: ERROR {e}")
        time.sleep(0.15)

    with open(os.path.join(OUT_DIR, "manifest.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(manifest) + "\n")
    print(f"完成: {len(manifest)} 张御神签图片")


if __name__ == "__main__":
    main()
