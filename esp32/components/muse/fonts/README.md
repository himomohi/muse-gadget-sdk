# Fonts

`muse_font_cjk_16.c` is the CJK fallback for the caption font, built in only
with `CONFIG_MUSE_CJK_FONT`. It holds GNU Unifont 16.0.04's 16x16 bitmaps,
the same cell as unscii-16, for CJK punctuation, kana, every CJK Unified
Ideograph (U+4E00 to U+9FFF) and the fullwidth forms: about 850 KB of flash.
`tools/muse/gen_cjk_font.sh` regenerates it.

GNU Unifont is by Roman Czyborra, Paul Hardy and contributors
(https://unifoundry.com/unifont/). Its compiled fonts are licensed under the
SIL Open Font License, version 1.1 (https://openfontlicense.org), and under
the GNU GPL version 2 or later with the GNU font embedding exception. This
file is a conversion of an unaltered subset of the font.

## 한국어 폰트

`muse_font_ko_16.c.gz`와 `muse_font_ko_ui_12.c.gz`는 GNU Unifont 16.0.04의
변경하지 않은 부분집합을 LVGL 형식으로 변환한 생성 소스입니다.
한글 전체 음절 11,172자와 U+1100~11FF, U+3130~318F, U+A960~A97F,
U+D7B0~D7FF 자모를 포함합니다. `CONFIG_MUSE_LANGUAGE_KOREAN`이 켜지면 자동으로 빌드합니다.

원본 OTF SHA-256: `0e3981ab552231b5a2a870f2b61741903a4bf25c23ef5aeb05fdced1b3c7af4d`.
변환 도구는 `lv_font_conv@1.5.3`이며 1bpp를 사용합니다.
`tools/muse/gen_korean_fonts.py`가 동일한 버전과 해시를 검증해 재생성합니다.
생성 소스의 gzip 시간은 0으로 고정하며 일반 빌드에서는 다운로드하지 않습니다.

출처: https://unifoundry.com/unifont/ . 원본 CJK 폰트와 동일하게 SIL Open Font License 1.1
및 GNU GPL v2 이상과 폰트 임베딩 예외가 적용됩니다. 라이선스 정보는 `LICENSE.Unifont.txt`에도 포함합니다.
