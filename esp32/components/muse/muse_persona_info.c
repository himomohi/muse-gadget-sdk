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

#include "muse_persona.h"

/* Kept separate from the renderer so a custom avatar can still use pet care. */
static const char *const CHAR_IDS[PERSONA_COUNT] = { "mallow", "sprout", "pebble" };
static const char *const CHAR_NAMES[PERSONA_COUNT] = { "몰로", "새싹", "몽돌" };

static const char *const COSTUME_NAMES[COSTUME_COUNT] = {
    [COSTUME_NONE] = "없음",
    [COSTUME_NIGHTCAP] = "잠옷 모자",
    [COSTUME_PARTY] = "파티 고깔",
    [COSTUME_HEADPHONES] = "헤드폰",
    [COSTUME_UMBRELLA] = "우산",
    [COSTUME_SCARF] = "목도리",
    [COSTUME_BANDAGE] = "붕대",
};

uint8_t persona_character_count(void) { return PERSONA_COUNT; }
const char *persona_character_id(uint8_t id)
{
    return id < PERSONA_COUNT ? CHAR_IDS[id] : CHAR_IDS[0];
}
const char *persona_character_name(uint8_t id)
{
    return id < PERSONA_COUNT ? CHAR_NAMES[id] : CHAR_NAMES[0];
}
uint8_t persona_costume_count(void) { return COSTUME_COUNT; }
const char *persona_costume_name(uint8_t id)
{
    return id < COSTUME_COUNT ? COSTUME_NAMES[id] : COSTUME_NAMES[0];
}

