#!/usr/bin/env bash
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

# Install the Linux Device SDK.
#
#   curl -fsSL <url>/install.sh | bash
#   bash install.sh [--from SOURCE] [--run-as USER] [--sdk-token TOKEN] [--yes] [--no-pair]
#   bash install.sh --uninstall [--purge]
#
# Installs system packages, a pinned uv, and musegadget into /opt/musegadget;
# sets BlueZ's GATT MTU for the Android app; installs and starts the
# musegadget service; then opens Bluetooth pairing for the Muse app.

set -euo pipefail

UV_VERSION="0.9.9"
PREFIX="/opt/musegadget"
VENV="$PREFIX/venv"
UNIT="/etc/systemd/system/musegadget.service"
STATE_DIR="/var/lib/musegadget"
BLUEZ_CONF="/etc/bluetooth/main.conf"
BLUEZ_DROPIN="/etc/systemd/system/bluetooth.service.d/zz-musegadget.conf"
DEFAULT_SOURCE="git+https://github.com/himomohi/muse-gadget-sdk@persona-pet#subdirectory=linux"
APT_PACKAGES=(bluez python3 python3-dbus python3-gi python3-cryptography curl ca-certificates)
ARCH_PACKAGES=(bluez python python-dbus python-gobject python-cryptography curl ca-certificates)

say() { printf '\033[1m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[33m경고:\033[0m %s\n' "$*" >&2; }
die() { printf '\033[31m오류:\033[0m %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<EOF
사용법: install.sh [옵션]

  --from SOURCE     로컬 linux/ 폴더, wheel 또는 pip/uv URL에서 설치합니다.
                    기본값: $DEFAULT_SOURCE
  --run-as USER     Muse 명령을 실행할 계정입니다. 기본값: 설치를 실행한 계정.
  --sdk-token TOKEN gadgets.muse.ai에서 발급받은 mgst_ SDK 토큰입니다.
                    생략하면 안전한 터미널 입력으로 요청합니다.
                    $STATE_DIR/sdk_token에 root만 읽을 수 있도록 저장합니다.
  --yes             확인 질문 없이 진행합니다.
  --no-pair         블루투스 연결을 시작하지 않고 설치합니다.
  --uninstall       프로그램을 삭제합니다. --purge가 없으면 기기 ID와 연결을 유지합니다.
  --purge           삭제할 때 기기 ID와 연결 정보도 삭제합니다.
  -h, --help        이 도움말을 표시합니다.
EOF
}

# Prompts read the terminal, not stdin, so `curl ... | bash` still works.
ask() {
    local prompt="$1" answer
    if [ "$ASSUME_YES" = 1 ]; then return 0; fi
    if [ ! -r /dev/tty ]; then die "확인할 터미널이 없습니다. --yes로 다시 실행하세요"; fi
    printf '%s [Y/n] ' "$prompt" >/dev/tty
    read -r answer </dev/tty || true
    case "$answer" in ""|y|Y|yes|YES) return 0 ;; *) return 1 ;; esac
}

as_root() { if [ "$(id -u)" -eq 0 ]; then "$@"; else sudo "$@"; fi; }

systemd_running() { [ -d /run/systemd/system ]; }

# --- Checks ------------------------------------------------------------------

check_system() {
    [ "$(uname -s)" = Linux ] || die "Linux 기기 SDK는 Linux에서 실행합니다."
    [ -r /etc/os-release ] || die "Linux 배포판을 확인할 수 없습니다(/etc/os-release 없음)."
    # shellcheck disable=SC1091
    . /etc/os-release
    local id="${ID:-}" version="${VERSION_ID:-0}" major="${VERSION_ID:-0}"
    major="${major%%.*}"
    case "$id" in
        debian|raspbian)
            DISTRO_FAMILY=debian
            [ "${major:-0}" -ge 11 ] 2>/dev/null ||
                die "$PRETTY_NAME은 너무 오래된 버전입니다. Debian 11(Raspberry Pi OS Bullseye) 이상으로 업그레이드하세요." ;;
        ubuntu)
            DISTRO_FAMILY=debian
            [ "$(printf '%s\n22.04\n' "$version" | sort -V | head -1)" = 22.04 ] ||
                die "$PRETTY_NAME은 너무 오래된 버전입니다. Ubuntu 22.04 이상으로 업그레이드하세요." ;;
        arch)
            DISTRO_FAMILY=arch ;;
        *)
            case " ${ID_LIKE:-} " in
                *" arch "*)
                    DISTRO_FAMILY=arch
                    warn "$PRETTY_NAME은 검증되지 않았지만 Arch 기반이므로 계속합니다." ;;
                *" debian "*|*" ubuntu "*)
                    DISTRO_FAMILY=debian
                    warn "$PRETTY_NAME은 검증되지 않았지만 Debian 기반이므로 계속합니다." ;;
                *) die "$PRETTY_NAME은 지원하지 않습니다. Debian/Ubuntu 또는 Arch Linux가 필요합니다." ;;
            esac ;;
    esac
    if [ "$DISTRO_FAMILY" = arch ]; then
        command -v pacman >/dev/null || die "pacman을 찾을 수 없습니다."
        ARCH="$(uname -m)"
        case "$ARCH" in x86_64|aarch64) ;; *) die "지원하지 않는 Arch 아키텍처: $ARCH" ;; esac
    else
        command -v apt-get >/dev/null || die "apt-get을 찾을 수 없습니다."
        # Many 32-bit Raspberry Pi installs run a 64-bit kernel, so ask dpkg,
        # which reports the userland architecture.
        ARCH="$(dpkg --print-architecture)"
        case "$ARCH" in armhf|arm64|amd64) ;; *) die "지원하지 않는 아키텍처: $ARCH" ;; esac
    fi
    if ! systemd_running; then
        warn "systemd가 실행 중이 아닙니다. 서비스를 설치하지만 시작하지는 않습니다."
    fi
    if [ -z "$(ls /sys/class/bluetooth 2>/dev/null)" ]; then
        warn "블루투스 어댑터를 찾지 못했습니다. 설치는 가능하지만 Muse 앱 연결에는 Bluetooth LE가 필요합니다."
    fi
}

choose_account() {
    if [ -z "$RUN_AS" ]; then
        if [ "$(id -u)" -ne 0 ]; then RUN_AS="$(id -un)"; else RUN_AS="${SUDO_USER:-}"; fi
    fi
    if [ -z "$RUN_AS" ] || [ "$RUN_AS" = root ]; then
        die "--run-as USER로 Muse 명령을 실행할 계정을 선택하세요(root 실행은 지원하지 않습니다)."
    fi
    id "$RUN_AS" >/dev/null 2>&1 || die "계정 '$RUN_AS'이 없습니다."

    local admin=""
    if as_root sudo -n -l -U "$RUN_AS" 2>/dev/null | grep -qE '\(ALL( : ALL)?\) (NOPASSWD: )?ALL'; then
        admin=" 이 계정에는 관리자(sudo) 권한이 있어 Muse가 기기의 인증 정보를 읽는 등 이 컴퓨터의 모든 작업을 할 수 있습니다."
    fi
    say "Muse는 이 컴퓨터에서 '$RUN_AS' 계정의 권한으로 명령을 실행할 수 있습니다.$admin"
    ask "계속할까요?" || die "취소했습니다. 다른 계정을 선택하려면 --run-as로 다시 실행하세요."
}

# --- Install -----------------------------------------------------------------

install_packages() {
    local missing=() pkg
    if [ "$DISTRO_FAMILY" = arch ]; then
        for pkg in "${ARCH_PACKAGES[@]}"; do
            pacman -Qq "$pkg" >/dev/null 2>&1 || missing+=("$pkg")
        done
    else
        for pkg in "${APT_PACKAGES[@]}"; do
            dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q 'install ok installed' || missing+=("$pkg")
        done
    fi
    case "$SOURCE" in git+*) command -v git >/dev/null || missing+=(git) ;; esac
    if [ "${#missing[@]}" -eq 0 ]; then return; fi
    say "시스템 패키지 설치: ${missing[*]}"
    if [ "$DISTRO_FAMILY" = arch ]; then
        as_root pacman -S --needed --noconfirm "${missing[@]}"
    else
        as_root env DEBIAN_FRONTEND=noninteractive apt-get update -qq
        as_root env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends "${missing[@]}"
    fi
}

install_uv() {
    if [ -x "$PREFIX/bin/uv" ] && [ "$("$PREFIX/bin/uv" --version | awk '{print $2}')" = "$UV_VERSION" ]; then
        return
    fi
    say "uv $UV_VERSION 설치 중"
    as_root mkdir -p "$PREFIX/bin"
    curl -fsSL "https://astral.sh/uv/$UV_VERSION/install.sh" |
        as_root env UV_INSTALL_DIR="$PREFIX/bin" UV_NO_MODIFY_PATH=1 sh -s -- --quiet
}

enable_bluez() {
    if [ "$DISTRO_FAMILY" = arch ] && systemd_running; then
        say "BlueZ 서비스 활성화 중"
        as_root systemctl enable --now bluetooth.service
    fi
}

install_musegadget() {
    local uv="$PREFIX/bin/uv"
    # The system interpreter is required: dbus and gi come from apt and are
    # built for it.
    if [ ! -x "$VENV/bin/python" ]; then
        say "가상 환경 생성: $VENV"
        as_root "$uv" venv --quiet --python /usr/bin/python3 --system-site-packages "$VENV"
    fi
    say "musegadget 설치: $SOURCE"
    as_root "$uv" pip install --quiet --python "$VENV/bin/python" --no-deps --reinstall "$SOURCE"
    local data
    data="$("$VENV/bin/python" -c 'import musegadget, os; print(os.path.join(os.path.dirname(musegadget.__file__), "data"))')"
    as_root "$uv" pip install --quiet --python "$VENV/bin/python" --require-hashes \
        -r "$data/requirements.lock"
    as_root ln -sf "$VENV/bin/musegadget" /usr/local/bin/musegadget
    DATA_DIR="$data"
}

configure_bluez() {
    # The Muse Android app writes (MTU - 3)-byte packets and fails above 512,
    # so keep the negotiated MTU at 256, as the ESP32 firmware does.
    local result
    result="$(as_root python3 - "$BLUEZ_CONF" <<'EOF'
import re, shutil, sys
path = sys.argv[1]
try:
    text = open(path).read()
except FileNotFoundError:
    text = ""
active = re.search(r"^\s*ExchangeMTU\s*=\s*(\d+)\s*$", text, re.M)
if active and int(active.group(1)) <= 256:
    print("unchanged"); sys.exit()
line = "ExchangeMTU = 256"
if active:
    text = text[:active.start()] + line + text[active.end():]
elif re.search(r"^\s*#\s*ExchangeMTU\s*=.*$", text, re.M):
    text = re.sub(r"^\s*#\s*ExchangeMTU\s*=.*$", line, text, count=1, flags=re.M)
elif re.search(r"^\[GATT\]\s*$", text, re.M):
    text = re.sub(r"^\[GATT\]\s*$", "[GATT]\n" + line, text, count=1, flags=re.M)
else:
    text = text.rstrip("\n") + ("\n\n" if text else "") + "[GATT]\n" + line + "\n"
if text != "":
    try:
        shutil.copy2(path, path + ".pre-musegadget")
    except FileNotFoundError:
        pass
open(path, "w").write(text)
print("changed")
EOF
)"
    if [ "$result" = changed ]; then
        say "BlueZ ExchangeMTU를 256으로 설정했습니다(백업: $BLUEZ_CONF.pre-musegadget)"
        if systemd_running; then as_root systemctl restart bluetooth; fi
    fi
    disable_bluez_battery
}

# BlueZ's battery plugin reads a connecting phone's battery level. iPhones
# answer that only over a bonded link, so BlueZ then asks the phone to bond,
# which this device refuses, and iOS pairing can drop. Gadgets never bond.
disable_bluez_battery() {
    systemd_running || return 0
    local unit_text result
    unit_text="$(systemctl cat bluetooth.service 2>/dev/null || true)"
    result="$(as_root python3 - "$BLUEZ_DROPIN" "$unit_text" <<'EOF'
import os, re, sys
path, unit_text = sys.argv[1], sys.argv[2]
# The last non-empty ExecStart= across the unit and its drop-ins is the one
# systemd runs. Leave anything quoted alone rather than risk rewriting it.
lines = re.findall(r"^ExecStart=(.*)$", unit_text, re.M)
command = next((l.strip() for l in reversed(lines) if l.strip()), "")
if not command or any(c in command for c in "\"'\\$%;"):
    print("skipped"); sys.exit()
args = command.split()
for i, arg in enumerate(args):
    inline = arg.startswith("--noplugin=")
    if inline or (arg in ("-P", "--noplugin") and i + 1 < len(args)):
        names = arg.split("=", 1)[1] if inline else args[i + 1]
        if "battery" in names.split(","):
            print("unchanged"); sys.exit()
        names = names + ",battery" if names else "battery"
        if inline:
            args[i] = "--noplugin=" + names
        else:
            args[i + 1] = names
        break
else:
    args.append("--noplugin=battery")
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "w") as f:
    f.write("# Written by the musegadget installer: never ask phones to bond.\n"
            "[Service]\nExecStart=\nExecStart=" + " ".join(args) + "\n")
print("changed")
EOF
)"
    case "$result" in
        changed)
            say "휴대폰에 기기 등록을 요청하지 않도록 BlueZ 배터리 플러그인을 껐습니다($BLUEZ_DROPIN)"
            as_root systemctl daemon-reload
            as_root systemctl restart bluetooth
            ;;
        skipped) say "bluetooth.service 실행 명령을 읽지 못해 BlueZ 플러그인을 그대로 유지합니다." ;;
    esac
}

save_sdk_token() {
    if [ -z "$SDK_TOKEN" ]; then
        if ! as_root test -s "$STATE_DIR/sdk_token"; then
            say "SDK 토큰이 없습니다. gadgets.muse.ai에서 발급받고 --sdk-token으로 다시 실행하세요. 토큰 없이는 연결할 수 없습니다."
        fi
        return 0
    fi
    say "SDK 토큰 저장 중"
    as_root install -d -m 0700 "$STATE_DIR"
    printf '%s\n' "$SDK_TOKEN" | as_root install -m 0600 /dev/stdin "$STATE_DIR/sdk_token"
}

prompt_sdk_token() {
    if [ -n "$SDK_TOKEN" ] || [ "$NO_PAIR" = 1 ] || [ "$ASSUME_YES" = 1 ]; then
        return 0
    fi
    [ -r /dev/tty ] || return 0
    printf 'Muse SDK 토큰(mgst_...; 입력 숨김, 유지/건너뛰기는 Enter): ' >/dev/tty
    IFS= read -r -s SDK_TOKEN </dev/tty || SDK_TOKEN=""
    printf '\n' >/dev/tty
    if [ -n "$SDK_TOKEN" ] && ! [[ "$SDK_TOKEN" =~ ^mgst_[A-Za-z0-9_-]{42}[AEIMQUYcgkosw048]$ ]]; then
        die "SDK 토큰이 올바르지 않습니다. gadgets.muse.ai에서 다시 복사하세요."
    fi
}

install_service() {
    say "musegadget 서비스 설치 중"
    sed "s/@RUN_AS@/$RUN_AS/" "$DATA_DIR/musegadget.service" | as_root tee "$UNIT" >/dev/null
    if systemd_running; then
        as_root systemctl daemon-reload
        as_root systemctl enable --now musegadget.service >/dev/null 2>&1
        as_root systemctl restart musegadget.service
    fi
}

pair() {
    if as_root test -s "$STATE_DIR/pairing.json"; then
        say "이미 연결되어 있습니다. 서비스가 Muse에 다시 연결합니다."
        return
    fi
    if [ "$NO_PAIR" = 1 ]; then
        say "연결을 건너뜁니다. 준비되면 'sudo musegadget pair'를 실행하세요."
        return
    fi
    cat <<EOF

Pair with your Muse:
  1. In the Muse app, turn on Settings > Devices > Developer mode.
  2. Add a device and choose the device named below.
  3. When asked for Wi-Fi, pick the network shown; no password is needed.

EOF
    as_root /usr/local/bin/musegadget pair || warn "연결되지 않았습니다. 'sudo musegadget pair'로 다시 시도하세요."
}

summary() {
    echo
    as_root /usr/local/bin/musegadget info
    cat <<EOF

서비스:   sudo systemctl status musegadget
기록:     sudo journalctl -u musegadget -f
삭제:     bash install.sh --uninstall
EOF
}

# --- Uninstall ---------------------------------------------------------------

uninstall() {
    say "musegadget 삭제 중"
    if systemd_running && [ -f "$UNIT" ]; then
        as_root systemctl disable --now musegadget.service >/dev/null 2>&1 || true
    fi
    as_root rm -f "$UNIT" /usr/local/bin/musegadget
    if systemd_running; then as_root systemctl daemon-reload; fi
    as_root rm -rf "$PREFIX"
    if [ "$PURGE" = 1 ]; then
        as_root rm -rf "$STATE_DIR"
        say "기기 ID와 연결 정보도 삭제했습니다. Muse 앱에서도 기기를 삭제하세요."
    else
        say "$STATE_DIR의 기기 ID와 연결 정보를 유지했습니다(삭제하려면 --purge 사용)."
    fi
    if [ -f "$BLUEZ_DROPIN" ]; then
        as_root rm -f "$BLUEZ_DROPIN"
        if systemd_running; then
            as_root systemctl daemon-reload
            as_root systemctl restart bluetooth
        fi
        say "BlueZ 배터리 플러그인을 다시 켰습니다."
    fi
    if [ -f "$BLUEZ_CONF.pre-musegadget" ]; then
        say "BlueZ 설정을 그대로 유지했습니다. 원본은 $BLUEZ_CONF.pre-musegadget에 있습니다."
    fi
}

main() {
    SOURCE="$DEFAULT_SOURCE" RUN_AS="" SDK_TOKEN="" ASSUME_YES=0 NO_PAIR=0 UNINSTALL=0 PURGE=0
    while [ $# -gt 0 ]; do
        case "$1" in
            --from) SOURCE="${2:?--from needs a value}"; shift 2 ;;
            --run-as) RUN_AS="${2:?--run-as needs a value}"; shift 2 ;;
            --sdk-token) SDK_TOKEN="${2:?--sdk-token needs a value}"; shift 2 ;;
            --yes|-y) ASSUME_YES=1; shift ;;
            --no-pair) NO_PAIR=1; shift ;;
            --uninstall) UNINSTALL=1; shift ;;
            --purge) PURGE=1; shift ;;
            -h|--help) usage; exit 0 ;;
            *) usage >&2; die "알 수 없는 옵션: $1" ;;
        esac
    done
    if [ "$(id -u)" -ne 0 ]; then
        command -v sudo >/dev/null || die "root로 실행하거나 sudo를 설치하세요."
        sudo true || die "이 설치기는 sudo가 필요합니다."
    fi
    if [ "$UNINSTALL" = 1 ]; then uninstall; return; fi
    if [ -d "$SOURCE" ]; then SOURCE="$(cd "$SOURCE" && pwd)"; fi

    check_system
    prompt_sdk_token
    if [ -n "$SDK_TOKEN" ] && ! [[ "$SDK_TOKEN" =~ ^mgst_[A-Za-z0-9_-]{42}[AEIMQUYcgkosw048]$ ]]; then
        die "SDK 토큰이 올바르지 않습니다. gadgets.muse.ai에서 다시 복사하세요."
    fi
    choose_account
    install_packages
    enable_bluez
    install_uv
    install_musegadget
    configure_bluez
    save_sdk_token
    install_service
    pair
    summary
}

# Everything runs from main, so a truncated download never runs a partial script.
main "$@"
