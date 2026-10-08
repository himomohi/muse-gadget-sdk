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

"""The tamagotchi pet engine core (muse_pet_core.c): stats, growth, actions,
mood, sickness and NVS blob round-trip. Pure C99, no IDF needed."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PetCoreTest(unittest.TestCase):
    def test_core(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / "pet_core_test"
            subprocess.run(
                ["cc", "-O2", "-Wall", "-Werror", "-I", "components/muse",
                 "tests/pet_core_test.c", "components/muse/muse_pet_core.c",
                 "-o", str(exe)],
                cwd=ROOT, check=True, capture_output=True, text=True,
            )
            out = subprocess.run([str(exe)], capture_output=True, text=True, check=False)
            self.assertEqual(out.returncode, 0, msg=out.stdout + out.stderr)
            self.assertIn("all tests passed", out.stdout)


if __name__ == "__main__":
    unittest.main()
