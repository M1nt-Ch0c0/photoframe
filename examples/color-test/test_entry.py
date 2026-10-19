"""Execute the actual autonomous ABI 2 color app, including both palettes."""
import ctypes
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parent
SDK=ROOT.parents[1]/"components/app_sdk/include"
MOCK=r'''
#include "photopainter_app.h"
#include <assert.h>
#include <string.h>
static app_context_v2_t c;
static int renders,result,reported,flags,reports;
static uint8_t captured[APP_FRAME_BYTES];
const app_context_v2_t *app_host_context_v2(void){return &c;}
void app_host_complete_v2(const app_result_v2_t *r){assert(r->event_id==c.event_id&&r->generation==c.generation);reports++;reported=r->status;flags=r->flags;}
int32_t app_host_display_v2(const uint8_t *p,uint32_t n){assert(n==APP_FRAME_BYTES);memcpy(captured,p,n);renders++;return result;}
void reset(int event,int r){c=(app_context_v2_t){.event=event,.event_id=123,.generation=7};renders=reports=reported=flags=0;result=r;}
int render_calls(void){return renders;}
int report_count(void){return reports;}
int reported_result(void){return reported;}
int ready(void){return flags;}
int pixel(int x,int y){return captured[y*800+x];}
'''
class ColorTestEntryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();mock=Path(cls.tmp.name)/"mock.c";mock.write_text(MOCK);cls.libs=[]
        for reverse in (0,1):
            out=Path(cls.tmp.name)/f"entry-{reverse}.dylib"
            subprocess.run(["cc","-shared","-fPIC","-Wall","-Wextra","-Werror","-Dmain=color_test_entry",
                f"-DCOLOR_TEST_REVERSE={reverse}","-I"+str(SDK),str(ROOT/"main/entry.c"),str(mock),"-o",str(out)],check=True)
            cls.libs.append(ctypes.CDLL(str(out)))
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def test_start_generates_complete_logical_chart_without_input(self):
        for reverse,lib in enumerate(self.libs):
            lib.reset(1,0);self.assertEqual(lib.color_test_entry(0,None),0)
            self.assertEqual((lib.render_calls(),lib.report_count(),lib.ready()),(1,1,1))
            for y in (0,239,479):
                for x in range(800):
                    color=x*6//800;self.assertEqual(lib.pixel(x,y),5-color if reverse else color)
    def test_input_is_unsupported_and_stop_never_displays(self):
        for lib in self.libs:
            for event,result in ((2,-9),(3,0),(4,0),(5,0)):
                lib.reset(event,0);self.assertEqual(lib.color_test_entry(0,None),result)
                self.assertEqual((lib.render_calls(),lib.report_count(),lib.ready()),(0,1,0))
    def test_display_failure_never_reports_ready(self):
        for lib in self.libs:
            lib.reset(1,-7);self.assertEqual(lib.color_test_entry(0,None),-7)
            self.assertEqual((lib.reported_result(),lib.ready()),(-7,0))
