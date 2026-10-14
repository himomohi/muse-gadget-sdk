#!/usr/bin/env python3
# Korean localization tooling for the Muse Gadget SDK fork.
# Licensed under the Apache License, Version 2.0.
"""Regenerate the C lookup table from the editable Korean translation catalog."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / "components/muse"
catalog = json.loads((ROOT / "translations/ko.json").read_text())
pattern = re.compile(r"%(?:[-+#0]*\d*(?:\.\d+)?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcsp]|%)")
for en, ko in catalog.items():
    if pattern.findall(en) != pattern.findall(ko):
        raise ValueError("Printf argument mismatch: " + en)
rows = "\n".join("    { " + json.dumps(en, ensure_ascii=False) + ", " +
                 json.dumps(ko, ensure_ascii=False) + " }," for en, ko in sorted(catalog.items()))
p = ROOT / "muse_locale.c"
s = p.read_text()
a = s.index("static const struct { const char *en, *ko; } STRINGS[] = {\n")
a = s.index("\n", a) + 1
b = s.index("\n};", a)
s = s[:a] + rows + s[b:]
p.write_text(s)
print(f"Updated {len(catalog)} Korean translations")
