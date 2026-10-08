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

#include "muse_pet.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#include "muse_persona.h"

static const char *TAG = "muse_pet";
static SemaphoreHandle_t s_lock;
static pet_state_t s_pet;
static bool s_ready;
static int64_t s_dance_until; /* esp_timer us */
static float s_gaze_x, s_gaze_y;
static int64_t s_gaze_at;

#define DANCE_SECS 12
#define GAZE_HOLD_SECS 3

static void pet_save_locked(void)
{
    nvs_handle_t h;
    if (nvs_open("muse_pet", NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    uint8_t blob[PET_BLOB_MAX];
    uint8_t len = pet_save(&s_pet, blob);
    if (nvs_set_blob(h, "state", blob, len) == ESP_OK) {
        nvs_commit(h);
    }
    nvs_close(h);
}

static void pet_load_locked(void)
{
    nvs_handle_t h;
    if (nvs_open("muse_pet", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    uint8_t blob[PET_BLOB_MAX];
    size_t len = sizeof(blob);
    if (nvs_get_blob(h, "state", blob, &len) == ESP_OK && pet_load(&s_pet, blob, (uint8_t)len) == 0) {
        ESP_LOGI(TAG, "loaded pet: ticks=%lu growth=%d char=%d", (unsigned long)s_pet.ticks,
                 s_pet.growth, s_pet.character);
    }
    nvs_close(h);
}

static void pet_task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        xSemaphoreTake(s_lock, portMAX_DELAY);
        uint8_t before = s_pet.growth;
        pet_tick(&s_pet, 1);
        if (s_pet.growth != before) {
            ESP_LOGI(TAG, "grew to stage %d", s_pet.growth);
        }
        pet_save_locked();
        xSemaphoreGive(s_lock);
    }
}

esp_err_t muse_pet_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    pet_init(&s_pet);
    pet_load_locked();
    pet_away(&s_pet); /* absence decay for the time the gadget was off */
    pet_save_locked();
    xSemaphoreGive(s_lock);
    s_ready = true;
    if (xTaskCreate(pet_task, "muse_pet", 3072, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void muse_pet_snapshot(pet_state_t *out)
{
    if (!s_ready) {
        pet_init(out);
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_pet;
    xSemaphoreGive(s_lock);
}

static void action(void (*fn)(pet_state_t *))
{
    if (!s_ready) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    fn(&s_pet);
    pet_save_locked();
    xSemaphoreGive(s_lock);
}

void muse_pet_feed(void) { action(pet_feed); }
void muse_pet_clean(void) { action(pet_clean); }
void muse_pet_touch(void) { action(pet_pet); }
void muse_pet_toggle_sleep(void) { action(pet_toggle_sleep); }

void muse_pet_play(void)
{
    action(pet_play);
    s_dance_until = esp_timer_get_time() + (int64_t)DANCE_SECS * 1000000;
}

void muse_pet_set_character(uint8_t id)
{
    if (!s_ready || id >= persona_character_count()) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_pet.character = id;
    pet_save_locked();
    xSemaphoreGive(s_lock);
}

float muse_pet_dance(void)
{
    int64_t left = s_dance_until - esp_timer_get_time();
    if (left <= 0) {
        return 0;
    }
    float f = (float)left / ((float)DANCE_SECS * 1000000.0f);
    return f > 1 ? 1 : f;
}

void muse_pet_gaze(float x, float y)
{
    s_gaze_x = x < -1 ? -1 : (x > 1 ? 1 : x);
    s_gaze_y = y < -1 ? -1 : (y > 1 ? 1 : y);
    s_gaze_at = esp_timer_get_time();
}

void muse_pet_get_gaze(float *x, float *y)
{
    int64_t age = esp_timer_get_time() - s_gaze_at;
    if (age > (int64_t)GAZE_HOLD_SECS * 1000000) {
        *x = 0;
        *y = 0;
        return;
    }
    float k = 1.0f - (float)age / ((float)GAZE_HOLD_SECS * 1000000.0f);
    *x = s_gaze_x * k;
    *y = s_gaze_y * k;
}
