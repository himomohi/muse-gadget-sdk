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

"""``musegadget`` command line."""

from __future__ import annotations

import argparse
import logging
import os
import sys

from musegadget import __version__, config, identity
from musegadget.pair import SETUP_WINDOW_S, PairOutcome, SetupWindow, pair

log = logging.getLogger("musegadget")


class KoreanArgumentParser(argparse.ArgumentParser):
    def __init__(self, *args, **kwargs):
        kwargs["add_help"] = False
        super().__init__(*args, **kwargs)
        self._positionals.title = "명령 및 인수"
        self._optionals.title = "옵션"
        self.add_argument("-h", "--help", action="help", help="도움말을 표시하고 종료")

    def format_usage(self):
        return super().format_usage().replace("usage: ", "사용법: ", 1)

    def format_help(self):
        return super().format_help().replace("usage: ", "사용법: ", 1)


def _ko_problem(message: str) -> str:
    return {
        "the SDK token is not valid; copy it again from gadgets.muse.ai":
            "SDK 토큰이 올바르지 않습니다. gadgets.muse.ai에서 다시 복사하세요",
    }.get(message, message)


def _setup_open(window: SetupWindow) -> None:
    if not window.has_sdk_token:
        print("SDK 토큰이 없습니다. gadgets.muse.ai에서 발급받은 뒤 "
              "`bash install.sh --sdk-token mgst_…`를 실행하세요. 토큰 없이는 연결할 수 없습니다.",
              file=sys.stderr)
    print(f"{window.timeout_s // 60}분 동안 연결을 기다립니다. Muse 앱에서 기기를 추가하고")
    print(f"{window.ble_name}을 선택하세요.")


def cmd_pair(args: argparse.Namespace) -> int:
    result = pair(on_open=_setup_open, force=args.force, timeout_s=args.timeout)
    if result.outcome is PairOutcome.PAIRED:
        print("연결했습니다.")
        return 0
    print({
        PairOutcome.ALREADY_PAIRED: "이미 연결되어 있습니다. 먼저 `musegadget unpair`를 실행하거나 --force를 사용하세요.",
        PairOutcome.INVALID_SDK_TOKEN: f"연결할 수 없습니다: {_ko_problem(result.sdk_token_problem)}.",
        PairOutcome.WINDOW_CLOSED: "연결하지 않은 채 대기 시간이 종료되었습니다.",
    }[result.outcome], file=sys.stderr)
    return 1


def cmd_run(args: argparse.Namespace) -> int:
    from musegadget.executor import Account, Executor
    from musegadget.service import run_service

    if os.geteuid() == 0:
        if not args.run_as:
            print("--run-as ACCOUNT로 명령을 실행할 계정을 지정하세요.",
                  file=sys.stderr)
            return 1
        try:
            account = Account.lookup(args.run_as)
        except KeyError:
            print(f"계정 {args.run_as!r}이 없습니다. 계정을 만들거나 --run-as로 다른 계정을 지정하세요.",
                  file=sys.stderr)
            return 1
    else:
        account = Account.current()
    try:
        sdk_token = config.sdk_token()
    except ValueError as exc:
        log.warning("running without an SDK token: %s", exc)
        sdk_token = None
    log.info("musegadget %s: commands run as %s", __version__, account.name)
    run_service(identity.load_or_create(), Executor(account), sdk_token)
    return 0


def cmd_send_user_msg(args: argparse.Namespace) -> int:
    import json
    import socket

    message = sys.stdin.read() if args.message == ["-"] else " ".join(args.message)
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.settimeout(90)
            sock.connect(str(config.socket_path()))
            request = {"message": message}
            if args.session_id:
                request["session_id"] = args.session_id
            sock.sendall(json.dumps(request).encode() + b"\n")
            reply = json.loads(sock.makefile("rb").readline())
    except (OSError, ValueError) as exc:
        print(f"musegadget 서비스에 연결할 수 없습니다: {exc}", file=sys.stderr)
        return 1
    if not reply.get("ok"):
        print(f"전송 실패: {reply.get('error') or reply}", file=sys.stderr)
        return 1
    print("Muse에 전송했습니다.")
    return 0


def cmd_info(args: argparse.Namespace) -> int:
    ident = identity.load_or_create()
    paired = config.load_json(config.PAIRING_FILE) is not None
    print(f"버전:      {__version__}")
    print(f"노드 ID:   {ident.node_id}")
    print(f"BLE 이름:  {ident.ble_name}")
    print(f"연결 상태: {'연결됨' if paired else '미연결'}")
    return 0


def cmd_unpair(args: argparse.Namespace) -> int:
    config.delete_json(config.PAIRING_FILE)
    print("연결 정보를 삭제했습니다. 기기 ID는 유지합니다.")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = KoreanArgumentParser(prog="musegadget", description="Muse 기기 연결과 관리")
    parser.add_argument("-v", "--verbose", action="store_true", help="상세 실행 기록 표시")
    sub = parser.add_subparsers(dest="command", required=True)

    pair_parser = sub.add_parser("pair", help="Muse 앱에 연결할 블루투스 설정 열기")
    pair_parser.add_argument("--timeout", type=int, default=SETUP_WINDOW_S,
                             help="연결 대기 시간(초, 기본값: %(default)s)")
    pair_parser.add_argument("--force", action="store_true", help="이미 연결되어 있어도 다시 연결하기")
    pair_parser.set_defaults(func=cmd_pair)

    run = sub.add_parser("run", help="Muse 연결을 유지하고 명령 실행하기")
    run.add_argument(
        "--run-as",
        default=os.environ.get("MUSEGADGET_RUN_AS") or os.environ.get("SUDO_USER"),
        help="명령을 실행할 계정(기본값: sudo를 실행한 계정)",
    )
    run.set_defaults(func=cmd_run)

    send = sub.add_parser("send-user-msg", help="이 기기에서 Muse로 메시지 보내기")
    send.add_argument("message", nargs="+", help="메시지 내용(표준 입력으로 읽으려면 -)")
    send.add_argument("--session-id",
                     help="메인 대화 대신 지정한 별도 대화로 보내기(새 ID면 대화 생성)")
    send.set_defaults(func=cmd_send_user_msg)

    sub.add_parser("info", help="기기 정보 보기").set_defaults(func=cmd_info)
    sub.add_parser("unpair", help="저장된 연결 정보 삭제").set_defaults(func=cmd_unpair)

    parser._positionals.title = "명령"
    parser._optionals.title = "옵션"
    args = parser.parse_args(argv)
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    try:
        return args.func(args)
    except PermissionError as exc:
        # The state directory is root-only on an installed device.
        print(f"{exc.filename or config.state_dir()}에 접근할 수 없습니다. sudo로 실행하세요.",
              file=sys.stderr)
        return 1
