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

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "muse_pet_core.h"

/*
 * Firmware glue for the pet engine: NVS persistence, the 60 s tick task,
 * and the live pose helpers (dance energy, touch gaze) the UI feeds into
 * the persona renderer.
 */

esp_err_t muse_pet_init(void);

/* Copies of the engine state for the UI (mutex-protected). */
void muse_pet_snapshot(pet_state_t *out);

/* Care actions (also persist). */
void muse_pet_feed(void);
void muse_pet_play(void);   /* starts a dance too */
void muse_pet_clean(void);
void muse_pet_touch(void);  /* affection petting */
void muse_pet_toggle_sleep(void);
void muse_pet_set_character(uint8_t id);

/* 0..1 dance energy, fading after play. */
float muse_pet_dance(void);
/* Touch-driven gaze target for eye tracking (-1..1), eases back to 0. */
void muse_pet_gaze(float x, float y);
void muse_pet_get_gaze(float *x, float *y);
