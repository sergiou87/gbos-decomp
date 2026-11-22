/*
 * xpheap.dll -- make the original win32 SDCC 3.3.0 compiler allocate its CRT heap from a
 * deterministic model of the Windows XP SP3 heap (tools/xpheap/xpheap_engine.h).
 *
 * SDCC lays out a function's stack slots in the address order of its symbol structures
 * (std::set<symbol *> in SDCCbtree.cc), so the layout depends on the host heap. The original
 * build ran on an XP-style heap; Wine's heap behaves differently. A patched copy of sdcc.exe
 * imports this DLL first (tools/xpheap/patch_imports.py), so it initialises before msvcrt and
 * redirects msvcrt's kernel32 heap imports: the first heap msvcrt creates (its CRT heap) is
 * served by the model at a fixed address (EMU_BASE, segments back to back at XP's growing
 * reserves); every other heap passes through. The model never touches user memory; this DLL
 * copies and zeroes blocks as HeapReAlloc/HEAP_ZERO_MEMORY require. Failures end the process
 * with exit code 70 and a message on stderr.
 *
 * Build (i686 MinGW, no CRT):
 *   i686-w64-mingw32-gcc -O2 -Wall -shared -ffreestanding -fno-builtin
 *     -fno-tree-loop-distribute-patterns -fno-stack-protector -mno-stack-arg-probe -nostdlib
 *     -Wl,-e,_DllMain@12 -Wl,--image-base,0x50000000 -o xpheap.dll xpheap.c -lkernel32 -lgcc
 */
#include <stdint.h>
#include <stddef.h>
#include <windows.h>

void *memset(void *d, int c, size_t n)
{
    volatile unsigned char *p = d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    volatile unsigned char *dp = d;
    const unsigned char *sp = s;
    if (dp < sp)
        while (n--) *dp++ = *sp++;
    else
        while (n--) dp[n] = sp[n];
    return d;
}

void *memcpy(void *d, const void *s, size_t n) { return memmove(d, s, n); }

static void message(const char *a, const char *b)
{
    HANDLE e = GetStdHandle(STD_ERROR_HANDLE);
    DWORD w, n;
    for (n = 0; a[n]; n++) ;
    WriteFile(e, "xpheap: ", 8, &w, NULL);
    WriteFile(e, a, n, &w, NULL);
    if (b) {
        for (n = 0; b[n]; n++) ;
        WriteFile(e, " ", 1, &w, NULL);
        WriteFile(e, b, n, &w, NULL);
    }
    WriteFile(e, "\r\n", 2, &w, NULL);
}

static void abort_process(const char *what, const char *detail)
{
    message(what, detail);
    ExitProcess(70);
}

static void fail(const char *fmt, ...) __attribute__((noreturn));
static void fail(const char *fmt, ...)
{
    abort_process("model exception:", fmt);
    for (;;) ;
}

static void die(const char *fmt, ...) __attribute__((noreturn));
static void die(const char *fmt, ...)
{
    abort_process("fatal:", fmt);
    for (;;) ;
}

static void *engine_realloc(void *p, size_t n)
{
    HANDLE ph = GetProcessHeap();
    return p ? HeapReAlloc(ph, 0, p, n) : HeapAlloc(ph, 0, n);
}

#define ENGINE_NORETURN __attribute__((noreturn))
#define ENGINE_MAP(bytes) VirtualAlloc(NULL, (bytes), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
#define ENGINE_UNMAP(p, bytes) VirtualFree((p), 0, MEM_RELEASE)
#define ENGINE_REALLOC(p, bytes) engine_realloc((p), (bytes))
#include "xpheap_engine.h"

/* Emulated layout; keep in sync with research/xp_heap/export_ops.py (EMU_BASE, EMU_BASES). */
#define EMU_BASE 0x20000000u
#define EMU_SEGMENTS 6
static uint32_t emu_bases[EMU_SEGMENTS];
static uint32_t *req_size;           /* requested size per unit of a busy block's header */
static int emu_created;
static uint32_t n_alloc, n_free, n_realloc;

typedef HANDLE (WINAPI *HeapCreate_t)(DWORD, SIZE_T, SIZE_T);
typedef LPVOID (WINAPI *HeapAlloc_t)(HANDLE, DWORD, SIZE_T);
typedef LPVOID (WINAPI *HeapReAlloc_t)(HANDLE, DWORD, LPVOID, SIZE_T);
typedef BOOL (WINAPI *HeapFree_t)(HANDLE, DWORD, LPVOID);
typedef SIZE_T (WINAPI *HeapSize_t)(HANDLE, DWORD, LPCVOID);
typedef BOOL (WINAPI *HeapDestroy_t)(HANDLE);
typedef BOOL (WINAPI *HeapValidate_t)(HANDLE, DWORD, LPCVOID);
typedef SIZE_T (WINAPI *HeapCompact_t)(HANDLE, DWORD);
typedef BOOL (WINAPI *HeapWalk_t)(HANDLE, LPPROCESS_HEAP_ENTRY);

static HeapCreate_t oCreate;
static HeapAlloc_t oAlloc;
static HeapReAlloc_t oReAlloc;
static HeapFree_t oFree;
static HeapSize_t oSize;
static HeapDestroy_t oDestroy;
static HeapValidate_t oValidate;
static HeapCompact_t oCompact;
static HeapWalk_t oWalk;

#define EMU_HANDLE ((HANDLE)(uintptr_t)EMU_BASE)

static void emu_init(void)
{
    uint32_t total = 0x10000u, i;
    for (i = 0; i < EMU_SEGMENTS; i++) {
        emu_bases[i] = EMU_BASE + total;
        total += 0x100000u << i;
    }
    if (VirtualAlloc((void *)(uintptr_t)EMU_BASE, total, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
            != (void *)(uintptr_t)EMU_BASE)
        abort_process("cannot map the emulated heap at 0x20000000", NULL);
    heap_reset(EMU_BASE, emu_bases, EMU_SEGMENTS);
    req_size = VirtualAlloc(NULL, (SIZE_T)H.nunits * 4, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!req_size)
        abort_process("cannot map the size table", NULL);
    h_alloc(6144);                    /* RtlCreateHeap's lookaside table, then lookasides on */
    H.lookaside_on = 1;
}

static uint32_t slot_of(LPCVOID p) { return unit_of((uint32_t)(uintptr_t)p - UNIT); }

static HANDLE WINAPI hCreate(DWORD f, SIZE_T init, SIZE_T max)
{
    if (!emu_created) {
        emu_created = 1;
        emu_init();
        return EMU_HANDLE;
    }
    return oCreate(f, init, max);
}

static LPVOID WINAPI hAlloc(HANDLE h, DWORD f, SIZE_T n)
{
    uint32_t p;
    if (h != EMU_HANDLE)
        return oAlloc(h, f, n);
    n_alloc++;
    p = h_alloc((uint32_t)n);
    req_size[slot_of((LPCVOID)(uintptr_t)p)] = (uint32_t)n;
    if (f & HEAP_ZERO_MEMORY)
        memset((void *)(uintptr_t)p, 0, n);
    return (LPVOID)(uintptr_t)p;
}

static LPVOID WINAPI hReAlloc(HANDLE h, DWORD f, LPVOID p, SIZE_T n)
{
    uint32_t old, q;
    if (h != EMU_HANDLE)
        return oReAlloc(h, f, p, n);
    if (!p)
        return NULL;
    n_realloc++;
    old = req_size[slot_of(p)];
    if (f & HEAP_REALLOC_IN_PLACE_ONLY) {
        if (!realloc_inplace((uint32_t)(uintptr_t)p, (uint32_t)n))
            return NULL;
        q = (uint32_t)(uintptr_t)p;
    } else {
        q = h_realloc((uint32_t)(uintptr_t)p, (uint32_t)n);
        if (q != (uint32_t)(uintptr_t)p)
            memmove((void *)(uintptr_t)q, p, old < n ? old : n);
    }
    req_size[slot_of((LPCVOID)(uintptr_t)q)] = (uint32_t)n;
    if ((f & HEAP_ZERO_MEMORY) && n > old)
        memset((unsigned char *)(uintptr_t)q + old, 0, n - old);
    return (LPVOID)(uintptr_t)q;
}

static BOOL WINAPI hFree(HANDLE h, DWORD f, LPVOID p)
{
    if (h != EMU_HANDLE)
        return oFree(h, f, p);
    if (p) {
        n_free++;
        h_free((uint32_t)(uintptr_t)p);
    }
    return TRUE;
}

static int busy_unit(LPCVOID p, uint32_t *out)
{
    uintptr_t addr = (uintptr_t)p;
    uint64_t hdr;
    Segment *seg = NULL;
    uint32_t u, n;
    int i;
    if (!emu_created || !H.c || !p || addr < H.lo + UNIT || (addr - H.lo) % UNIT)
        return 0;
    hdr = (uint64_t)addr - UNIT;
    for (i = 0; i < H.nseg; i++)
        if (H.seg[i].first <= hdr && hdr < H.seg[i].end) {
            seg = &H.seg[i];
            break;
        }
    if (!seg)
        return 0;
    for (i = 0; i < seg->nucr; i++)
        if (seg->ucr[i].addr <= hdr && hdr < seg->ucr[i].addr + seg->ucr[i].size)
            return 0;
    u = (uint32_t)((hdr - H.lo) / UNIT);
    n = SZ(u);
    if (!n || !is_busy(u) || hdr + (uint64_t)n * UNIT > seg->end)
        return 0;
    if (H.lookaside_on && n < MAXLIST) {
        for (i = 0; i < H.la_n[n]; i++)
            if (H.la[n][i] == u)
                return 0;
    }
    *out = u;
    return 1;
}

static BOOL validate_heap(void)
{
    int i, k;
    if (!emu_created || !H.c || H.nseg <= 0)
        return FALSE;
    for (i = 0; i < H.nseg; i++) {
        Segment *s = &H.seg[i];
        uint64_t pos = s->base;
        if (s->base >= s->first || s->first > s->end)
            return FALSE;
        for (k = 0; k <= s->nucr; k++) {
            uint64_t limit = k < s->nucr ? s->ucr[k].addr : s->end;
            if (k < s->nucr) {
                Ucr *r = &s->ucr[k];
                if (r->addr < s->first || !r->size || r->addr % PAGE || r->size % PAGE
                        || r->addr < pos || r->addr + r->size > s->end)
                    return FALSE;
            }
            while (pos < limit) {
                uint32_t u, sz;
                if (pos < H.lo || (pos - H.lo) % UNIT || (pos - H.lo) / UNIT >= H.nunits - 2)
                    return FALSE;
                u = (uint32_t)((pos - H.lo) / UNIT);
                sz = SZ(u);
                if (!sz || pos + (uint64_t)sz * UNIT > limit || H.c[u + sz].psz != sz)
                    return FALSE;
                pos += (uint64_t)sz * UNIT;
            }
            if (k < s->nucr)
                pos = s->ucr[k].addr + s->ucr[k].size;
        }
    }
    return TRUE;
}

static SIZE_T WINAPI hSize(HANDLE h, DWORD f, LPCVOID p)
{
    uint32_t u;
    if (h != EMU_HANDLE)
        return oSize(h, f, p);
    if (!busy_unit(p, &u))
        return (SIZE_T)-1;
    return req_size[u];
}

static BOOL WINAPI hDestroy(HANDLE h)
{
    return h == EMU_HANDLE ? TRUE : oDestroy(h);
}

static BOOL WINAPI hValidate(HANDLE h, DWORD f, LPCVOID p)
{
    uint32_t u;
    if (h != EMU_HANDLE)
        return oValidate(h, f, p);
    return p ? (busy_unit(p, &u) ? TRUE : FALSE) : validate_heap();
}

static SIZE_T WINAPI hCompact(HANDLE h, DWORD f)
{
    return h == EMU_HANDLE ? 0 : oCompact(h, f);
}

static BOOL WINAPI hWalk(HANDLE h, LPPROCESS_HEAP_ENTRY e)
{
    if (h == EMU_HANDLE)
        abort_process("HeapWalk on the emulated heap is not supported", NULL);
    return oWalk(h, e);
}

static void patch(HMODULE mod)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((char *)mod + dos->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)((char *)mod +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    static const char *names[9] = { "HeapCreate", "HeapAlloc", "HeapReAlloc", "HeapFree", "HeapSize",
                                    "HeapDestroy", "HeapValidate", "HeapCompact", "HeapWalk" };
    void *hook[9] = { (void *)hCreate, (void *)hAlloc, (void *)hReAlloc, (void *)hFree, (void *)hSize,
                      (void *)hDestroy, (void *)hValidate, (void *)hCompact, (void *)hWalk };
    void *want[9];
    int i, patched = 0;
    for (i = 0; i < 9; i++)
        want[i] = (void *)GetProcAddress(k32, names[i]);
    oCreate = (HeapCreate_t)want[0]; oAlloc = (HeapAlloc_t)want[1]; oReAlloc = (HeapReAlloc_t)want[2];
    oFree = (HeapFree_t)want[3]; oSize = (HeapSize_t)want[4]; oDestroy = (HeapDestroy_t)want[5];
    oValidate = (HeapValidate_t)want[6]; oCompact = (HeapCompact_t)want[7]; oWalk = (HeapWalk_t)want[8];
    for (; d->Name; d++) {
        void **iat = (void **)((char *)mod + d->FirstThunk);
        for (; *iat; iat++)
            for (i = 0; i < 9; i++)
                if (want[i] && *iat == want[i]) {
                    DWORD old;
                    VirtualProtect(iat, sizeof(void *), PAGE_READWRITE, &old);
                    *iat = hook[i];
                    VirtualProtect(iat, sizeof(void *), old, &old);
                    patched |= 1 << i;
                }
    }
    if ((patched & 0x1f) != 0x1f)
        abort_process("msvcrt does not import the expected heap functions", NULL);
}

static char *decimal(char *end, uint32_t v)
{
    *--end = 0;
    do { *--end = (char)('0' + v % 10); v /= 10; } while (v);
    return end;
}

/* Opt-in diagnostic (XPHEAP_REPORT set): operations served by the model, printed at exit. */
static void report(void)
{
    char buf[3][16];
    char var[4];
    SetLastError(0);
    if (!GetEnvironmentVariableA("XPHEAP_REPORT", var, sizeof var)
            && GetLastError() == ERROR_ENVVAR_NOT_FOUND)
        return;
    message(emu_created ? "model served allocs" : "model never created", decimal(buf[0] + 16, n_alloc));
    message("frees", decimal(buf[1] + 16, n_free));
    message("reallocs", decimal(buf[2] + 16, n_realloc));
}

__declspec(dllexport) int xpheap_marker(void) { return 0; }

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        HMODULE crt = GetModuleHandleA("msvcrt.dll");
        if (!crt)
            abort_process("msvcrt.dll is not loaded", NULL);
        patch(crt);
    } else if (reason == DLL_PROCESS_DETACH) {
        report();
    }
    return TRUE;
}
