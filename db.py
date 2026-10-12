import ctypes as ct
import os

# Loading the compiled C shared library
base_dir = os.path.dirname(os.path.abspath(__file__))
lib_path = os.path.join(base_dir, "libstorage.so")
if not os.path.exists(lib_path):
    raise FileNotFoundError(f"Could not find {lib_path}. Compile your C code first!")

_lib = ct.CDLL(lib_path)

# Defining matching C structures in ctypes
class FreeSpaceMap(ct.Structure):
    _fields_ = [
        ("free_bytes", ct.c_uint16 * 1024),
        ("last_page_id", ct.c_int)
    ]

class IndexEntry(ct.Structure):
    _fields_ = [
        ("key", ct.c_char * 64),
        ("page_id", ct.c_int),
        ("slot_id", ct.c_int)
    ]

class IndexTable(ct.Structure):
    _fields_ = [
        ("entries", IndexEntry * 1000),
        ("count", ct.c_int)
    ]

# Binding C function signatures
_lib.init_fsm.argtypes = [ct.POINTER(FreeSpaceMap)]
_lib.init_index.argtypes = [ct.POINTER(IndexTable)]
_lib.recover_from_wal.argtypes = [ct.c_char_p, ct.c_char_p, ct.POINTER(FreeSpaceMap), ct.POINTER(IndexTable)]
_lib.recover_from_wal.restype = ct.c_int

_lib.db_insert.argtypes = [ct.c_char_p, ct.POINTER(FreeSpaceMap), ct.POINTER(IndexTable), ct.c_char_p, ct.c_char_p]
_lib.db_insert.restype = ct.c_int

_lib.db_delete.argtypes = [ct.c_char_p, ct.POINTER(FreeSpaceMap), ct.POINTER(IndexTable), ct.c_char_p]
_lib.db_delete.restype = ct.c_int

_lib.db_update.argtypes = [ct.c_char_p, ct.POINTER(FreeSpaceMap), ct.POINTER(IndexTable), ct.c_char_p, ct.c_char_p]
_lib.db_update.restype = ct.c_int

_lib.db_get.argtypes = [ct.c_char_p, ct.POINTER(IndexTable), ct.c_char_p, ct.c_char_p, ct.c_size_t]
_lib.db_get.restype = ct.c_int

# Python high-level class wrapper
class KeyValueStore:
    def __init__(self, db_filename = "database.db", wal_filename = "database.wal"):
        self.db_filename = db_filename.encode('utf-8')
        self.wal_filename = wal_filename.encode('utf-8')
        
        # Initializing FSM and index table in RAM
        self.fsm = FreeSpaceMap()
        self.index = IndexTable()
        
        _lib.init_fsm(ct.byref(self.fsm))
        _lib.init_index(ct.byref(self.index))
        
        # Performing crash recovery from WAL on startup
        _lib.recover_from_wal(self.wal_filename, self.db_filename, ct.byref(self.fsm), ct.byref(self.index))
    
    def put(self, key: str, value: str):
        res = _lib.db_insert(
            self.db_filename,
            ct.byref(self.fsm),
            ct.byref(self.index),
            key.encode('utf-8'),
            value.encode('utf-8')
        )
        
        if res:
            raise RuntimeError(f"Failed to insert key '{key}'")
    
    def get(self, key: str) -> str:
        buf_len = 65536 # 64KB
        buf = ct.create_string_buffer(buf_len)
        
        bytes_read = _lib.db_get(
            self.db_filename,
            ct.byref(self.index),
            key.encode('utf-8'),
            buf,
            buf_len
        )
        
        if bytes_read < 0:
            return None # Key not found
        
        return buf.value.decode('utf-8')
    
    def delete(self, key: str):
        res = _lib.db_delete(
            self.db_filename,
            ct.byref(self.fsm),
            ct.byref(self.index),
            key.encode('utf-8')
        )
        
        if res:
            raise KeyError(f"Key '{key}' not found for deletion")
    
    def update(self, key: str, new_value: str):
        res = _lib.db_update(
            self.db_filename,
            ct.byref(self.fsm),
            ct.byref(self.index),
            key.encode('utf-8'),
            new_value.encode('utf-8')
        )
        
        if res:
            raise KeyError(f"Key '{key}' not found for update")