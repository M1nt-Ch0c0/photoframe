"""Application restoration/first-screen policy with SDK services mocked."""
import ctypes
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
MOCK=r'''
#include "photopainter_app.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static app_context_v2_t c;
static uint8_t cached[APP_FRAME_BYTES];
static int has_cache,decode_result,display_result,cache_result,renders,puts,report,flags,black;
const app_context_v2_t *app_host_context_v2(void){return &c;}
void app_host_complete_v2(const app_result_v2_t *r){assert(r->event_id==99&&r->generation==5);report=r->status;flags=r->flags;}
int32_t app_host_display_v2(const uint8_t *p,uint32_t n){assert(n==APP_FRAME_BYTES);renders++;black=0;for(unsigned i=0;i<n;i++)black+=p[i]==APP_BLACK;return display_result;}
int32_t app_host_cache_put_v2(uint32_t v,const uint8_t *p,uint32_t n){assert(v==1&&n==APP_FRAME_BYTES);(void)p;puts++;return cache_result;}
const uint8_t *app_host_cache_get_v2(uint32_t v,uint32_t *n){assert(v==1);*n=has_cache?APP_FRAME_BYTES:0;return has_cache?cached:NULL;}
int photoframe_decode_indexed(const uint8_t *p,size_t n,uint8_t **out){assert(p&&n==3);*out=NULL;if(decode_result)return decode_result;*out=calloc(1,APP_FRAME_BYTES);return 0;}
void reset(int event,int cache,int decode,int display,int store){c=(app_context_v2_t){.event=event,.event_id=99,.generation=5,.input_type=1,.data=(const uint8_t *)"png",.data_size=3};has_cache=cache;decode_result=decode;display_result=display;cache_result=store;renders=puts=flags=black=0;report=999;}
int metric(int i){return i==0?renders:i==1?puts:i==2?report:i==3?flags:black;}
'''
class PhotoFrameEntryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();mock=Path(cls.tmp.name)/"mock.c";mock.write_text(MOCK);out=Path(cls.tmp.name)/"app.dylib"
        subprocess.run(["cc","-shared","-fPIC","-Wall","-Wextra","-Werror","-Dmain=frame_entry",
            "-DPHOTOFRAME_APP_ENTRY=1","-DPHOTOFRAME_ABI_2=1","-I"+str(ROOT/"components/app_sdk/include"),
            "-I"+str(ROOT/"components/photoframe/include"),str(ROOT/"main/entry.c"),str(mock),"-o",str(out)],check=True)
        cls.lib=ctypes.CDLL(str(out))
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def test_missing_cache_draws_waiting_page_and_existing_cache_restores(self):
        for cached in (0,1):
            self.lib.reset(1,cached,0,0,0);self.assertEqual(self.lib.frame_entry(0,None),0)
            self.assertEqual([self.lib.metric(i) for i in range(4)],[1,0,0,1])
            black=self.lib.metric(4)
            if cached:self.assertEqual(black,384000)
            else:self.assertTrue(0<black<384000)
    def test_input_validation_and_display_failure_never_overwrite_cache(self):
        for decode,display,renders in ((-2,0,0),(0,-7,1)):
            self.lib.reset(2,0,decode,display,0);self.assertEqual(self.lib.frame_entry(0,None),decode or display)
            self.assertEqual(self.lib.metric(0),renders);self.assertEqual(self.lib.metric(1),0)
    def test_optional_cache_failure_does_not_turn_success_into_retry(self):
        self.lib.reset(2,0,0,0,-5);self.assertEqual(self.lib.frame_entry(0,None),0)
        self.assertEqual([self.lib.metric(i) for i in range(4)],[1,1,0,0])
    def test_stop_does_not_draw(self):
        self.lib.reset(5,0,0,0,0);self.assertEqual(self.lib.frame_entry(0,None),0)
        self.assertEqual([self.lib.metric(i) for i in range(4)],[0,0,0,0])
