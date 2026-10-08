#!/usr/bin/env python3
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Render persona preview GIFs using the firmware's persona renderer.

    python3 tools/muse/persona_gifs.py [out_dir]     (default: ./persona_gifs)

Needs a C compiler and Pillow.
"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "components", "muse", "muse_persona.c")
FRAME_MS = 40


def main():
    from PIL import Image

    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "persona_gifs")
    os.makedirs(out, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "persona_anim")
        subprocess.run(
            ["cc", "-O2", "-Wall", "-I", "components/muse", "tools/muse/persona_anim.c", SRC,
             "-lm", "-o", exe],
            cwd=ROOT, check=True, capture_output=True, text=True,
        )
        frames_dir = os.path.join(tmp, "frames")
        os.makedirs(frames_dir)
        subprocess.check_call([exe, frames_dir])

        for name in sorted(os.listdir(frames_dir)):
            d = os.path.join(frames_dir, name)
            ppms = sorted(f for f in os.listdir(d) if f.endswith(".ppm"))
            imgs = [Image.open(os.path.join(d, f)) for f in ppms]
            # keep every 2nd frame to bound GIF size
            imgs = imgs[::2]
            gif = os.path.join(out, name + ".gif")
            imgs[0].save(gif, save_all=True, append_images=imgs[1:], duration=FRAME_MS * 2,
                         loop=0)
            print("wrote", gif)


if __name__ == "__main__":
    main()
