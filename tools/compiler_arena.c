/* A process-lifetime arena for the single-threaded historical SDCC driver.
 * Symbol pointers retain allocation order, independently of host malloc zones.
 * No allocations are reused; the compiler process releases the entire mapping.
 */
#include <errno.h>
#include <malloc/malloc.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define CAPACITY ((size_t)1024 * 1024 * 1024)

static _Atomic(uintptr_t) base;
static _Atomic(size_t) used;

static void *arena_malloc(size_t size)
{
    uintptr_t start = atomic_load(&base);
    if (!start) {
        void *memory = mmap(NULL, CAPACITY, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANON, -1, 0);
        if (memory == MAP_FAILED)
            return NULL;
        uintptr_t expected = 0;
        if (!atomic_compare_exchange_strong(&base, &expected, (uintptr_t)memory))
            munmap(memory, CAPACITY);
        start = atomic_load(&base);
    }
    if (size > CAPACITY - 32) {
        errno = ENOMEM;
        return NULL;
    }
    size_t total = (size + 31) & ~(size_t)15;
    size_t offset = atomic_load(&used);
    do {
        if (offset > CAPACITY - total) {
            errno = ENOMEM;
            return NULL;
        }
    } while (!atomic_compare_exchange_weak(&used, &offset, offset + total));
    size_t *header = (size_t *)(start + offset);
    *header = size;
    return (char *)header + 16;
}

static int belongs(const void *pointer)
{
    uintptr_t address = (uintptr_t)pointer;
    uintptr_t start = atomic_load(&base);
    return start && address >= start && address < start + CAPACITY;
}

static void *arena_calloc(size_t count, size_t size)
{
    if (count && size > SIZE_MAX / count) {
        errno = ENOMEM;
        return NULL;
    }
    size_t length = count * size;
    void *pointer = arena_malloc(length);
    return pointer ? memset(pointer, 0, length) : NULL;
}

static size_t arena_size(const void *pointer)
{
    return belongs(pointer) ? *(const size_t *)((const char *)pointer - 16)
                            : malloc_size(pointer);
}

static void arena_free(void *pointer)
{
    if (!belongs(pointer))
        free(pointer);
}

static void *arena_realloc(void *pointer, size_t size)
{
    if (!pointer)
        return arena_malloc(size);
    if (!belongs(pointer))
        return realloc(pointer, size);
    size_t old = arena_size(pointer);
    void *result = arena_malloc(size);
    if (result)
        memcpy(result, pointer, old < size ? old : size);
    return result;
}

#define INTERPOSE(replacement, original) \
    __attribute__((used)) static struct { const void *replace; const void *original; } \
    interpose_##original __attribute__((section("__DATA,__interpose"))) = \
        {(const void *)(replacement), (const void *)(original)}

INTERPOSE(arena_malloc, malloc);
INTERPOSE(arena_calloc, calloc);
INTERPOSE(arena_realloc, realloc);
INTERPOSE(arena_free, free);
INTERPOSE(arena_size, malloc_size);
