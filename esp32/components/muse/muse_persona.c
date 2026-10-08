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

/*
 * Persona renderer: original characters drawn on a 64x64 grid, flat-shaded
 * with dithered edges. Cheap enough per pixel for chips without an FPU.
 */
#include "muse_persona.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "muse_pixel.h"

#define W MUSE_PX_W
#define H MUSE_PX_H
#define TAU 6.2831853f

/* ------------------------------------------------------------------ palette */

enum {
    C_BG = 0,
    C_OUT,
    C_BD0,   /* body dark */
    C_BD1,   /* body mid */
    C_BD2,   /* body light */
    C_FACE,
    C_FACED,
    C_EYE,
    C_SHINE,
    C_BROW,
    C_BLUSH,
    C_MOUTH,
    C_TONGUE,
    C_ACC,   /* per-character accent */
    C_ACC2,
    C_GLOW,
    C_WHITE,
    C_SHADOW,
    C_HEART,
    C_LEAF,
    C_LEAFD,
    C_COUNT,
};

typedef struct {
    float r, g, b;
} rgb_t;

typedef struct {
    const char *id;
    const char *name; /* UTF-8 */
    uint32_t body_dark, body_mid, body_light;
    uint32_t face, face_dark;
    uint32_t accent, accent2;
    uint8_t ear;      /* 0 round, 1 sprout leaves, 2 rock nubs */
    uint8_t shape;    /* 0 round, 1 tall, 2 wide */
    uint8_t eye;      /* 0 bead, 1 oval, 2 droopy-cute */
    float bounce;     /* idle bounce amplitude */
    float blink_min, blink_max;
} chardef_t;

static const chardef_t CHARS[PERSONA_COUNT] = {
    [PERSONA_MALLOW] = {
        .id = "mallow", .name = "몰로",
        .body_dark = 0xd9b48f, .body_mid = 0xf0d3ac, .body_light = 0xfbeed3,
        .face = 0xfff6e6, .face_dark = 0xf3ddba,
        .accent = 0xff9ecf, .accent2 = 0xb78cff,
        .ear = 0, .shape = 0, .eye = 0,
        .bounce = 1.0f, .blink_min = 2.0f, .blink_max = 5.0f,
    },
    [PERSONA_SPROUT] = {
        .id = "sprout", .name = "새싹",
        .body_dark = 0x7fb069, .body_mid = 0xa3d18b, .body_light = 0xcdebb8,
        .face = 0xf4ffe9, .face_dark = 0xd9ecbf,
        .accent = 0x4fc96b, .accent2 = 0x2a9d8f,
        .ear = 1, .shape = 1, .eye = 1,
        .bounce = 1.4f, .blink_min = 1.6f, .blink_max = 4.2f,
    },
    [PERSONA_PEBBLE] = {
        .id = "pebble", .name = "몽돌",
        .body_dark = 0x6b7688, .body_mid = 0x8d99ae, .body_light = 0xb9c4d6,
        .face = 0xeef2fa, .face_dark = 0xd3dcea,
        .accent = 0x7fd4ff, .accent2 = 0x5a7dff,
        .ear = 2, .shape = 2, .eye = 2,
        .bounce = 0.6f, .blink_min = 2.6f, .blink_max = 6.0f,
    },
};

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
    return id < PERSONA_COUNT ? CHARS[id].id : CHARS[0].id;
}
const char *persona_character_name(uint8_t id)
{
    return id < PERSONA_COUNT ? CHARS[id].name : CHARS[0].name;
}
uint8_t persona_costume_count(void) { return COSTUME_COUNT; }
const char *persona_costume_name(uint8_t id)
{
    return id < COSTUME_COUNT ? COSTUME_NAMES[id] : COSTUME_NAMES[0];
}

/* Per-mode accent colours for the surrounding UI. */
uint32_t muse_pixel_accent(muse_mode_t mode)
{
    switch (mode) {
    case MUSE_MODE_LISTENING: return 0x5cb8ff;
    case MUSE_MODE_THINKING:  return 0xe07bff;
    case MUSE_MODE_SPEAKING:  return 0x6ff0bf;
    case MUSE_MODE_ERROR:     return 0xff5c5c;
    case MUSE_MODE_OFF:       return 0x7c72d0;
    case MUSE_MODE_BOOT:      return 0xa9c0ff;
    default:                  return 0xa77dff;
    }
}

/* ------------------------------------------------------------------ helpers */

static uint16_t s_pal[C_COUNT];
static uint8_t s_fb[W * H];
static uint8_t s_mask[W * H];

static inline rgb_t hex_rgb(uint32_t c)
{
    return (rgb_t){ (float)((c >> 16) & 0xff), (float)((c >> 8) & 0xff), (float)(c & 0xff) };
}

static inline uint16_t to565(rgb_t c)
{
    int r = (int)(c.r + 0.5f), g = (int)(c.g + 0.5f), b = (int)(c.b + 0.5f);
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void load_palette(const chardef_t *ch)
{
    uint32_t fixed[C_COUNT] = {
        [C_BG] = 0x0b0b14,
        [C_OUT] = 0x2c2430,
        [C_EYE] = 0x1d1512,
        [C_SHINE] = 0xffffff,
        [C_BROW] = 0x5a4a3f,
        [C_BLUSH] = 0xf4aaa0,
        [C_MOUTH] = 0x40211c,
        [C_TONGUE] = 0xe86a7a,
        [C_WHITE] = 0xffffff,
        [C_SHADOW] = 0x141020,
        [C_HEART] = 0xff4f8b,
        [C_LEAF] = 0x4fc96b,
        [C_LEAFD] = 0x2f8f4e,
    };
    fixed[C_BD0] = ch->body_dark;
    fixed[C_BD1] = ch->body_mid;
    fixed[C_BD2] = ch->body_light;
    fixed[C_FACE] = ch->face;
    fixed[C_FACED] = ch->face_dark;
    fixed[C_ACC] = ch->accent;
    fixed[C_ACC2] = ch->accent2;
    fixed[C_GLOW] = ch->accent;
    for (int i = 0; i < C_COUNT; i++) {
        s_pal[i] = to565(hex_rgb(fixed[i]));
    }
}

static inline void px(int x, int y, uint8_t c)
{
    if ((unsigned)x < W && (unsigned)y < H) {
        s_fb[y * W + x] = c;
    }
}

static inline int iround(float v) { return (int)floorf(v + 0.5f); }
static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float fracf(float v) { return v - floorf(v); }

static uint32_t s_rng = 0x1234567u;
static float frand(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return (float)(s_rng & 0xffffff) / (float)0x1000000;
}

static const uint8_t BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
static inline float bayer(int x, int y)
{
    return (BAYER[((y & 3) << 2) | (x & 3)] + 0.5f) / 16.0f;
}

/* Filled disc. */
static void disc(float cx, float cy, float r, uint8_t c)
{
    int x0 = iround(cx - r), x1 = iround(cx + r);
    int y0 = iround(cy - r), y1 = iround(cy + r);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy <= r * r + 0.5f) {
                px(x, y, c);
            }
        }
    }
}

/* Filled axis-aligned ellipse. */
static void ell(float cx, float cy, float rx, float ry, uint8_t c)
{
    int x0 = iround(cx - rx), x1 = iround(cx + rx);
    int y0 = iround(cy - ry), y1 = iround(cy + ry);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) {
                px(x, y, c);
            }
        }
    }
}

/* Thick line segment. */
static void seg(float x0, float y0, float x1, float y1, float w, uint8_t c)
{
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    int n = (int)(len * 1.5f) + 1;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n;
        disc(x0 + dx * t, y0 + dy * t, w * 0.5f, c);
    }
}

/* Bitmap stamp: '#' fill, 'o' alt. */
static void stamp(const char *const *rows, int nrows, int x0, int y0, uint8_t fill, uint8_t alt)
{
    for (int r = 0; r < nrows; r++) {
        for (int c = 0; rows[r][c]; c++) {
            char ch = rows[r][c];
            if (ch == '#') {
                px(x0 + c, y0 + r, fill);
            } else if (ch == 'o') {
                px(x0 + c, y0 + r, alt);
            }
        }
    }
}

static void draw_sparkles_lite(float cx, float cy, float t);

/* --------------------------------------------------------------- character */

typedef struct {
    float cx, cy;   /* body centre */
    float a, b;     /* half width / half height */
    float fx, fy;   /* face centre */
    float fa, fb;   /* face half extents */
    float top;      /* head top y */
} body_t;

/* Eye state, eased toward the touch-driven gaze target. */
typedef struct {
    float next_blink;
    float blink_start;
    float gx, gy;   /* smoothed gaze -1..1 */
    float last_t;
} eyes_t;

static eyes_t s_eyes = { .next_blink = 2.0f, .blink_start = -10.0f };

static float eyes_update(const chardef_t *ch, float t, float dt, float want_gx, float want_gy,
                         muse_mode_t mode, float mode_t)
{
    eyes_t *e = &s_eyes;
    if (t >= e->next_blink) {
        e->blink_start = t;
        e->next_blink = t + ch->blink_min + frand() * (ch->blink_max - ch->blink_min);
    }
    /* Gaze: follow the touch point when given, else wander. */
    float tgx, tgy;
    if (fabsf(want_gx) > 0.05f || fabsf(want_gy) > 0.05f) {
        tgx = clampf(want_gx, -1, 1);
        tgy = clampf(want_gy, -1, 1) * 0.7f;
    } else {
        tgx = 0.55f * sinf(t * 0.9f);
        tgy = 0.35f * sinf(t * 0.63f + 1.7f);
    }
    if (mode == MUSE_MODE_THINKING) {
        tgx = 0.8f * sinf(mode_t * 1.3f);
        tgy = -0.9f;
    } else if (mode == MUSE_MODE_LISTENING) {
        tgx *= 0.4f;
        tgy = 0.15f;
    }
    float k = 1.0f - expf(-dt * 10.0f);
    e->gx += (tgx - e->gx) * k;
    e->gy += (tgy - e->gy) * k;

    float bt = (t - e->blink_start) / 0.16f;
    if (bt < 0 || bt > 1) {
        return 0;
    }
    return 1.0f - fabsf(bt * 2 - 1);
}

typedef enum { E_NORMAL, E_HAPPY, E_X, E_SLEEPY } eye_style_t;

static void draw_eye(const chardef_t *ch, float ex, float ey, float openness,
                     eye_style_t style, float gx, float gy, float scale)
{
    int ox = iround(gx * 1.6f * scale), oy = iround(gy * 1.2f * scale);
    if (style == E_HAPPY) {
        static const char *const HP[] = { ".##.", "#..#" };
        stamp(HP, 2, iround(ex) - 2 + ox / 2, iround(ey) + oy / 2, C_EYE, C_EYE);
        return;
    }
    if (style == E_X) {
        static const char *const XX[] = { "#..#", ".##.", ".##.", "#..#" };
        stamp(XX, 4, iround(ex) - 2, iround(ey) - 1, C_EYE, C_EYE);
        return;
    }
    if (style == E_SLEEPY || openness < 0.35f) {
        static const char *const SH[] = { "####" };
        stamp(SH, 1, iround(ex) - 2 + ox / 2, iround(ey) + 1, C_EYE, C_EYE);
        return;
    }
    if (ch->eye == 1) {
        /* oval eyes */
        ell(ex + ox, ey + oy, 2.2f * scale, 3.0f * scale, C_EYE);
        disc(ex + ox - 0.7f * scale, ey + oy - 1.0f * scale, 0.9f * scale, C_SHINE);
    } else if (ch->eye == 2) {
        /* droopy-cute half-lidded beads */
        ell(ex + ox, ey + oy, 2.0f * scale, 2.0f * scale, C_EYE);
        disc(ex + ox - 0.6f * scale, ey + oy - 0.6f * scale, 0.7f * scale, C_SHINE);
        seg(ex - 2.4f * scale, ey - 1.8f * scale, ex + 2.4f * scale, ey - 1.8f * scale,
            1.2f * scale, C_BD0);
    } else {
        static const char *const B[] = { ".##.", "#o##", "####", ".##." };
        stamp(B, 4, iround(ex) - 2 + ox, iround(ey) - 2 + oy, C_EYE, C_SHINE);
    }
}

typedef enum { MO_SMILE, MO_O, MO_HMM, MO_TALK, MO_GRIN, MO_FLAT, MO_SAD } mouth_t;

static void draw_mouth(float x, float y, mouth_t m, float open, float scale)
{
    int ix = iround(x), iy = iround(y);
    switch (m) {
    case MO_SMILE: {
        static const char *const S[] = { "#...#", ".###." };
        stamp(S, 2, ix - 2, iy, C_MOUTH, C_MOUTH);
        break;
    }
    case MO_SAD: {
        static const char *const S[] = { ".###.", "#...#" };
        stamp(S, 2, ix - 2, iy, C_MOUTH, C_MOUTH);
        break;
    }
    case MO_O:
        ell(x, y, 1.8f * scale, 2.4f * scale, C_MOUTH);
        ell(x, y + 0.6f * scale, 1.0f * scale, 1.2f * scale, C_TONGUE);
        break;
    case MO_HMM: {
        static const char *const S[] = { "..#", "##." };
        stamp(S, 2, ix - 1, iy, C_MOUTH, C_MOUTH);
        break;
    }
    case MO_TALK: {
        int h = 1 + iround(clampf(open, 0, 1) * 3.0f);
        for (int j = 0; j < h; j++) {
            for (int i = -1; i <= 1; i++) {
                px(ix + i, iy + j, (h >= 3 && j == h - 1) ? C_TONGUE : C_MOUTH);
            }
        }
        break;
    }
    case MO_GRIN: {
        static const char *const S[] = { "######", "#o##o#", ".####." };
        stamp(S, 3, ix - 3, iy, C_MOUTH, C_TONGUE);
        break;
    }
    case MO_FLAT: {
        static const char *const S[] = { "####" };
        stamp(S, 1, ix - 2, iy + 1, C_MOUTH, C_MOUTH);
        break;
    }
    }
}

static void draw_blush(float x, float y, float strength, float scale)
{
    if (strength <= 0.05f) {
        return;
    }
    int n = strength > 0.6f ? 3 : 2;
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < n; i++) {
            if (bayer(iround(x) + i, iround(y) + j) < strength) {
                px(iround(x) - n / 2 + i, iround(y) + j, C_BLUSH);
            }
        }
    }
}

/* Body fill: superellipse with a soft vertical shade. */
static void draw_body(const chardef_t *ch, const body_t *bd, float shade_top)
{
    float n = ch->shape == 1 ? 2.6f : ch->shape == 2 ? 2.2f : 2.8f;
    int x0 = (int)(bd->cx - bd->a - 2), x1 = (int)(bd->cx + bd->a + 2);
    int y0 = (int)(bd->cy - bd->b - 2), y1 = (int)(bd->cy + bd->b + 2);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = fabsf(x + 0.5f - bd->cx) / bd->a;
            float dy = fabsf(y + 0.5f - bd->cy) / bd->b;
            float f = powf(dx, n) + powf(dy, n);
            if (f > 1.06f) {
                continue;
            }
            uint8_t c;
            float v = (y + 0.5f - (bd->cy - bd->b)) / (2 * bd->b); /* 0 top .. 1 bottom */
            if (f > 0.86f) {
                c = C_OUT; /* edge */
            } else if (v < 0.35f) {
                c = C_BD2;
            } else if (v < 0.7f) {
                c = C_BD1;
            } else {
                c = C_BD0;
            }
            /* face panel */
            float fx = fabsf(x + 0.5f - bd->fx) / bd->fa;
            float fy = fabsf(y + 0.5f - bd->fy) / bd->fb;
            if (fx * fx + fy * fy <= 1.0f && f <= 0.98f) {
                c = v < 0.5f ? C_FACE : C_FACED;
            }
            px(x, y, c);
            s_mask[y * W + x] = 1;
        }
    }
    (void)shade_top;
}

/* Per-character head appendages. */
static void draw_ears(const chardef_t *ch, const body_t *bd, float t, float dance)
{
    float top = bd->top;
    if (ch->ear == 0) {
        /* round ears */
        float wig = sinf(t * 2.0f) * 0.8f + dance * sinf(t * 9.0f) * 2.0f;
        ell(bd->cx - bd->a * 0.62f, top + 2 + wig * 0.3f, 4.2f, 4.6f, C_BD1);
        ell(bd->cx - bd->a * 0.62f, top + 2 + wig * 0.3f, 2.2f, 2.6f, C_FACE);
        ell(bd->cx + bd->a * 0.62f, top + 2 - wig * 0.3f, 4.2f, 4.6f, C_BD1);
        ell(bd->cx + bd->a * 0.62f, top + 2 - wig * 0.3f, 2.2f, 2.6f, C_FACE);
    } else if (ch->ear == 1) {
        /* sprout: stem + two wiggling leaves */
        float sway = sinf(t * 2.4f) * 1.5f + dance * sinf(t * 10.0f) * 3.0f;
        seg(bd->cx, top + 2, bd->cx + sway * 0.4f, top - 5, 1.6f, C_LEAFD);
        float lx = bd->cx + sway * 0.4f;
        ell(lx - 4.5f, top - 7 + sway * 0.2f, 4.0f, 2.2f, C_LEAF);
        ell(lx + 4.5f, top - 8 - sway * 0.2f, 4.0f, 2.2f, C_LEAF);
        ell(lx - 4.5f, top - 7.4f + sway * 0.2f, 2.0f, 1.0f, C_WHITE);
    } else {
        /* rock nubs */
        ell(bd->cx - bd->a * 0.55f, top + 1, 3.0f, 2.4f, C_BD0);
        ell(bd->cx + bd->a * 0.5f, top + 0, 2.4f, 2.0f, C_BD0);
        /* cracks */
        seg(bd->cx - 6, bd->cy + 8, bd->cx - 2, bd->cy + 12, 1.0f, C_BD0);
        seg(bd->cx - 2, bd->cy + 12, bd->cx - 4, bd->cy + 15, 1.0f, C_BD0);
    }
}

static void draw_limbs(const chardef_t *ch, const body_t *bd, float t, float dance, float happy,
                       muse_mode_t mode)
{
    float base = bd->cy + bd->b;
    /* feet */
    ell(bd->cx - 6.5f, base - 1, 4.0f, 2.4f, C_BD0);
    ell(bd->cx + 6.5f, base - 1, 4.0f, 2.4f, C_BD0);
    /* arms */
    float ax = bd->a + 1.0f;
    float ay = bd->cy + 3.0f;
    if (dance > 0.05f) {
        float w = sinf(t * (7.0f + 5.0f * dance)) * (0.6f + 0.4f * dance);
        seg(bd->cx - ax, ay, bd->cx - ax - 3 - w * 3.0f, ay - 9, 3.2f, C_BD1);
        seg(bd->cx + ax, ay, bd->cx + ax + 3 + w * 3.0f, ay - 9, 3.2f, C_BD1);
    } else if (happy > 0.2f) {
        float w = sinf(t * 13.0f) * 2.0f * happy;
        seg(bd->cx - ax, ay, bd->cx - ax - 2, ay - 8 + w, 3.0f, C_BD1);
        seg(bd->cx + ax, ay, bd->cx + ax + 2, ay - 8 - w, 3.0f, C_BD1);
    } else if (mode == MUSE_MODE_LISTENING) {
        seg(bd->cx - ax + 1, ay + 2, bd->cx - ax + 3, bd->fy + 4, 3.0f, C_BD1);
        seg(bd->cx + ax - 1, ay + 2, bd->cx + ax - 3, bd->fy + 4, 3.0f, C_BD1);
    } else {
        float sway = sinf(t * 1.8f) * 1.0f;
        seg(bd->cx - ax, ay, bd->cx - ax - 1 + sway, ay + 7, 3.0f, C_BD1);
        seg(bd->cx + ax, ay, bd->cx + ax + 1 - sway, ay + 7, 3.0f, C_BD1);
    }
    (void)ch;
}

/* ------------------------------------------------------------------ egg */

static void draw_egg(float cx, float cy, float t, float hatch)
{
    /* egg body */
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float dx = (x + 0.5f - cx) / 13.0f;
            float dy = (y + 0.5f - cy) / 16.0f;
            /* narrower at the top */
            float w = 1.0f - 0.25f * clampf((cy - (y + 0.5f)) / 16.0f, 0, 1);
            float f = (dx / w) * (dx / w) + dy * dy;
            if (f > 1.05f) {
                continue;
            }
            uint8_t c = f > 0.85f ? C_OUT : (dy < -0.2f ? C_WHITE : (dy < 0.4f ? C_FACE : C_FACED));
            px(x, y, c);
            s_mask[y * W + x] = 1;
        }
    }
    /* wobble */
    /* cracks as hatching nears */
    if (hatch > 0.3f) {
        seg(cx - 8, cy - 2, cx - 2, cy + 2, 1.2f, C_BD0);
        seg(cx - 2, cy + 2, cx + 3, cy - 1, 1.2f, C_BD0);
        seg(cx + 3, cy - 1, cx + 8, cy + 3, 1.2f, C_BD0);
        if (hatch > 0.7f) {
            /* peeking eyes */
            disc(cx - 3.5f, cy - 4, 1.6f, C_EYE);
            disc(cx + 3.5f, cy - 4, 1.6f, C_EYE);
        }
    }
    /* soft rock animation */
    (void)t;
}

/* -------------------------------------------------------------- costumes */

static void draw_costume(uint8_t costume, const body_t *bd, float t, float dance)
{
    float top = bd->top;
    float cx = bd->cx;
    switch (costume) {
    case COSTUME_NIGHTCAP: {
        float tilt = sinf(t * 1.2f) * 2.0f;
        /* floppy cap */
        for (int i = 0; i <= 10; i++) {
            float k = (float)i / 10;
            float w = 9.0f * (1 - k * 0.75f);
            float x = cx - 4 + k * (10 + tilt) ;
            float y = top - 2 - k * 11;
            disc(x, y, w * 0.5f + 1.5f, C_ACC2);
        }
        disc(cx - 4 + 10 + tilt, top - 13, 2.6f, C_WHITE); /* pompom */
        seg(cx - 9, top + 1, cx + 9, top + 1, 3.0f, C_ACC); /* brim */
        break;
    }
    case COSTUME_PARTY: {
        float bob = dance > 0.05f ? sinf(t * 9.0f) * 1.5f : sinf(t * 2.0f) * 0.6f;
        for (int i = 0; i <= 8; i++) {
            float k = (float)i / 8;
            float w = 8.0f * (1 - k * 0.85f);
            float x = cx + k * 2.0f;
            float y = top - 1 - k * 13 + bob * k;
            disc(x, y, w * 0.5f + 1.0f, (i % 2) ? C_ACC : C_ACC2);
        }
        disc(cx + 2.0f, top - 14 + bob, 2.2f, C_HEART);
        break;
    }
    case COSTUME_HEADPHONES: {
        /* band */
        for (int a = 0; a <= 20; a++) {
            float ang = 3.14159f * (float)a / 20;
            float x = cx + cosf(ang) * (bd->a + 3.0f);
            float y = top + 4 - sinf(ang) * (bd->a + 6.0f);
            disc(x, y, 2.0f, C_OUT);
        }
        float pulse = 1.0f + dance * 0.25f * sinf(t * 10.0f);
        ell(cx - bd->a - 3.0f, top + 8, 3.4f * pulse, 4.6f * pulse, C_ACC);
        ell(cx + bd->a + 3.0f, top + 8, 3.4f * pulse, 4.6f * pulse, C_ACC);
        ell(cx - bd->a - 3.0f, top + 8, 1.6f, 2.2f, C_ACC2);
        ell(cx + bd->a + 3.0f, top + 8, 1.6f, 2.2f, C_ACC2);
        break;
    }
    case COSTUME_UMBRELLA: {
        /* mini umbrella hat */
        for (int i = -12; i <= 12; i++) {
            float x = cx + i;
            float y = top - 6 - (1 - abs(i) / 12.0f) * 7.0f;
            for (int yy = (int)y; yy < (int)y + 3; yy++) {
                px((int)x, yy, (i / 4 % 2 == 0) ? C_ACC : C_ACC2);
            }
        }
        disc(cx, top - 14, 1.6f, C_OUT);
        seg(cx, top - 6, cx, top - 1, 1.4f, C_OUT);
        /* rain drops */
        for (int i = 0; i < 5; i++) {
            float dx = sinf(i * 2.4f) * 20.0f;
            float dy = fracf(t * 0.9f + i * 0.23f) * 26.0f;
            px(iround(cx + dx), iround(top - 4 + dy), C_ACC2);
            px(iround(cx + dx), iround(top - 3 + dy), C_WHITE);
        }
        break;
    }
    case COSTUME_SCARF: {
        float ny = bd->fy + bd->fb + 2.0f;
        float sway = sinf(t * 2.2f) * 1.2f + dance * sinf(t * 8.0f) * 2.0f;
        for (int x = (int)(cx - bd->a); x <= (int)(cx + bd->a); x++) {
            for (int yy = (int)ny - 2; yy <= (int)ny + 2; yy++) {
                px(x, yy, ((x / 3) % 2 == 0) ? C_ACC : C_ACC2);
            }
        }
        seg(cx + bd->a * 0.5f, ny + 2, cx + bd->a * 0.5f + 3 + sway, ny + 11, 4.0f, C_ACC);
        seg(cx + bd->a * 0.5f, ny + 2, cx + bd->a * 0.5f + 3 + sway, ny + 11, 2.0f, C_ACC2);
        break;
    }
    case COSTUME_BANDAGE: {
        seg(bd->cx - 8, bd->fy - 6, bd->cx + 8, bd->fy + 2, 3.4f, C_WHITE);
        seg(bd->cx - 6, bd->cy + 6, bd->cx + 7, bd->cy + 10, 3.4f, C_WHITE);
        px(iround(bd->cx), iround(bd->fy - 2), C_HEART);
        break;
    }
    default:
        break;
    }
}

/* ------------------------------------------------------------ background */

static void draw_background(const chardef_t *ch, float cx, float cy, float t, float dance)
{
    (void)ch;
    /* soft glow */
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float dx = x + 0.5f - cx, dy = (y + 0.5f - cy) * 1.15f;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < 30 && bayer(x, y) < (30 - d) / 30.0f * 0.5f) {
                px(x, y, C_GLOW);
            }
        }
    }
    /* twinkles */
    int n = 6 + (int)(dance * 6);
    for (int i = 0; i < n; i++) {
        float a = t * (0.4f + dance * 1.6f) + i * TAU / n;
        float r = 24.0f + 3.0f * sinf(i * 1.7f + t * 0.8f);
        int x = iround(cx + cosf(a) * r);
        int y = iround(cy - 4 + sinf(a) * r * 0.75f);
        float tw = 0.5f + 0.5f * sinf(t * 4.0f + i * 2.1f);
        if (tw > 0.75f) {
            px(x, y, C_WHITE);
            px(x + 1, y, C_ACC);
            px(x - 1, y, C_ACC);
        } else if (tw > 0.4f) {
            px(x, y, C_ACC2);
        }
    }
    /* shadow */
    for (int x = (int)(cx - 13); x <= (int)(cx + 13); x++) {
        float e = fabsf(x + 0.5f - cx) / 13.0f;
        for (int r = 0; r < 2; r++) {
            if (bayer(x, 58 + r) > e * 0.85f) {
                px(x, 58 + r, C_SHADOW);
            }
        }
    }
}

static void draw_hearts(float cx, float top, float t, float amount)
{
    static const char *const HEART[] = { ".#.#.", "#o###", "#####", ".###.", "..#.." };
    for (int i = 0; i < 2; i++) {
        float ph = fracf(t * 0.9f + i * 0.5f);
        if (ph > amount) {
            continue;
        }
        int hx = iround(cx + (i ? 13 : -18) + sinf(ph * TAU + i) * 2);
        int hy = iround(top - ph * 10);
        stamp(HEART, 5, hx, hy, C_HEART, C_WHITE);
    }
}

static void draw_rings(float cx, float cy, float t, float level)
{
    for (int k = 0; k < 2; k++) {
        float ph = fracf(t * 0.9f + k * 0.5f);
        float r = 20 + ph * 10;
        float fade = (1 - ph) * (0.4f + level * 0.6f);
        int n = (int)(r * 2.0f);
        for (int i = 0; i < n; i++) {
            float a = i * TAU / n;
            int x = iround(cx + cosf(a) * r);
            int y = iround(cy + sinf(a) * r * 0.9f);
            if (bayer(x, y) < fade) {
                px(x, y, C_ACC);
            }
        }
    }
}

static void draw_thought_dots(float x, float y, float t)
{
    int active = (int)(fracf(t * 1.6f) * 3.0f);
    for (int i = 0; i < 3; i++) {
        uint8_t c = i == active ? C_WHITE : C_ACC;
        disc(x + i * 4, y - i * 2 - (i == active ? 1 : 0), 1.4f, c);
    }
}

/* ------------------------------------------------------------------ frame */

void muse_pixel_render(const muse_pose_t *p)
{
    uint8_t ch_id = p->character < PERSONA_COUNT ? p->character : 0;
    const chardef_t *ch = &CHARS[ch_id];
    uint8_t growth = p->growth < GROWTH_COUNT ? p->growth : GROWTH_ADULT;
    float t = p->t;
    float dance = clampf(p->dance, 0, 1);
    int mood = p->mood < -2 ? -2 : (p->mood > 2 ? 2 : p->mood);

    float dt = s_eyes.last_t > 0 ? clampf(t - s_eyes.last_t, 0, 0.2f) : 0.04f;
    s_eyes.last_t = t;

    load_palette(ch);
    memset(s_fb, C_BG, sizeof(s_fb));
    memset(s_mask, 0, sizeof(s_mask));

    muse_mode_t mode = p->mode;
    float level = clampf(p->level, 0, 1);
    float happy = clampf(p->happy, 0, 1);

    /* ---- motion ---- */
    float dance_ph = t * (6.0f + 5.0f * dance);
    float hop = dance > 0.03f ? fabsf(sinf(dance_ph)) * 7.0f * dance : 0;
    float sway = dance > 0.03f ? sinf(dance_ph * 0.5f) * 7.0f * dance : 0;
    float lean = dance > 0.03f ? sinf(dance_ph * 0.5f + 1.0f) * 0.18f * dance : 0;
    float squash = dance > 0.03f ? 1.0f + 0.12f * sinf(dance_ph * 2.0f) * dance : 1.0f;

    float bob = sinf(t * 1.9f) * ch->bounce;
    float breathe = sinf(t * 2.1f + 1.0f) * 0.03f;
    if (mode == MUSE_MODE_LISTENING) {
        bob = sinf(t * 3.0f) * 0.7f;
    } else if (mode == MUSE_MODE_SPEAKING) {
        bob = sinf(t * 5.0f) * 0.7f - level * 1.6f;
    } else if (mode == MUSE_MODE_THINKING) {
        bob = sinf(t * 2.4f) * 0.9f;
        lean += sinf(t * 1.3f) * 0.06f;
    } else if (mode == MUSE_MODE_ERROR) {
        bob = 1.2f;
        lean += sinf(t * 16.0f) * 0.05f * (p->mode_t < 0.7f ? 1 : 0);
    }
    if (happy > 0) {
        hop += fabsf(sinf(t * 9.0f)) * 3.0f * happy;
    }

    /* growth scale */
    float gs = growth == GROWTH_BABY ? 0.72f : growth == GROWTH_CHILD ? 0.88f : 1.0f;

    /* boot: rise from below */
    float boot = mode == MUSE_MODE_BOOT ? clampf(p->mode_t / 1.2f, 0, 1) : 1.0f;

    body_t bd;
    float base_a = (ch->shape == 1 ? 14.0f : ch->shape == 2 ? 18.0f : 16.0f) * gs;
    float base_b = (ch->shape == 1 ? 25.0f : ch->shape == 2 ? 20.0f : 23.0f) * gs;
    bd.a = base_a * (1 + breathe) / squash;
    bd.b = base_b * (1 - breathe) * squash;
    bd.cx = 32.0f + sway + lean * 10.0f;
    bd.cy = (58.0f - bd.b) * boot + (64.0f + bd.b) * (1 - boot) + bob * 0.5f - hop;
    bd.fa = bd.a * 0.62f;
    bd.fb = 7.2f * gs;
    bd.fx = bd.cx + lean * 6.0f;
    bd.fy = bd.cy - bd.b * 0.28f + bob * 0.3f;
    bd.top = bd.cy - bd.b;

    /* ---- background ---- */
    draw_background(ch, bd.cx, bd.cy - 2, t, dance);

    if (growth == GROWTH_EGG) {
        /* egg wobble driven by time; cracks hint at the coming hatch */
        draw_egg(bd.cx, 40.0f + sinf(t * 2.0f) * 1.0f, t, 0.55f);
        draw_sparkles_lite(bd.cx, 40, t);
        return;
    }

    /* ---- body & limbs ---- */
    draw_body(ch, &bd, 0);
    draw_ears(ch, &bd, t, dance);
    draw_limbs(ch, &bd, t, dance, happy, mode);

    /* ---- face ---- */
    float blink = eyes_update(ch, t, dt, p->gaze_x, p->gaze_y, mode, p->mode_t);
    float open = 1.0f - blink;
    if (mode == MUSE_MODE_BOOT) {
        open = p->mode_t < 0.9f ? 0.0f : clampf((p->mode_t - 0.9f) / 0.3f, 0, 1);
    } else if (mode == MUSE_MODE_OFF) {
        open = clampf(1.2f - p->mode_t, 0, 1);
    }

    eye_style_t estyle = E_NORMAL;
    mouth_t mouth = MO_SMILE;
    float mouth_open = 0;
    switch (mode) {
    case MUSE_MODE_LISTENING:
        mouth = MO_O;
        break;
    case MUSE_MODE_THINKING:
        open *= 0.85f;
        mouth = MO_HMM;
        break;
    case MUSE_MODE_SPEAKING:
        mouth = MO_TALK;
        mouth_open = level * 1.2f + 0.08f;
        break;
    case MUSE_MODE_ERROR:
        estyle = E_X;
        mouth = MO_FLAT;
        break;
    case MUSE_MODE_OFF:
        estyle = E_SLEEPY;
        mouth = MO_SMILE;
        break;
    default:
        break;
    }
    /* mood shaping */
    if (mood <= -2) {
        mouth = MO_SAD;
    } else if (mood == -1 && mouth == MO_SMILE) {
        mouth = MO_FLAT;
    } else if (mood >= 2 && mouth == MO_SMILE) {
        mouth = MO_GRIN;
    }
    if ((happy > 0.25f || dance > 0.5f) && mode != MUSE_MODE_ERROR) {
        estyle = E_HAPPY;
        mouth = MO_GRIN;
    }
    /* sleepy when energy is gone (mood -2 + off handled by pet via costume) */

    float eye_y = bd.fy - 0.5f;
    float eye_dx = bd.fa * 0.46f;
    float escale = growth == GROWTH_BABY ? 1.25f : 1.0f;
    draw_eye(ch, bd.fx - eye_dx, eye_y, open, estyle, s_eyes.gx, s_eyes.gy, escale * gs);
    draw_eye(ch, bd.fx + eye_dx, eye_y, open, estyle, s_eyes.gx, s_eyes.gy, escale * gs);

    /* brows for expressive states */
    int bl = iround(bd.fx - eye_dx), br = iround(bd.fx + eye_dx), by = iround(eye_y) - 4;
    if (mode == MUSE_MODE_THINKING || mood < 0) {
        px(bl - 1, by + 1, C_BROW); px(bl, by + 1, C_BROW);
        px(br + 1, by + 1, C_BROW); px(br, by + 1, C_BROW);
    } else if (mood >= 2) {
        px(bl - 1, by - 1, C_BROW); px(bl, by - 1, C_BROW);
        px(br + 1, by - 1, C_BROW); px(br, by - 1, C_BROW);
    }

    float blush = 0.45f + happy * 0.4f + (mood >= 1 ? 0.2f : 0) + dance * 0.25f;
    if (mood <= -2) {
        blush = 0.1f;
    }
    draw_blush(bd.fx - bd.fa * 0.7f, eye_y + 2.5f, blush, gs);
    draw_blush(bd.fx + bd.fa * 0.7f, eye_y + 2.5f, blush, gs);
    draw_mouth(bd.fx, eye_y + 3.5f, mouth, mouth_open, gs);

    /* ---- costume ---- */
    if (p->costume != COSTUME_NONE && p->costume < COSTUME_COUNT) {
        draw_costume(p->costume, &bd, t, dance);
    }

    /* ---- foreground ---- */
    if (mode == MUSE_MODE_LISTENING) {
        draw_rings(bd.cx, bd.fy + 2, t, level);
    } else if (mode == MUSE_MODE_SPEAKING) {
        draw_rings(bd.cx, bd.fy + 2, t, level * 0.7f);
    }
    if (mode == MUSE_MODE_THINKING) {
        draw_thought_dots(bd.cx + 13, bd.top + 3, t);
    }
    if (happy > 0) {
        draw_hearts(bd.cx, bd.top + 1, t, happy);
    }
    if (mode == MUSE_MODE_ERROR) {
        static const char *const BANG[] = { ".##.", ".##.", ".##.", "....", ".##." };
        stamp(BANG, 5, iround(bd.cx) + 17, iround(bd.top) - 2, C_ACC, C_ACC);
    }
}

/* small sparkles for the egg stage */
static void draw_sparkles_lite(float cx, float cy, float t)
{
    for (int i = 0; i < 5; i++) {
        float a = t * 0.7f + i * TAU / 5;
        int x = iround(cx + cosf(a) * 20.0f);
        int y = iround(cy + sinf(a) * 15.0f);
        if (0.5f + 0.5f * sinf(t * 3.0f + i * 1.3f) > 0.6f) {
            px(x, y, C_ACC2);
        }
    }
}

/* ------------------------------------------------------------- scale api */

#define MAP_MAX 512
static uint8_t s_map[MAP_MAX];
static int s_size;

void muse_pixel_set_size(int px)
{
    s_size = px < MAP_MAX ? px : MAP_MAX;
    for (int i = 0; i < s_size; i++) {
        s_map[i] = (uint8_t)(i * W / s_size);
    }
}

void muse_pixel_scale(uint16_t *dst, int stride_px, int x0, int x1, int y0, int y1)
{
    int n = x1 - x0 + 1;
    const uint8_t *xmap = &s_map[x0];
    for (int y = y0; y <= y1; y++, dst += stride_px) {
        uint8_t m = s_map[y];
        const uint8_t *row = &s_fb[m * W];
        for (int i = 0; i < n; i++) {
            dst[i] = s_pal[row[xmap[i]]];
        }
    }
}
