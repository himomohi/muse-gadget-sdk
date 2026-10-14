#!/usr/bin/env python3
# Korean localization tooling for the Muse Gadget SDK fork.
# Licensed under the Apache License, Version 2.0.
"""Expand a checked-in generated font, without network access at build time."""
import gzip
import sys
from pathlib import Path

source, output = map(Path, sys.argv[1:])
data = gzip.decompress(source.read_bytes())
output.parent.mkdir(parents=True, exist_ok=True)
if not output.exists() or output.read_bytes() != data:
    output.write_bytes(data)
