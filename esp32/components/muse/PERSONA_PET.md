<!--
Copyright (c) Meta Platforms, Inc. and affiliates.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Persona renderer + Tamagotchi pet engine

This fork adds a personality layer on top of the Muse Gadget SDK:

## Persona renderer (`muse_persona.c`)

An original multi-character 64x64 pixel-art renderer implementing the
`muse_pixel.h` API. Selected at build time when `CONFIG_MUSE_PERSONA=y`
(the default); a user-supplied `components/muse/avatar/muse_pixel.c` still
takes precedence.

- **Multi-character**: three original characters — mallow (몰로), sprout
  (새싹), pebble (몽돌) — switchable at runtime via `pose.character`.
- **Dance**: `pose.dance` (0..1) drives rhythmic hops, sways, arm waves and
  squash-and-stretch.
- **Eye tracking**: `pose.gaze_x/gaze_y` (-1..1) steer the pupils. Tapping
  the face feeds the tap point; it eases back to centre after a few seconds.
- **Costumes**: `pose.costume` draws situational accessories — nightcap,
  party hat, headphones, umbrella (with rain), scarf, bandage.
- **Growth**: `pose.growth` renders egg / baby / child / adult. The egg
  wobbles with hatching cracks.
- **Mood**: `pose.mood` (-2..2) shapes brows, mouth and blush.

Preview on the host without a board:

```sh
python3 tools/muse/persona_gifs.py /tmp/persona_gifs
```

## Pet engine (`muse_pet_core.c` + `muse_pet.c`)

A tamagotchi-style raising system. The core is pure C99 and covered by
`tests/test_muse_pet.py`.

- Stats (0..100): satiety, happiness, cleanliness, energy, affection.
- One tick per minute (FreeRTOS task); stats decay, energy restores while
  sleeping. A boot applies one gentle "away" decay.
- Actions: feed, play (starts a dance), wash, sleep/wake, petting.
- Growth: egg (10 min) -> baby (2 h) -> child (8 h) -> adult.
- Neglect (a stat at 0 for 30 min) makes the pet sick (bandage costume,
  sad face); feed + wash + cheer it back to health.
- Persisted in NVS (`muse_pet` namespace) across reboots and reflashes.

## Pet UI (`muse_pet_ui.c`)

A third tile, one swipe left from settings (touch boards only): stat bars
with colour states, FEED / PLAY / WASH / SLEEP action buttons, and the
character switcher (`< name >`). Tapping the face pets the character
(affection up + gaze follows the tap).

Labels are English: the firmware fonts have no Korean glyphs.

## Kconfig

- `MUSE_PERSONA` (default y): the persona renderer.
- `MUSE_PET` (default y, depends on `MUSE_PERSONA`): the pet engine + UI.
