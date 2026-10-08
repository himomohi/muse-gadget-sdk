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

#include <stdint.h>

/*
 * Tamagotchi-style pet engine core. Pure C99, no ESP-IDF dependency, so the
 * host unit tests can compile it directly. The firmware glue
 * (muse_pet.c) owns the NVS persistence and the 60 s tick timer.
 *
 * Time unit: one tick = one minute of awake life.
 */

#define PET_TICKS_EGG_HATCH 10    /* egg -> baby */
#define PET_TICKS_BABY_GROW 120   /* baby -> child (2 h) */
#define PET_TICKS_CHILD_GROW 480  /* child -> adult (8 h) */

#define PET_SICK_TICKS 30         /* a zeroed stat this long -> sick */

typedef struct {
    uint8_t satiety;     /* 포만감 0..100 */
    uint8_t happiness;   /* 행복도 0..100 */
    uint8_t cleanliness; /* 청결도 0..100 */
    uint8_t energy;      /* 체력 0..100 */
    uint8_t affection;   /* 친밀도 0..100 */
    uint32_t ticks;      /* minutes lived */
    uint8_t growth;      /* persona_growth_t */
    uint8_t character;   /* persona_character_t */
    uint8_t sleeping;    /* bool */
    uint8_t sick;        /* bool */
    uint8_t zero_ticks;  /* consecutive ticks with a stat at 0 */
    uint8_t play_cool;   /* play cooldown, ticks */
    uint8_t feed_cool;   /* feed cooldown, ticks */
} pet_state_t;

void pet_init(pet_state_t *s);              /* fresh egg */
void pet_tick(pet_state_t *s, uint32_t n);  /* advance n minutes */
void pet_away(pet_state_t *s);              /* one boot's absence decay */

void pet_feed(pet_state_t *s);   /* 밥주기 */
void pet_play(pet_state_t *s);   /* 놀아주기 (dance!) */
void pet_clean(pet_state_t *s);  /* 씻기기 */
void pet_pet(pet_state_t *s);    /* 쓰다듬기 */
void pet_toggle_sleep(pet_state_t *s);

/* -2..2 mood modifier for the renderer. */
int8_t pet_mood(const pet_state_t *s);
/* Suggested costume for the current condition. */
uint8_t pet_costume(const pet_state_t *s);
/* 0..100 overall care score. */
uint8_t pet_care(const pet_state_t *s);

/* Blob serialisation for NVS. Returns bytes written. */
#define PET_BLOB_MAX 32
uint8_t pet_save(const pet_state_t *s, uint8_t *out);
int pet_load(pet_state_t *s, const uint8_t *in, uint8_t len); /* 0 ok */
