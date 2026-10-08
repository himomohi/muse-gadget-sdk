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
 * Muse persona renderer: an original multi-character 64x64 pixel-art
 * renderer implementing the muse_pixel.h API (muse_pixel_render and
 * friends). It adds, via the extended muse_pose_t fields:
 *
 *  - multi-character: several original characters, switched at runtime
 *  - dance: rhythmic dance moves driven by pose.dance (0..1)
 *  - eye tracking: pupils follow pose.gaze_x/gaze_y (-1..1, touch-driven)
 *  - costumes: situational accessories driven by pose.costume
 *  - growth: egg -> baby -> child -> adult driven by pose.growth
 *  - mood: -2..2 pet mood modifier shaping brows/mouth/blush
 *
 * A user-supplied components/muse/avatar/muse_pixel.c still takes
 * precedence over this renderer at build time (see CMakeLists.txt).
 */

/* Character ids. */
typedef enum {
    PERSONA_MALLOW = 0, /* 몰로: round cream blob, round ears */
    PERSONA_SPROUT = 1, /* 새싹: mint sprout creature, leaf sprout */
    PERSONA_PEBBLE = 2, /* 몽돌: slate rock creature, rocky nubs */
    PERSONA_COUNT,
} persona_character_t;

/* Costume ids. */
typedef enum {
    COSTUME_NONE = 0,
    COSTUME_NIGHTCAP,   /* sleep */
    COSTUME_PARTY,      /* celebration */
    COSTUME_HEADPHONES, /* music / dance */
    COSTUME_UMBRELLA,   /* rain */
    COSTUME_SCARF,      /* chilly */
    COSTUME_BANDAGE,    /* sick */
    COSTUME_COUNT,
} persona_costume_t;

/* Growth stages. */
typedef enum {
    GROWTH_EGG = 0,
    GROWTH_BABY,
    GROWTH_CHILD,
    GROWTH_ADULT,
    GROWTH_COUNT,
} persona_growth_t;

uint8_t persona_character_count(void);
const char *persona_character_id(uint8_t id);   /* "mallow" ... */
const char *persona_character_name(uint8_t id); /* UTF-8 display name */
uint8_t persona_costume_count(void);
const char *persona_costume_name(uint8_t id);
