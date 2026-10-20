"""Windows Credential Manager; never write API keys to config or repository."""
import ctypes
from ctypes import wintypes

TARGET = 'CodexDeepSeekBridge/API'

class Credential(ctypes.Structure):
    _fields_ = [('Flags', wintypes.DWORD), ('Type', wintypes.DWORD),
                ('TargetName', wintypes.LPWSTR), ('Comment', wintypes.LPWSTR),
                ('LastWritten', wintypes.FILETIME), ('CredentialBlobSize', wintypes.DWORD),
                ('CredentialBlob', ctypes.POINTER(ctypes.c_ubyte)),
                ('Persist', wintypes.DWORD), ('AttributeCount', wintypes.DWORD),
                ('Attributes', ctypes.c_void_p), ('TargetAlias', wintypes.LPWSTR),
                ('UserName', wintypes.LPWSTR)]

def write_key(key):
    api = ctypes.WinDLL('Advapi32.dll', use_last_error=True)
    api.CredWriteW.argtypes = [ctypes.POINTER(Credential), wintypes.DWORD]
    api.CredWriteW.restype = wintypes.BOOL
    raw = key.encode('utf-8')
    blob = (ctypes.c_ubyte * len(raw)).from_buffer_copy(raw)
    c = Credential(Type=1, TargetName=TARGET, Comment='DeepSeek API for local Codex',
                   CredentialBlobSize=len(raw), CredentialBlob=blob, Persist=2,
                   UserName='DeepSeek')
    if not api.CredWriteW(ctypes.byref(c), 0):
        raise ctypes.WinError(ctypes.get_last_error())

def read_key():
    api = ctypes.WinDLL('Advapi32.dll', use_last_error=True)
    api.CredReadW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                             ctypes.POINTER(ctypes.POINTER(Credential))]
    api.CredReadW.restype = wintypes.BOOL
    api.CredFree.argtypes = [ctypes.c_void_p]
    p = ctypes.POINTER(Credential)()
    if not api.CredReadW(TARGET, 1, 0, ctypes.byref(p)):
        raise RuntimeError('DeepSeek credential missing; run key-set.')
    try:
        return ctypes.string_at(p.contents.CredentialBlob,
                                p.contents.CredentialBlobSize).decode('utf-8')
    finally:
        api.CredFree(p)

def delete_key():
    api = ctypes.WinDLL('Advapi32.dll', use_last_error=True)
    api.CredDeleteW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD]
    api.CredDeleteW.restype = wintypes.BOOL
    if not api.CredDeleteW(TARGET, 1, 0) and ctypes.get_last_error() != 1168:
        raise ctypes.WinError(ctypes.get_last_error())
