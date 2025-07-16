#!/usr/bin/env python3
"""
fix_shaders.py  –  final version
• remove UTF-8 BOM
• convert CR/LF → LF
• delete trailing spaces/Tabs
• force first line to exactly "#version 320 es"
• save UTF-8 (no BOM)
"""

import os, re, sys, argparse

BOM = b"\xef\xbb\xbf"
TRAIL = re.compile(r"[ \t]+$")

def clean_file(p: str):
    raw = open(p, "rb").read()
    if raw.startswith(BOM):
        raw = raw[len(BOM):]

    try:
        txt = raw.decode("utf-8")
    except UnicodeDecodeError:
        print(f"[skip] {p} – not UTF-8")
        return

    txt = txt.replace("\r\n", "\n").replace("\r", "\n")
    lines = txt.split("\n")

    # 1) 处理第一行
    if lines and lines[0].lstrip().startswith("#version"):
        lines[0] = "#version 320 es"

    # 2) 其他行去尾随空格 / Tab
    lines = [TRAIL.sub("", ln) for ln in lines]

    cleaned = "\n".join(lines).rstrip("\n") + "\n"
    open(p, "wb").write(cleaned.encode("utf-8"))
    print(f"✓ {p}")

def walk(root: str):
    n = 0
    for d, _, fs in os.walk(root):
        for f in fs:
            if f.lower().endswith(".comp"):
                clean_file(os.path.join(d, f)); n += 1
    print(f"\n{n} file(s) processed.")

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("-d", "--dir", default=os.path.join(os.path.dirname(__file__), "shaders"),
                    help="directory containing .comp files")
    args = ap.parse_args()
    if not os.path.isdir(args.dir):
        sys.exit(f"Directory not found: {args.dir}")
    walk(args.dir)