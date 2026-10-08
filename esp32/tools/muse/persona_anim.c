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

/* Host preview dump for the persona renderer: characters, dance, costumes,
 * growth stages and gaze tracking, written as PPMs (<dir>/<name>/NNN.ppm).
 */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include "muse_pixel.h"
#include "muse_persona.h"

#define S 5
#define N (MUSE_PX_W * S)
#define DT 0.04f
#define WARMUP 1.0f

static uint16_t buf[N * N];

typedef struct {
    const char *name;
    muse_mode_t mode;
    float secs;
    uint8_t character;
    uint8_t costume;
    uint8_t growth;
    float dance;
    int gaze_sweep; /* 0 none, 1 sweep gaze_x */
    int8_t mood;
} anim_t;

static const anim_t ANIMS[] = {
    { "mallow_idle", MUSE_MODE_IDLE, 4.0f, PERSONA_MALLOW, COSTUME_NONE, GROWTH_ADULT, 0, 0, 0 },
    { "sprout_idle", MUSE_MODE_IDLE, 4.0f, PERSONA_SPROUT, COSTUME_NONE, GROWTH_ADULT, 0, 0, 0 },
    { "pebble_idle", MUSE_MODE_IDLE, 4.0f, PERSONA_PEBBLE, COSTUME_NONE, GROWTH_ADULT, 0, 0, 0 },
    { "mallow_dance", MUSE_MODE_IDLE, 4.0f, PERSONA_MALLOW, COSTUME_HEADPHONES, GROWTH_ADULT, 1, 0, 1 },
    { "sprout_dance", MUSE_MODE_IDLE, 4.0f, PERSONA_SPROUT, COSTUME_PARTY, GROWTH_ADULT, 0.8f, 0, 2 },
    { "mallow_gaze", MUSE_MODE_IDLE, 4.0f, PERSONA_MALLOW, COSTUME_NONE, GROWTH_ADULT, 0, 1, 0 },
    { "egg", MUSE_MODE_IDLE, 3.0f, PERSONA_MALLOW, COSTUME_NONE, GROWTH_EGG, 0, 0, 0 },
    { "baby", MUSE_MODE_IDLE, 3.0f, PERSONA_SPROUT, COSTUME_NONE, GROWTH_BABY, 0, 0, 1 },
    { "child", MUSE_MODE_IDLE, 3.0f, PERSONA_PEBBLE, COSTUME_NONE, GROWTH_CHILD, 0, 0, 0 },
    { "nightcap", MUSE_MODE_IDLE, 3.0f, PERSONA_MALLOW, COSTUME_NIGHTCAP, GROWTH_ADULT, 0, 0, -1 },
    { "umbrella", MUSE_MODE_IDLE, 3.0f, PERSONA_SPROUT, COSTUME_UMBRELLA, GROWTH_ADULT, 0, 0, 0 },
    { "scarf", MUSE_MODE_IDLE, 3.0f, PERSONA_PEBBLE, COSTUME_SCARF, GROWTH_ADULT, 0, 0, 0 },
    { "bandage", MUSE_MODE_IDLE, 3.0f, PERSONA_MALLOW, COSTUME_BANDAGE, GROWTH_ADULT, 0, 0, -2 },
    { "speaking", MUSE_MODE_SPEAKING, 3.0f, PERSONA_SPROUT, COSTUME_NONE, GROWTH_ADULT, 0, 0, 0 },
    { "listening", MUSE_MODE_LISTENING, 3.0f, PERSONA_PEBBLE, COSTUME_NONE, GROWTH_ADULT, 0, 0, 0 },
};

static float speech_level(float t)
{
    float v = fabsf(sinf(t * 6.3f)) * (0.55f + 0.45f * sinf(t * 1.7f + 1.0f));
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

static void save(const char *path)
{
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6 %d %d 255\n", N, N);
    for (int i = 0; i < N * N; i++) {
        uint16_t c = buf[i];
        uint8_t rgb[3] = { (uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31) };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    float base = 10.0f;
    muse_pixel_set_size(N);

    for (size_t a = 0; a < sizeof(ANIMS) / sizeof(ANIMS[0]); a++) {
        const anim_t *an = &ANIMS[a];
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, an->name);
        mkdir(path, 0755);

        int frames = (int)(an->secs / DT + 0.5f);
        int warm = (int)(WARMUP / DT + 0.5f);
        for (int i = -warm; i < frames; i++) {
            float rt = i * DT;
            muse_pose_t p = {
                .mode = an->mode,
                .t = base + rt,
                .mode_t = rt < 0 ? 0 : rt + 2.0f,
                .level = an->mode == MUSE_MODE_SPEAKING ? speech_level(rt) : 0,
                .happy = 0,
                .gaze_x = an->gaze_sweep ? sinf(rt * 1.4f) : 0,
                .gaze_y = an->gaze_sweep ? 0.3f * cosf(rt * 0.9f) : 0,
                .dance = an->dance,
                .character = an->character,
                .costume = an->costume,
                .growth = an->growth,
                .mood = an->mood,
            };
            muse_pixel_render(&p);
            if (i >= 0) {
                muse_pixel_scale(buf, N, 0, N - 1, 0, N - 1);
                snprintf(path, sizeof(path), "%s/%s/%03d.ppm", dir, an->name, i);
                save(path);
            }
        }
        base += 100.0f;
    }
    return 0;
}
