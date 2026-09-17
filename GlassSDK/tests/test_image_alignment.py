"""Compile the real animation example and verify every borrowed frame address."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
SDK=Path(__file__).resolve().parents[1]
source=SDK/'examples/image_animation/image_animation.c'
program='#include "'+str(source)+'"\n'+r'''
#include <assert.h>
int main(void) {
    assert(IMAGE_DATA_SIZE == 2534U);
    assert(sizeof(s_frame_data[0]) == 2536U);
    for(unsigned i=0;i<FRAME_COUNT;++i) {
        assert(((uintptr_t)s_frame_data[i]&3U)==0U);
        decode_fighter_frame(s_frame_data[i],s_fighter_frame_indexes[i]);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='sdk-image-alignment-') as folder:
    src=Path(folder)/'test.c';binary=Path(folder)/'test';src.write_text(program)
    subprocess.run([os.environ.get('CC','cc'),'-std=gnu99',*shlex.split(os.environ.get('CFLAGS','')),
        '-I'+str(SDK/'include'),str(src),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('animation frame addresses aligned; storage stride2536 and exact payload2534 preserved')
