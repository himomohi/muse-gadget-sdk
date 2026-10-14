"""Exercise Korean format strings, English fallback, and UTF-8 truncation."""
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class KoreanLocaleTest(unittest.TestCase):
    def test_catalog_matches_c_and_preserves_format_arguments(self):
        source = (ROOT / 'components/muse/muse_locale.c').read_text()
        catalog = json.loads((ROOT / 'components/muse/translations/ko.json').read_text())
        for en, ko in catalog.items():
            self.assertIn('{ ' + json.dumps(en, ensure_ascii=False) + ', ' +
                          json.dumps(ko, ensure_ascii=False) + ' },', source)

    def test_runtime_formats_and_fallback(self):
        code = r'''
#include <assert.h>
#include <string.h>
#include "muse_locale.h"
int main(void) {
    char buf[80];
#if CONFIG_MUSE_LANGUAGE_KOREAN
    assert(!strcmp(muse_tr("SETTINGS"), "설정"));
    assert(!strcmp(muse_tr("SLEEP"), "재우기"));
    assert(!strcmp(muse_tr("Pairing code"), "연결 코드"));
    muse_locale_snprintf(buf, sizeof(buf), "CHARGING %d%%", 73);
    assert(!strcmp(buf, "충전 중 73%"));
    for (size_t n = 1; n < 10; ++n) {
        muse_locale_snprintf(buf, n, "SETTINGS");
        size_t size = strlen(buf);
        assert(size == 0 || size == 3 || size == 6);
    }
#else
    assert(!strcmp(muse_tr("SETTINGS"), "SETTINGS"));
    muse_locale_snprintf(buf, sizeof(buf), "CHARGING %d%%", 73);
    assert(!strcmp(buf, "CHARGING 73%"));
#endif
    assert(muse_tr(NULL) == NULL);
    assert(!strcmp(muse_tr("MuseGadget-ABC123"), "MuseGadget-ABC123"));
    assert(!strcmp(muse_tr("wifi.connect"), "wifi.connect"));
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'test.c'
            path.write_text(code)
            for korean in (0, 1):
                exe = Path(tmp) / f'locale-{korean}'
                subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                                f'-DCONFIG_MUSE_LANGUAGE_KOREAN={korean}',
                                '-I', str(ROOT / 'components/muse'), str(path),
                                str(ROOT / 'components/muse/muse_locale.c'), '-o', str(exe)],
                               check=True, capture_output=True, text=True)
                subprocess.run([str(exe)], check=True)
