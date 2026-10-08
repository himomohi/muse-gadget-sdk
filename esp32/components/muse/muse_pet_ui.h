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

#include "lvgl.h"

/*
 * Pet tile: tamagotchi care UI for the persona renderer. Lives one swipe left
 * of settings on touch boards. Labels are English: the firmware fonts have no
 * Korean glyphs.
 */

void muse_pet_ui_build(lv_obj_t *tile);
/* Periodic refresh; call from the UI tick with whether the tile is visible. */
void muse_pet_ui_tick(bool visible);
