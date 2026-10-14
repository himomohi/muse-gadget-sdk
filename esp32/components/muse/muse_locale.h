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
#include <stdarg.h>
#include <stddef.h>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif
#if CONFIG_MUSE_LANGUAGE_KOREAN || defined(MUSE_LOCALE_IMPLEMENTATION)
const char *muse_tr(const char *text);
#else
static inline const char *muse_tr(const char *text) { return text; }
#endif
int muse_locale_vsnprintf(char *buf, size_t size, const char *format, va_list ap);
int muse_locale_snprintf(char *buf, size_t size, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
#ifdef __cplusplus
}
#endif
