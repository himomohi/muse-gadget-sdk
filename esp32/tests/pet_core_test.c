/* Host test for the pet engine core (muse_pet_core.c). Pure C99, no IDF. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "muse_pet_core.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void test_init(void)
{
    pet_state_t s;
    pet_init(&s);
    CHECK(s.growth == 0); /* egg */
    CHECK(s.satiety == 70 && s.happiness == 70);
    CHECK(s.ticks == 0 && !s.sleeping && !s.sick);
}

static void test_tick_decay(void)
{
    pet_state_t s;
    pet_init(&s);
    pet_tick(&s, 10); /* hatch */
    CHECK(s.growth == 1);
    uint8_t sat = s.satiety;
    pet_tick(&s, 1);
    CHECK(s.satiety == sat - 2);
    CHECK(s.ticks == 11);
}

static void test_growth(void)
{
    pet_state_t s;
    pet_init(&s);
    pet_tick(&s, 9);
    CHECK(s.growth == 0);
    pet_tick(&s, 1);
    CHECK(s.growth == 1);
    pet_tick(&s, 110);
    CHECK(s.growth == 2);
    pet_tick(&s, 360);
    CHECK(s.growth == 3);
}

static void test_actions(void)
{
    pet_state_t s;
    pet_init(&s);
    pet_tick(&s, 10);
    s.satiety = 50;
    pet_feed(&s);
    CHECK(s.satiety == 75);
    /* cooldown blocks immediate re-feed */
    pet_feed(&s);
    CHECK(s.satiety == 75);
    pet_tick(&s, 5);
    uint8_t h = s.happiness;
    pet_play(&s);
    CHECK(s.happiness == (h > 80 ? 100 : h + 20));
    pet_clean(&s);
    CHECK(s.cleanliness == 100);
    uint8_t a = s.affection;
    pet_pet(&s);
    CHECK(s.affection == (a > 92 ? 100 : a + 8));
    /* egg ignores actions */
    pet_state_t e;
    pet_init(&e);
    pet_feed(&e);
    CHECK(e.satiety == 70);
}

static void test_sleep(void)
{
    pet_state_t s;
    pet_init(&s);
    pet_tick(&s, 10);
    s.energy = 50;
    pet_toggle_sleep(&s);
    CHECK(s.sleeping == 1);
    pet_tick(&s, 1);
    CHECK(s.energy == 58);
    CHECK(pet_costume(&s) == 1); /* nightcap */
    pet_toggle_sleep(&s);
    CHECK(s.sleeping == 0);
}

static void test_sick(void)
{
    pet_state_t s;
    pet_init(&s);
    pet_tick(&s, 10);
    s.satiety = 0;
    pet_tick(&s, 30);
    CHECK(s.sick == 1);
    CHECK(pet_mood(&s) == -2);
    CHECK(pet_costume(&s) == 6); /* bandage */
    /* nursing back: feed + wash + cheer up */
    s.satiety = 80;
    s.cleanliness = 80;
    s.happiness = 80;
    pet_tick(&s, 1);
    CHECK(s.sick == 0);
}

static void test_mood(void)
{
    pet_state_t s;
    pet_init(&s);
    pet_tick(&s, 10);
    s.satiety = s.happiness = s.cleanliness = s.energy = s.affection = 100;
    CHECK(pet_mood(&s) == 2);
    s.satiety = s.happiness = s.cleanliness = s.energy = s.affection = 10;
    CHECK(pet_mood(&s) == -2);
}

static void test_save_load(void)
{
    pet_state_t s, r;
    pet_init(&s);
    pet_tick(&s, 123);
    s.character = 2;
    uint8_t blob[PET_BLOB_MAX];
    uint8_t len = pet_save(&s, blob);
    CHECK(len == 17);
    memset(&r, 0, sizeof(r));
    CHECK(pet_load(&r, blob, len) == 0);
    CHECK(r.ticks == 123 && r.character == 2 && r.growth == s.growth);
    CHECK(r.satiety == s.satiety && r.affection == s.affection);
    /* corrupt */
    blob[0] = 9;
    CHECK(pet_load(&r, blob, len) != 0);
    CHECK(pet_load(&r, blob, 3) != 0);
}

static void test_away(void)
{
    pet_state_t s;
    pet_init(&s);
    pet_away(&s);
    CHECK(s.ticks == 60);
    CHECK(s.growth == 1); /* hatched while away */
}

int main(void)
{
    test_init();
    test_tick_decay();
    test_growth();
    test_actions();
    test_sleep();
    test_sick();
    test_mood();
    test_save_load();
    test_away();
    if (failures == 0) {
        printf("pet core: all tests passed\n");
    }
    return failures != 0;
}
