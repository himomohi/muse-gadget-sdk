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

from __future__ import annotations

import json
import sys
import threading
import types

import pytest

from musegadget import cli, config
from musegadget import pair as pair_module
from musegadget.pair import PairOutcome, PairResult, SetupWindow, pair


class FakeBleServer:
    def __init__(self, name, *, on_write, on_disconnect):
        self.name = name
        self.stopped = threading.Event()

    def run(self):
        self.stopped.wait(5)

    def stop(self):
        self.stopped.set()


class CompletingController:
    def __init__(self, *, transport, on_complete, **kwargs):
        self.on_complete = on_complete
        self.transport = transport

    def start(self):
        self.on_complete()

    def stop(self):
        pass


class SilentController(CompletingController):
    def start(self):
        self.transport.stop()


@pytest.fixture
def state(tmp_path, monkeypatch):
    monkeypatch.setenv(config.STATE_DIR_ENV, str(tmp_path))
    monkeypatch.delenv(config.SDK_TOKEN_ENV, raising=False)
    monkeypatch.setitem(sys.modules, "musegadget.ble_server",
                        types.SimpleNamespace(BleServer=FakeBleServer))
    monkeypatch.setattr(pair_module, "BLE_SHUTDOWN_DELAY_S", 0)
    (tmp_path / config.IDENTITY_FILE).write_text(json.dumps({"mac": "02:00:00:ab:cd:ef"}))
    return tmp_path


def test_a_paired_device_is_left_alone_unless_forced(state, monkeypatch, capsys):
    (state / config.PAIRING_FILE).write_text('{"access_token": "kept"}')
    monkeypatch.setattr(pair_module, "SetupController", CompletingController)

    assert cli.main(["pair", "--timeout", "60"]) == 1
    assert capsys.readouterr().err == "이미 연결되어 있습니다. 먼저 `musegadget unpair`를 실행하거나 --force를 사용하세요.\n"
    assert cli.main(["pair", "--force", "--timeout", "60"]) == 0


def test_an_invalid_sdk_token_stops_pairing_before_setup_opens(state, monkeypatch, capsys):
    monkeypatch.setenv(config.SDK_TOKEN_ENV, "not-a-token")
    monkeypatch.setattr(pair_module, "SetupController", CompletingController)

    assert cli.main(["pair", "--timeout", "60"]) == 1
    output = capsys.readouterr()
    assert output.err == "연결할 수 없습니다: SDK 토큰이 올바르지 않습니다. gadgets.muse.ai에서 다시 복사하세요.\n"
    assert output.out == ""


def test_completed_setup_reports_paired(state, monkeypatch, capsys):
    monkeypatch.setattr(pair_module, "SetupController", CompletingController)

    assert cli.main(["pair", "--timeout", "120"]) == 0
    output = capsys.readouterr()
    assert output.out == ("2분 동안 연결을 기다립니다. Muse 앱에서 기기를 추가하고\n"
                          "MuseGadgetABCDEF을 선택하세요.\n연결했습니다.\n")
    assert output.err == ("SDK 토큰이 없습니다. gadgets.muse.ai에서 발급받은 뒤 "
                          "`bash install.sh --sdk-token mgst_…`를 실행하세요. 토큰 없이는 연결할 수 없습니다.\n")


def test_a_setup_window_that_closes_unpaired_says_so(state, monkeypatch, capsys):
    monkeypatch.setattr(pair_module, "SetupController", SilentController)

    assert cli.main(["pair", "--timeout", "60"]) == 1
    assert capsys.readouterr().err.endswith("연결하지 않은 채 대기 시간이 종료되었습니다.\n")


def test_pair_prints_nothing_and_hands_the_setup_window_to_the_caller(state, monkeypatch, capsys):
    monkeypatch.setattr(pair_module, "SetupController", CompletingController)
    windows = []

    assert pair(on_open=windows.append, timeout_s=120) == PairResult(PairOutcome.PAIRED)
    assert windows == [SetupWindow("MuseGadgetABCDEF", 120, has_sdk_token=False)]
    (state / config.PAIRING_FILE).write_text('{"access_token": "kept"}')
    assert pair(on_open=windows.append, timeout_s=120) == PairResult(PairOutcome.ALREADY_PAIRED)
    monkeypatch.setenv(config.SDK_TOKEN_ENV, "not-a-token")
    assert pair(on_open=windows.append, force=True, timeout_s=120) == PairResult(
        PairOutcome.INVALID_SDK_TOKEN,
        sdk_token_problem="the SDK token is not valid; copy it again from gadgets.muse.ai")
    assert len(windows) == 1
    assert capsys.readouterr() == ("", "")
