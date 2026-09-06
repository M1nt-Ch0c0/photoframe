"""Verify the independent app entry's input gate, output and result contract."""
import ctypes
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent
COMPONENT = ROOT.parents[1] / 'components/photoframe'
MOCK = r'''
#include <stdlib.h>
#include <stdint.h>
static int decoded, rendered, report, decode_result, render_result;
static unsigned char captured[192000];
const uint8_t *photoframe_host_png_data(void) { return (const uint8_t *)"fixture"; }
size_t photoframe_host_png_size(void) { return 7; }
void photoframe_host_report_result(int r) { report = r; }
int photoframe_decode_png(const uint8_t *p, size_t n, uint8_t **out) {
    if (!p || n != 7) abort();
    ++decoded; *out = decode_result ? NULL : malloc(192000); return decode_result;
}
int photoframe_e6_render(const uint8_t *p, size_t n) {
    if (n != sizeof(captured)) abort();
    for (size_t i=0; i<n; ++i) captured[i]=p[i];
    ++rendered; return render_result;
}
void reset(int d, int r) { decoded = rendered = report = 0; decode_result=d; render_result=r; }
int render_calls(void) { return rendered; }
int reported(void) { return report; }
int pixel(int x, int y) {
    unsigned char b=captured[192000-1-(y*800+x)/2];
    return x%2 ? b>>4 : b&15;
}
'''


class ColorTestEntryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        mock = Path(cls.tmp.name)/'mock.c'; mock.write_text(MOCK)
        cls.libs = []
        for reverse in (0, 1):
            out = Path(cls.tmp.name)/f'entry-{reverse}.dylib'
            subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror',
                '-Dmain=color_test_entry', f'-DCOLOR_TEST_REVERSE={reverse}',
                '-I'+str(COMPONENT/'include'), '-I'+str(COMPONENT/'private_include'),
                str(ROOT/'main/entry.c'),str(mock),'-o',str(out)], check=True)
            cls.libs.append(ctypes.CDLL(str(out)))

    @classmethod
    def tearDownClass(cls): cls.tmp.cleanup()

    def test_rejects_input_before_any_display_io(self):
        for lib in self.libs:
            for error in (-1,-2,-3,-4,-5):
                lib.reset(error,0)
                self.assertEqual(lib.color_test_entry(0,None),error)
                self.assertEqual(lib.render_calls(),0)
                self.assertEqual(lib.reported(),error)

    def test_distinct_chart_versions_and_correct_180_degree_packing(self):
        palette=[0,1,2,3,5,6]
        for reverse,lib in enumerate(self.libs):
            lib.reset(0,0)
            self.assertEqual(lib.color_test_entry(0,None),0)
            self.assertEqual(lib.render_calls(),1)
            for y in (0,239,479):
                for x in range(800):
                    band=x*6//800
                    self.assertEqual(lib.pixel(x,y),palette[5-band if reverse else band])

    def test_display_failure_is_reported_without_false_success(self):
        for lib in self.libs:
            lib.reset(0,-7)
            self.assertEqual(lib.color_test_entry(0,None),-7)
            self.assertEqual(lib.reported(),-7)
