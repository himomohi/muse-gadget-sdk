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

#pragma once
#include "lvgl.h"
#include "muse_locale.h"
#ifdef __cplusplus
extern "C" {
#endif
const lv_font_t *muse_locale_font(const lv_font_t *font);
void muse_locale_label_set_text(lv_obj_t *label, const char *text);
#ifdef __cplusplus
}
#endif

/* Opt in only in display modules, after their other includes. Protocol,
 * credentials, network IDs and machine-readable output are never translated. */
#if CONFIG_MUSE_LANGUAGE_KOREAN && !defined(MUSE_LOCALE_UI_IMPLEMENTATION)
#define lv_label_set_text(label, text) muse_locale_label_set_text(label, text)
#define lv_obj_set_style_text_font(obj, font, part) \
    lv_obj_set_style_text_font(obj, muse_locale_font(font), part)
#define lv_textarea_set_placeholder_text(obj, text) \
    lv_textarea_set_placeholder_text(obj, muse_tr(text))
#define lv_font_get_line_height(font) lv_font_get_line_height(muse_locale_font(font))
#define snprintf muse_locale_snprintf
#define vsnprintf muse_locale_vsnprintf
#endif
