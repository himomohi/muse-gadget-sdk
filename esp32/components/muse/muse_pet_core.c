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

#include "muse_pet_core.h"

/* Growth stage ids mirror persona_growth_t; kept numeric here so this file
 * stays free of renderer headers. */
#define G_EGG 0
#define G_BABY 1
#define G_CHILD 2
#define G_ADULT 3

/* Costume ids mirror persona_costume_t. */
#define C_NONE 0
#define C_NIGHTCAP 1
#define C_PARTY 2
#define C_HEADPHONES 3
#define C_UMBRELLA 4
#define C_SCARF 5
#define C_BANDAGE 6

static uint8_t sat_add(uint8_t v, int d)
{
    int r = (int)v + d;
    return (uint8_t)(r < 0 ? 0 : (r > 100 ? 100 : r));
}

void pet_init(pet_state_t *s)
{
    s->satiety = 70;
    s->happiness = 70;
    s->cleanliness = 70;
    s->energy = 80;
    s->affection = 50;
    s->ticks = 0;
    s->growth = G_EGG;
    s->character = 0;
    s->sleeping = 0;
    s->sick = 0;
    s->zero_ticks = 0;
    s->play_cool = 0;
    s->feed_cool = 0;
}

static void pet_grow(pet_state_t *s)
{
    if (s->growth == G_EGG && s->ticks >= PET_TICKS_EGG_HATCH) {
        s->growth = G_BABY;
        s->happiness = sat_add(s->happiness, 20); /* hatching joy */
    } else if (s->growth == G_BABY && s->ticks >= PET_TICKS_BABY_GROW) {
        s->growth = G_CHILD;
    } else if (s->growth == G_CHILD && s->ticks >= PET_TICKS_CHILD_GROW) {
        s->growth = G_ADULT;
    }
}

void pet_tick(pet_state_t *s, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint8_t sl = s->sleeping ? 1 : 0;
        if (s->growth != G_EGG) {
            s->satiety = sat_add(s->satiety, sl ? -1 : -2);
            s->cleanliness = sat_add(s->cleanliness, -1);
            if (s->satiety < 20) {
                s->happiness = sat_add(s->happiness, -3);
            } else {
                s->happiness = sat_add(s->happiness, sl ? 0 : -1);
            }
            s->affection = sat_add(s->affection, -1);
        }
        if (s->sleeping) {
            s->energy = sat_add(s->energy, 8);
            if (s->energy >= 100) {
                s->sleeping = 0; /* fully rested: wake up */
            }
        } else {
            s->energy = sat_add(s->energy, s->growth == G_EGG ? 0 : -1);
        }
        if (s->play_cool) {
            s->play_cool--;
        }
        if (s->feed_cool) {
            s->feed_cool--;
        }
        /* sickness bookkeeping */
        if (s->growth != G_EGG &&
            (s->satiety == 0 || s->happiness == 0 || s->cleanliness == 0)) {
            if (s->zero_ticks < 255) {
                s->zero_ticks++;
            }
            if (s->zero_ticks >= PET_SICK_TICKS) {
                s->sick = 1;
            }
        } else {
            s->zero_ticks = 0;
            if (s->sick && s->satiety > 40 && s->cleanliness > 40) {
                s->sick = 0; /* nursed back */
            }
        }
        s->ticks++;
        pet_grow(s);
    }
}

void pet_away(pet_state_t *s)
{
    /* One boot = a gentle absence decay, as if about an hour passed. */
    pet_tick(s, 60);
}

void pet_feed(pet_state_t *s)
{
    if (s->feed_cool || s->growth == G_EGG) {
        return;
    }
    if (s->satiety >= 95) {
        s->happiness = sat_add(s->happiness, 2); /* a snack for joy */
    } else {
        s->satiety = sat_add(s->satiety, 25);
        s->happiness = sat_add(s->happiness, 5);
    }
    s->cleanliness = sat_add(s->cleanliness, -3); /* messy eater */
    s->feed_cool = 5;
}

void pet_play(pet_state_t *s)
{
    if (s->play_cool || s->sleeping || s->growth == G_EGG) {
        return;
    }
    s->happiness = sat_add(s->happiness, 20);
    s->affection = sat_add(s->affection, 6);
    s->energy = sat_add(s->energy, -10);
    s->satiety = sat_add(s->satiety, -5);
    s->play_cool = 5;
}

void pet_clean(pet_state_t *s)
{
    if (s->growth == G_EGG) {
        return;
    }
    s->cleanliness = 100;
    s->happiness = sat_add(s->happiness, 5);
}

void pet_pet(pet_state_t *s)
{
    if (s->growth == G_EGG) {
        return;
    }
    s->affection = sat_add(s->affection, 8);
    s->happiness = sat_add(s->happiness, 8);
}

void pet_toggle_sleep(pet_state_t *s)
{
    if (s->growth == G_EGG) {
        return;
    }
    s->sleeping = !s->sleeping;
}

int8_t pet_mood(const pet_state_t *s)
{
    if (s->sick) {
        return -2;
    }
    if (s->sleeping) {
        return -1;
    }
    uint16_t avg = (s->satiety + s->happiness + s->cleanliness + s->energy + s->affection) / 5;
    if (avg < 25) {
        return -2;
    }
    if (avg < 45) {
        return -1;
    }
    if (avg < 70) {
        return 0;
    }
    if (avg < 90) {
        return 1;
    }
    return 2;
}

uint8_t pet_costume(const pet_state_t *s)
{
    if (s->sleeping) {
        return C_NIGHTCAP;
    }
    if (s->sick) {
        return C_BANDAGE;
    }
    return C_NONE;
}

uint8_t pet_care(const pet_state_t *s)
{
    return (s->satiety + s->happiness + s->cleanliness + s->energy + s->affection) / 5;
}

uint8_t pet_save(const pet_state_t *s, uint8_t *out)
{
    out[0] = 1; /* version */
    out[1] = s->satiety;
    out[2] = s->happiness;
    out[3] = s->cleanliness;
    out[4] = s->energy;
    out[5] = s->affection;
    out[6] = (uint8_t)(s->ticks & 0xff);
    out[7] = (uint8_t)((s->ticks >> 8) & 0xff);
    out[8] = (uint8_t)((s->ticks >> 16) & 0xff);
    out[9] = (uint8_t)((s->ticks >> 24) & 0xff);
    out[10] = s->growth;
    out[11] = s->character;
    out[12] = s->sleeping;
    out[13] = s->sick;
    out[14] = s->zero_ticks;
    out[15] = s->play_cool;
    out[16] = s->feed_cool;
    return 17;
}

int pet_load(pet_state_t *s, const uint8_t *in, uint8_t len)
{
    if (len < 17 || in[0] != 1) {
        return -1;
    }
    s->satiety = in[1] > 100 ? 100 : in[1];
    s->happiness = in[2] > 100 ? 100 : in[2];
    s->cleanliness = in[3] > 100 ? 100 : in[3];
    s->energy = in[4] > 100 ? 100 : in[4];
    s->affection = in[5] > 100 ? 100 : in[5];
    s->ticks = (uint32_t)in[6] | ((uint32_t)in[7] << 8) | ((uint32_t)in[8] << 16) |
               ((uint32_t)in[9] << 24);
    s->growth = in[10] > G_ADULT ? G_ADULT : in[10];
    s->character = in[11] > 2 ? 0 : in[11];
    s->sleeping = in[12] ? 1 : 0;
    s->sick = in[13] ? 1 : 0;
    s->zero_ticks = in[14];
    s->play_cool = in[15];
    s->feed_cool = in[16];
    return 0;
}
