# Muse 기기 SDK 한국어 사용 안내

## 한국어로 표시되는 부분

기기 상태, 연결 승인, 터치 설정, 버튼·키보드 메뉴, 음성 녹음·전송 알림,
배터리·전원 안내, 펫 돌봄 화면과 블루투스 설정 웹페이지를 한국어로 표시합니다.
Linux 명령줄의 연결 안내와 도움말도 한국어입니다.

브랜드명, Wi-Fi·USB·IP·VM·MAC 같은 표준 표기, 명령과 API 필드 이름,
사용자가 입력한 SSID·기기 ID·토큰·서버 주소는 그대로 유지합니다.
서버가 보낸 임의의 오류나 답변 내용을 자동 번역하지는 않습니다.

## 내려받기와 빌드

```sh
git clone --branch persona-pet https://github.com/himomohi/muse-gadget-sdk.git
cd muse-gadget-sdk/esp32
```

ESP-IDF **v6.0.1**을 설치하고 환경을 활성화하세요. 다른 버전은 원본 SDK에서 지원하지 않습니다.
기기별 대상 칩·핀·플래시 크기는 [보드 목록](esp32/devices/README.md)과
[원본 빌드 안내](esp32/README.md)를 기준으로 선택합니다.

```sh
. ~/esp/esp-idf-v6/export.sh
idf.py menuconfig
```

SDK 토큰을 해당 빌드의 설정에 입력합니다. 화면이 있는 Muse 프로필에서는
`CONFIG_MUSE_LANGUAGE_KOREAN=y`가 기본입니다. 이미 만들어 둔 `sdkconfig`는
기본값보다 우선하므로 기존 빌드에는 이 옵션을 직접 켜 주세요.
`CONFIG_MUSE_PERSONA=y`는 캐릭터, `CONFIG_MUSE_PET=y`는 펫 돌봄을 켭니다.

예를 들어 Waveshare S3 1.75C 프로필은 다음과 같이 빌드할 수 있습니다.

```sh
tools/muse/board.sh build s3
```

실제 연결된 보드를 확인한 뒤 그 보드에 맞는 프로필과 포트로 플래시하세요.
재플래시는 기존 와이파이와 앱 연결을 유지합니다.

## 화면 사용

기본 화면에는 **시작 중 / 준비 / 듣는 중 / 생각 중 / 답변 중 / 오류**가 표시됩니다.
터치 화면에서 왼쪽으로 넘기면 **설정**, 다시 왼쪽으로 넘기면 **펫 돌봄**입니다.
설정의 하위 페이지에서는 오른쪽으로 넘기거나 뒤로 버튼으로 돌아갑니다.
버튼 보드에서는 메뉴 버튼이나 키보드의 방향키·Enter·Esc를 사용합니다.

설정에서 와이파이, Muse 서버·VM·토큰, 블루투스, 소리, 밝기, 자동 절전,
배터리 측정, 전원 끄기를 관리합니다. 연결 초기화는 와이파이와 앱 연결을 삭제하고 재시작합니다.

## 펫 돌봄

캐릭터는 **몰로 · 새싹 · 몽돌**입니다.
포만감·행복·청결·체력·친밀도를 확인하고 **먹이 / 놀기 / 씻기 / 재우기**로 돌보세요.
자는 동안에는 **깨우기**가 표시됩니다. 얼굴을 누르면 친밀도가 오르고 시선이 따라옵니다.

알(10분) → 아기(2시간) → 어린이(8시간) → 성체로 성장합니다.
돌봄 상태는 NVS에 저장되므로 재시작하거나 다시 플래시해도 유지됩니다.
커스텀 아바타가 있으면 그 아바타를 우선 사용하며 펫 엔진과 돌봄 메뉴는 유지합니다.

## 휴대폰·웹 설정

Muse 앱에서 개발자 모드를 켜고 `MuseGadget` 기기를 추가하세요.
기기가 연결 승인을 요청하면 화면을 누르거나 안내된 버튼을 누릅니다.

기기의 **설정 → 블루투스 → 휴대폰 연결**을 켠 후 다음 페이지도 사용할 수 있습니다.

```sh
cd tools/muse
python3 -m http.server 8000
```

Chrome에서 `http://localhost:8000/ble_setup.html`을 열고 기기에 연결합니다.
Web Bluetooth는 HTTPS 또는 localhost가 필요합니다. iOS Safari에서는 지원하지 않으므로
Web Bluetooth를 지원하는 브라우저가 필요합니다.
한글 SSID도 전송할 수 있으며 이름은 UTF-8 기준 32바이트 이하로 입력하세요.
기기 화면의 키패드는 비밀번호·주소 입력용 영문·숫자 키를 유지합니다.

## Linux 기기

```sh
cd linux
bash install.sh --from . --run-as 사용자계정
sudo musegadget pair
musegadget info
musegadget send-user-msg "안녕하세요"
```

`--from .`으로 이 포크의 한국어 버전을 설치하세요. 설치기는 명령을 실행할 계정 권한을 안내하고
확인한 뒤 진행합니다. 자세한 하드웨어·서비스 설정은 [Linux 안내](linux/README.md)를 참고하세요.

## 폰트와 번역 수정

한글 음절 **U+AC00~U+D7A3(11,172자)**와 자모를 12px·16px로 포함합니다.
이는 기존 중국어·일본어 CJK 폰트 옵션과 별개이며 한국어 모드가 폰트를 자동으로 포함합니다.
컴팩트 화면에서도 한글 폰트로 대체하고 문자가 잘린 UTF-8 조각으로 끝나지 않도록 처리합니다.

번역은 `esp32/components/muse/translations/ko.json`에서 수정합니다.
`%s`, `%d`, `%u` 같은 형식 인수는 순서·유형을 유지해야 합니다.

```sh
python3 esp32/tools/muse/gen_locale.py
python3 esp32/tools/muse/gen_korean_fonts.py
```

폰트 재생성에는 Node.js/npx가 필요합니다. 일반 빌드는 저장된 압축 폰트를 로컬에서 풀어 사용하므로
폰트 다운로드나 Node.js가 필요하지 않습니다. 영어로 빌드하려면 `CONFIG_MUSE_LANGUAGE_KOREAN=n`으로 설정합니다.

## 검증

```sh
cd esp32
python3 -m unittest discover -s tests -p 'test_*.py'
cd ..
cmake -S esp32/simulator -B build/simulator -G Ninja -DMUSE_SIM_WARNINGS_AS_ERRORS=ON
cmake --build build/simulator --parallel
ctest --test-dir build/simulator --output-on-failure
```

시뮬레이터는 한국어 화면과 한글 전체 음절의 글리프 존재를 검사합니다.
실제 보드에서는 ESP-IDF 빌드·파티션 크기와 연결·터치·음성 동작도 확인해야 합니다.
