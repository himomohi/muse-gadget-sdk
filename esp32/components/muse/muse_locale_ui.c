/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define MUSE_LOCALE_UI_IMPLEMENTATION
#include "muse_locale_ui.h"

#if CONFIG_MUSE_LANGUAGE_KOREAN
LV_FONT_DECLARE(muse_font_ko_16)
LV_FONT_DECLARE(muse_font_ko_ui_12)
static struct {
    const lv_font_t *source;
    lv_font_t font;
    lv_font_t korean;
} FONTS[16];
#endif

const lv_font_t *muse_locale_font(const lv_font_t *font)
{
#if CONFIG_MUSE_LANGUAGE_KOREAN
    if (!font) return font;
    for (size_t i = 0; i < sizeof(FONTS) / sizeof(FONTS[0]); ++i) {
        if (font == &FONTS[i].font) return font;
        if (FONTS[i].source == font) return &FONTS[i].font;
        if (!FONTS[i].source) {
            FONTS[i].source = font;
            FONTS[i].font = *font;
            FONTS[i].korean = font->line_height < 16 ? muse_font_ko_ui_12 : muse_font_ko_16;
            FONTS[i].korean.fallback = font->fallback;
            FONTS[i].font.fallback = &FONTS[i].korean;
            if (FONTS[i].font.line_height < FONTS[i].korean.line_height) {
                FONTS[i].font.line_height = FONTS[i].korean.line_height;
                FONTS[i].font.base_line = FONTS[i].korean.base_line;
            }
            return &FONTS[i].font;
        }
    }
#endif
    return font;
}

void muse_locale_label_set_text(lv_obj_t *label, const char *text)
{
    lv_label_set_text(label, muse_tr(text));
}
