/* Korean glyph coverage regression: use LVGL's real fallback lookup. */
#include <assert.h>
#include <string.h>
#include "lvgl.h"
#include "muse_locale_ui.h"

int main(void)
{
    lv_init();
    const lv_font_t *fonts[] = {
        muse_locale_font(&lv_font_unscii_8),
        muse_locale_font(&lv_font_unscii_16),
        muse_locale_font(&lv_font_montserrat_20),
    };
    for (unsigned i = 0; i < sizeof(fonts) / sizeof(fonts[0]); ++i) {
        for (uint32_t cp = 0xac00; cp <= 0xd7a3; ++cp) {
            lv_font_glyph_dsc_t glyph;
            assert(lv_font_get_glyph_dsc(fonts[i], &glyph, cp, 0));
            assert(!glyph.is_placeholder && glyph.resolved_font && glyph.box_w);
            assert(glyph.box_h <= (uint32_t)fonts[i]->line_height);
        }
        lv_font_glyph_dsc_t symbol;
        assert(lv_font_get_glyph_dsc(fonts[i], &symbol, 'A', 0));
        assert(muse_locale_font(fonts[i]) == fonts[i]);
    }
    assert(!strcmp(muse_tr("READY"), "준비"));
    lv_deinit();
    return 0;
}
