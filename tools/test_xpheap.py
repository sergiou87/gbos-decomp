"""Tests for the Win32 XP heap interposer (xpheap.c)."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import build_wine
from rom import ROOT

MOCK_WINDOWS_H = """\
#ifndef MOCK_WINDOWS_H
#define MOCK_WINDOWS_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#undef memset
#undef memmove
#undef memcpy
void *memcpy(void *d, const void *s, size_t n);

#define WINAPI
#define __declspec(x)
#define TRUE 1
#define FALSE 0
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define STD_ERROR_HANDLE ((DWORD)-12)
#define MEM_RESERVE 0x2000
#define MEM_COMMIT 0x1000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 0x04
#define HEAP_ZERO_MEMORY 0x08
#define HEAP_REALLOC_IN_PLACE_ONLY 0x10
#define IMAGE_DIRECTORY_ENTRY_IMPORT 1
#define ERROR_ENVVAR_NOT_FOUND 203u

typedef int BOOL;
typedef uint32_t DWORD;
typedef size_t SIZE_T;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *HINSTANCE;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef void *LPPROCESS_HEAP_ENTRY;

typedef struct { uint32_t e_lfanew; } IMAGE_DOS_HEADER;
typedef struct { uint32_t VirtualAddress, Size; } IMAGE_DATA_DIRECTORY;
typedef struct { IMAGE_DATA_DIRECTORY DataDirectory[16]; } IMAGE_OPTIONAL_HEADER;
typedef struct { IMAGE_OPTIONAL_HEADER OptionalHeader; } IMAGE_NT_HEADERS;
typedef struct { uint32_t OriginalFirstThunk, TimeDateStamp, ForwarderChain, Name, FirstThunk; } IMAGE_IMPORT_DESCRIPTOR;

static DWORD mock_last_error;
static int mock_env_set;
static const char *mock_env_val = "";
static char mock_stderr_buf[1024];
static size_t mock_stderr_len;

static inline HANDLE GetStdHandle(DWORD n) { (void)n; return (HANDLE)1; }
static inline BOOL WriteFile(HANDLE h, const void *buf, DWORD n, DWORD *w, void *ov) {
    (void)h; (void)ov;
    if (mock_stderr_len + n < sizeof(mock_stderr_buf)) {
        memcpy(mock_stderr_buf + mock_stderr_len, buf, n);
        mock_stderr_len += n;
        mock_stderr_buf[mock_stderr_len] = 0;
    }
    if (w) *w = n;
    return TRUE;
}
static inline void ExitProcess(unsigned code) { exit((int)code); }
static inline HANDLE GetProcessHeap(void) { return (HANDLE)2; }
static inline LPVOID HeapAlloc(HANDLE h, DWORD f, SIZE_T n) { (void)h; (void)f; return calloc(1, n ? n : 1); }
static inline LPVOID HeapReAlloc(HANDLE h, DWORD f, LPVOID p, SIZE_T n) { (void)h; (void)f; return realloc(p, n ? n : 1); }
static inline LPVOID VirtualAlloc(LPVOID addr, SIZE_T sz, DWORD type, DWORD prot) {
    (void)type; (void)prot;
    if (addr) return addr;
    return calloc(1, sz);
}
static inline BOOL VirtualFree(LPVOID addr, SIZE_T sz, DWORD type) {
    (void)sz; (void)type;
    if (addr != (LPVOID)(uintptr_t)0x20000000u) free(addr);
    return TRUE;
}
static inline HMODULE GetModuleHandleA(const char *name) { (void)name; return NULL; }
static inline void *GetProcAddress(HMODULE m, const char *name) { (void)m; (void)name; return NULL; }
static inline BOOL VirtualProtect(LPVOID a, SIZE_T s, DWORD n, DWORD *o) {
    (void)a; (void)s; (void)n; if (o) *o = 0; return TRUE;
}
static inline void SetLastError(DWORD e) { mock_last_error = e; }
static inline DWORD GetLastError(void) { return mock_last_error; }
static inline DWORD GetEnvironmentVariableA(const char *name, char *buf, DWORD sz) {
    size_t len;
    (void)name;
    if (!mock_env_set) {
        mock_last_error = ERROR_ENVVAR_NOT_FOUND;
        return 0;
    }
    len = 0;
    while (mock_env_val[len]) len++;
    if (sz) buf[0] = 0;
    if (len + 1 <= sz && buf) memcpy(buf, mock_env_val, len + 1);
    return (DWORD)len;
}
#endif
"""

HARNESS_C = """\
#include <assert.h>
#include "xpheap.c"

static int contains(const char *haystack, const char *needle)
{
    size_t i, j;
    for (i = 0; haystack[i]; i++) {
        for (j = 0; needle[j] && haystack[i + j] == needle[j]; j++) ;
        if (!needle[j]) return 1;
    }
    return 0;
}

int main(void)
{
    HANDLE h = hCreate(0, 0, 0);
    LPVOID small_blk, big_blk;

    assert(h == EMU_HANDLE);
    assert(hValidate(h, 0, NULL) == TRUE);
    assert(hSize(h, 0, NULL) == (SIZE_T)-1);
    assert(hSize(h, 0, (LPCVOID)(uintptr_t)0x1234u) == (SIZE_T)-1);
    assert(hValidate(h, 0, (LPCVOID)(uintptr_t)0x1234u) == FALSE);
    /* Segment header chunk at EMU_BASE + 8 must not validate as a user block. */
    assert(hSize(h, 0, (LPCVOID)(uintptr_t)(EMU_BASE + UNIT)) == (SIZE_T)-1);
    assert(hValidate(h, 0, (LPCVOID)(uintptr_t)(EMU_BASE + UNIT)) == FALSE);

    small_blk = hAlloc(h, 0, 29);
    assert(small_blk != NULL);
    assert(hSize(h, 0, small_blk) == 29);
    assert(hValidate(h, 0, small_blk) == TRUE);

    /* Freeing a small block places it on a lookaside list while F_BUSY stays set in metadata. */
    assert(hFree(h, 0, small_blk) == TRUE);
    assert(hSize(h, 0, small_blk) == (SIZE_T)-1);
    assert(hValidate(h, 0, small_blk) == FALSE);
    assert(hValidate(h, 0, NULL) == TRUE);

    /* Allocate and free a large block to exercise UCR commit/decommit validation. */
    big_blk = hAlloc(h, 0, 0x20000);
    assert(hSize(h, 0, big_blk) == 0x20000);
    assert(hValidate(h, 0, big_blk) == TRUE);
    assert(hFree(h, 0, big_blk) == TRUE);
    assert(hSize(h, 0, big_blk) == (SIZE_T)-1);
    assert(hValidate(h, 0, NULL) == TRUE);

    /* Unset XPHEAP_REPORT stays silent; empty XPHEAP_REPORT="" still emits diagnostics. */
    mock_env_set = 0;
    mock_stderr_len = 0;
    /* Pretend stale non-203 last error before report(). */
    SetLastError(0);
    report();
    assert(mock_stderr_len == 0);

    mock_env_set = 1;
    mock_env_val = "";
    SetLastError(ERROR_ENVVAR_NOT_FOUND);
    report();
    assert(contains(mock_stderr_buf, "model served allocs"));
    return 0;
}
"""


class XpHeapTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which("cc"), "Host C compiler unavailable")
    def test_hsize_hvalidate_and_report_contracts(self):
        with tempfile.TemporaryDirectory() as temporary:
            tmp = Path(temporary)
            (tmp / "windows.h").write_text(MOCK_WINDOWS_H)
            (tmp / "harness.c").write_text(HARNESS_C)
            binary = tmp / "harness"
            subprocess.run(
                ["cc", "-std=c11", "-O2", "-Wall", "-I", str(tmp),
                 "-I", str(ROOT / "tools/xpheap"), str(tmp / "harness.c"), "-o", str(binary)],
                check=True,
            )
            subprocess.run([str(binary)], check=True)

    @unittest.skipUnless(shutil.which("i686-w64-mingw32-gcc"), "MinGW-w64 compiler unavailable")
    def test_freestanding_mingw_dll_builds_cleanly(self):
        with tempfile.TemporaryDirectory() as temporary:
            dll = Path(temporary) / "xpheap.dll"
            subprocess.run(
                ["i686-w64-mingw32-gcc", *build_wine.DLL_FLAGS, "-Werror",
                 "-I", str(ROOT / "tools/xpheap"),
                 "-o", str(dll), str(ROOT / "tools/xpheap/xpheap.c"), "-lkernel32", "-lgcc"],
                check=True,
            )
            self.assertTrue(dll.is_file())


if __name__ == "__main__":
    unittest.main()
