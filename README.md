<!--
Copyright (c) Meta Platforms, Inc. and affiliates.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Muse 기기 SDK · 한국어 버전

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset=".github/images/muse-gadgets-dark.png">
    <img src=".github/images/muse-gadgets-light.png" width="900" alt="Muse gadgets: a Waveshare round AMOLED, an M5Stack StickS3, Muse Home Link, a Raspberry Pi and a Seeed reTerminal e-ink display">
  </picture>
</p>

ESP32 보드나 라즈베리 파이·리눅스 컴퓨터를 Muse와 연결하는 오픈소스 SDK입니다.
화면, 버튼, 마이크, 스피커, 센서와 직접 만든 기기를 연결할 수 있습니다.

이 포크의 `persona-pet` 브랜치는 원본 SDK의 최신 구조를 기반으로 한국어 화면과
캐릭터·펫 돌봄 기능을 제공합니다. 원본 소스는 [facebookincubator/muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk)입니다.

| 시작하기 | 내용 |
|---|---|
| [한국어 사용 안내](README.ko.md) | 화면 메뉴, 연결, 펫 돌봄, 언어 설정, 검증 방법 |
| [ESP32 SDK](esp32) | 화면·음성·센서를 사용하는 펌웨어 |
| [Linux SDK](linux) | 라즈베리 파이·리눅스 기기 연결과 관리 |
| [블루투스 설정 화면](esp32/tools/muse/ble_setup.html) | 한국어로 와이파이·기기 토큰·소리·절전 설정 |
| [원본 영어 안내](README.en.md) | 상위 저장소의 소개·라이선스·개발 문서 |

## 연결 준비

먼저 [SDK 토큰](https://gadgets.muse.ai/settings/sdk-tokens)을 발급받고
[Gadget SDK 이용 조건](https://gadgets.muse.ai/sdk-terms)을 확인하세요.
연결하는 각 기기에는 토큰이 필요합니다. 토큰을 소스에 커밋하지 마세요.

휴대폰의 Muse 앱에서 **설정 → 기기 → 개발자 모드**를 켜고 기기를 추가합니다.
`MuseGadget`으로 시작하는 기기를 선택하고 화면이나 버튼으로 연결을 승인하세요.
화면에 표시되는 설정·상태·돌봄 문구는 한국어가 기본입니다.

한국어는 펌웨어의 `CONFIG_MUSE_LANGUAGE_KOREAN=y`로 선택합니다.
영어로 사용하려면 이 옵션을 끄세요. 명령 이름, API, 기기 ID, 토큰, SSID는 바꾸지 않습니다.

## 커뮤니티와 라이선스

기기 제작과 커스텀 사례는 [커뮤니티 Discord](https://discord.gg/3bhjCkZdd6)에서 공유할 수 있습니다.

SDK는 [Apache License 2.0](LICENSE)을 따릅니다. minimp3(CC0), Adafruit 픽셀 폰트(BSD-2-Clause),
EchoEar 패널 초기화 코드(MIT)는 원래 라이선스를 유지합니다.
한글 폰트는 GNU Unifont의 변경하지 않은 부분집합이며 [폰트 안내](esp32/components/muse/fonts/README.md)에
출처와 라이선스를 적었습니다. Jollybot 아바타에는 SDK의 Apache 라이선스가 적용되지 않습니다.
빌드 시 가져오는 ESP-IDF, LVGL, SDL 의존성에도 각각의 라이선스가 적용됩니다.
