#!/usr/bin/env python3
# Korean localization tooling for the Muse Gadget SDK fork.
# Licensed under the Apache License, Version 2.0.
"""Reproduce the 12/16 px full-Hangul fonts (requires Node.js/npx)."""
import gzip
import hashlib
import subprocess
import tempfile
import urllib.request
from pathlib import Path

VERSION = "16.0.04"
SHA256 = "0e3981ab552231b5a2a870f2b61741903a4bf25c23ef5aeb05fdced1b3c7af4d"
ROOT = Path(__file__).resolve().parents[2] / "components/muse/fonts"
RANGES = ["0x1100-0x11FF", "0x3130-0x318F", "0xA960-0xA97F", "0xAC00-0xD7A3", "0xD7B0-0xD7FF"]
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    font = tmp / f"unifont-{VERSION}.otf"
    data = urllib.request.urlopen(f"https://unifoundry.com/pub/unifont/unifont-{VERSION}/font-builds/unifont-{VERSION}.otf").read()
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise ValueError("Unifont SHA-256 mismatch")
    font.write_bytes(data)
    for size, name in [(16, "muse_font_ko_16"), (12, "muse_font_ko_ui_12")]:
        out = tmp / (name + ".c")
        args = ["npx", "-y", "lv_font_conv@1.5.3", "--font", str(font), "--size", str(size),
                "--bpp", "1", "--format", "lvgl", "--lv-font-name", name, "--no-compress"]
        for r in RANGES: args += ["-r", r]
        subprocess.run(args + ["-o", str(out)], check=True)
        source = out.read_text().replace('#include "lvgl/lvgl.h"', '#include "lvgl.h"')
        source = source.replace(str(font), font.name).replace(str(out), out.name)
        (ROOT / (name + ".c.gz")).write_bytes(gzip.compress(source.encode(), mtime=0))
        print("wrote", ROOT / (name + ".c.gz"))
