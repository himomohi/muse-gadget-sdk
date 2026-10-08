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

#include "muse_pet_ui.h"

#include <stdio.h>

#include "muse_persona.h"
#include "muse_pet.h"

#define COLOR_TEXT 0xf2efff
#define COLOR_DIM 0x8b84a8
#define COLOR_CARD 0x1a1530
#define COLOR_ACCENT 0xa77dff
#define COLOR_OK 0x6ff0bf
#define COLOR_WARN 0xffb45c
#define COLOR_DANGER 0xff5c5c

static lv_obj_t *s_title;
static lv_obj_t *s_bars[5];
static lv_obj_t *s_vals[5];
static lv_obj_t *s_status;
static lv_obj_t *s_sleep_btn;
static lv_obj_t *s_sleep_lbl;
static uint32_t s_last_drawn; /* pet.ticks at last refresh */

static const char *const STAT_NAMES[5] = { "SAT", "JOY", "CLEAN", "NRG", "LOVE" };
static const char *const GROWTH_NAMES[4] = { "egg", "baby", "child", "adult" };

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, "");
    return l;
}

static void on_feed(lv_event_t *e)
{
    (void)e;
    muse_pet_feed();
    s_last_drawn = 0; /* force refresh */
}

static void on_play(lv_event_t *e)
{
    (void)e;
    muse_pet_play();
    s_last_drawn = 0;
}

static void on_clean(lv_event_t *e)
{
    (void)e;
    muse_pet_clean();
    s_last_drawn = 0;
}

static void on_sleep(lv_event_t *e)
{
    (void)e;
    muse_pet_toggle_sleep();
    s_last_drawn = 0;
}

static void on_char_prev(lv_event_t *e)
{
    (void)e;
    pet_state_t s;
    muse_pet_snapshot(&s);
    uint8_t n = persona_character_count();
    muse_pet_set_character((s.character + n - 1) % n);
    s_last_drawn = 0;
}

static void on_char_next(lv_event_t *e)
{
    (void)e;
    pet_state_t s;
    muse_pet_snapshot(&s);
    muse_pet_set_character((s.character + 1) % persona_character_count());
    s_last_drawn = 0;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int w, int h)
{
    lv_obj_t *b = lv_btn_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_CARD), 0);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_t *l = make_label(b, &lv_font_montserrat_16, COLOR_TEXT);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    return b;
}

void muse_pet_ui_build(lv_obj_t *tile)
{
    /* Title row: < name · stage > */
    lv_obj_t *row = lv_obj_create(tile);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, 330, 44);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 78);

    lv_obj_t *prev = make_button(row, LV_SYMBOL_LEFT, 52, 40);
    lv_obj_add_event_cb(prev, on_char_prev, LV_EVENT_CLICKED, NULL);
    s_title = make_label(row, &lv_font_montserrat_20, COLOR_TEXT);
    lv_obj_set_width(s_title, 190);
    lv_obj_t *next = make_button(row, LV_SYMBOL_RIGHT, 52, 40);
    lv_obj_add_event_cb(next, on_char_next, LV_EVENT_CLICKED, NULL);

    /* Stat bars */
    for (int i = 0; i < 5; i++) {
        lv_obj_t *r = lv_obj_create(tile);
        lv_obj_remove_style_all(r);
        lv_obj_set_size(r, 330, 30);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 128 + i * 32);

        lv_obj_t *name = make_label(r, &lv_font_montserrat_14, COLOR_DIM);
        lv_label_set_text(name, STAT_NAMES[i]);
        lv_obj_set_width(name, 64);

        s_bars[i] = lv_bar_create(r);
        lv_obj_set_size(s_bars[i], 200, 14);
        lv_bar_set_range(s_bars[i], 0, 100);
        lv_obj_set_style_bg_color(s_bars[i], lv_color_hex(0x2a2440), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_bars[i], lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);

        s_vals[i] = make_label(r, &lv_font_montserrat_14, COLOR_TEXT);
        lv_obj_set_width(s_vals[i], 44);
    }

    /* Action buttons 2x2 */
    static const struct {
        const char *label;
        void (*cb)(lv_event_t *);
    } actions[4] = {
        { "FEED", on_feed }, { "PLAY", on_play }, { "WASH", on_clean }, { NULL, on_sleep },
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = make_button(tile, actions[i].label ? actions[i].label : "SLEEP", 150, 52);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (i % 2 ? 82 : -82), 300 + (i / 2) * 60);
        lv_obj_add_event_cb(b, actions[i].cb, LV_EVENT_CLICKED, NULL);
        if (!actions[i].label) {
            s_sleep_btn = b;
            s_sleep_lbl = lv_obj_get_child(b, 0);
        }
    }

    s_status = make_label(tile, &lv_font_montserrat_16, COLOR_DIM);
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 428);
}

static const char *mood_text(const pet_state_t *s)
{
    if (s->growth == 0) {
        return "an egg... keep it warm";
    }
    if (s->sick) {
        return "sick! wash + feed";
    }
    if (s->sleeping) {
        return "sleeping... zzz";
    }
    if (s->satiety < 20) {
        return "hungry!";
    }
    if (s->happiness < 20) {
        return "bored... play!";
    }
    if (s->cleanliness < 20) {
        return "dirty! wash me";
    }
    if (s->energy < 20) {
        return "tired... sleep?";
    }
    int8_t m = pet_mood(s);
    if (m >= 2) {
        return "over the moon!";
    }
    if (m == 1) {
        return "happy";
    }
    if (m == 0) {
        return "doing ok";
    }
    return "needs care";
}

void muse_pet_ui_tick(bool visible)
{
    if (!visible) {
        return;
    }
    pet_state_t s;
    muse_pet_snapshot(&s);
    if (s.ticks == s_last_drawn) {
        return;
    }
    s_last_drawn = s.ticks;

    char buf[48];
    snprintf(buf, sizeof(buf), "%s - %s", persona_character_id(s.character),
             GROWTH_NAMES[s.growth < 4 ? s.growth : 3]);
    lv_label_set_text(s_title, buf);

    uint8_t vals[5] = { s.satiety, s.happiness, s.cleanliness, s.energy, s.affection };
    for (int i = 0; i < 5; i++) {
        lv_bar_set_value(s_bars[i], vals[i], LV_ANIM_OFF);
        uint32_t col = vals[i] < 20 ? COLOR_DANGER : vals[i] < 45 ? COLOR_WARN : COLOR_OK;
        lv_obj_set_style_bg_color(s_bars[i], lv_color_hex(col), LV_PART_INDICATOR);
        snprintf(buf, sizeof(buf), "%d", vals[i]);
        lv_label_set_text(s_vals[i], buf);
    }
    lv_label_set_text(s_status, mood_text(&s));
    if (s_sleep_lbl) {
        lv_label_set_text(s_sleep_lbl, s.sleeping ? "WAKE" : "SLEEP");
    }
}
