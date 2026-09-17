"""Use actual firmware C to check every generated package, including byte splits."""
import ctypes as c
from pathlib import Path
import struct
import argparse
parser = argparse.ArgumentParser(description='Verify SDK packages against the firmware XIP C helper')
parser.add_argument('--library', required=True, help='Host shared library built from gm_plugin_xip.c')
parser.add_argument('--packages', required=True, type=Path)
args = parser.parse_args()
lib=c.CDLL(args.library)
class Header(c.Structure):
    _fields_=[('magic',c.c_ubyte*4),('format',c.c_uint16),('abi',c.c_uint16)]+[(x,c.c_uint32) for x in ('size','flash_size','init_size','ram_size','entry','slots','fixups','pointers','flash_offset','ram_offset','crc','version')]+[('name',c.c_char*64)]
class View(c.Structure):
    _fields_=[('package',c.c_void_p),('h',Header)]
lib.gm_xip_open_prefix.argtypes=[c.POINTER(View),c.c_void_p,c.c_uint32,c.c_uint32];lib.gm_xip_open_prefix.restype=c.c_bool
lib.gm_xip_transform.argtypes=[c.POINTER(View),c.c_uint32,c.c_void_p,c.c_uint32,c.c_uint32,c.c_bool];lib.gm_xip_transform.restype=c.c_bool
lib.gm_xip_verify.argtypes=[c.POINTER(View),c.c_uint32,c.c_bool];lib.gm_xip_verify.restype=c.c_bool
lib.gm_xip_load_ram.argtypes=[c.POINTER(View),c.c_void_p,c.c_uint32,c.c_uint32,c.c_void_p];lib.gm_xip_load_ram.restype=c.c_bool
for path in args.packages.rglob('*.gmp'):
    raw=path.read_bytes(); data=c.create_string_buffer(raw,len(raw));v=View();bind=0x20107fc
    assert lib.gm_xip_open_prefix(c.byref(v),data,len(raw),len(raw)),path
    assert lib.gm_xip_verify(c.byref(v),bind,False),path
    # Byte-at-a-time covers every split position of both instructions.
    for offset in range(v.h.flash_offset,v.h.ram_offset):
        assert lib.gm_xip_transform(c.byref(v),offset,c.byref(data,offset),1,bind,True)
    assert lib.gm_xip_verify(c.byref(v),bind,True),path
    if v.h.fixups:
        off=struct.unpack_from('<I',raw,120+v.h.slots*4)[0]+v.h.flash_offset
        original=data[off];data[off]=bytes([original[0]^1]);assert not lib.gm_xip_verify(c.byref(v),bind,True);data[off]=original
    ram=c.create_string_buffer(v.h.ram_size+16);c.memset(ram,0xcc,len(ram));slots=(c.c_uint32*128)()
    assert lib.gm_xip_load_ram(c.byref(v),ram,0x20001000,0x05510000,slots)
    assert ram.raw[v.h.ram_size:]==b'\xcc'*16
    assert ram.raw[v.h.init_size:v.h.ram_size]==bytes(v.h.ram_size-v.h.init_size)
    for index in range(v.h.slots):
        target=struct.unpack_from('<I',raw,120+index*4)[0]
        expected=(target&0x7fffffff)+(0x20001000 if target&0x80000000 else 0x05510000)
        assert slots[index]==expected,(path,'slot',index)
    for index in range(v.h.pointers):
        off,target=struct.unpack_from('<II',raw,120+v.h.slots*4+v.h.fixups*8+index*8)
        expected=(target&0x7fffffff)+(0x20001000 if target&0x80000000 else 0x05510000)
        assert struct.unpack_from('<I',ram.raw,off)[0]==expected,(path,'pointer',index)
    print(path.stem,v.h.flash_size,v.h.ram_size,'OK')
