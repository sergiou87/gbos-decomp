/*
 * Model of the Windows XP SP3 heap (ntdll 5.1.2600.5512) behind msvcrt's CRT heap: lookaside lists
 * with adaptive depth, LIFO dedicated free lists, size-ordered FreeLists[0], coalescing, commits
 * through uncommitted ranges, decommits, segment growth and the LargeBlocksIndex heap cache.
 * Derived from the ntdll disassembly and validated against every allocation result of traced
 * compiles (research/xp_heap: xpheap.py is the reference model, xpsim.c the research driver).
 *
 * Shared by research/xp_heap/xpsim.c and tools/xpheap/xpheap.c (the Win32 DLL that makes the
 * original win32 SDCC allocate from this model). The includer defines, before including:
 *   ENGINE_NORETURN            noreturn attribute
 *   ENGINE_MAP(bytes)          zeroed memory for the cell array, NULL on failure
 *   ENGINE_UNMAP(p, bytes)     release it
 *   ENGINE_REALLOC(p, bytes)   realloc for small internal arrays
 * and provides fail() / die() (see below), memset() and memmove().
 */
#ifndef XPHEAP_ENGINE_H
#define XPHEAP_ENGINE_H

#define UNIT 8u
#define PAGE 4096u
#define MAXLIST 128u
#define VM_THRESHOLD 0xFE00u               /* units (0x7f000 bytes) */
#define DECOMMIT_BLOCK (0x1000u >> 3)      /* PEB HeapDeCommitFreeBlockThreshold */
#define DECOMMIT_TOTAL (0x10000u >> 3)     /* PEB HeapDeCommitTotalFreeThreshold */
#define SEGMENT_COMMIT 0x2000u             /* PEB HeapSegmentCommit */
#define SEGMENT_RESERVE 0x100000u          /* PEB HeapSegmentReserve */
#define LARGE (0x80u + ((DECOMMIT_BLOCK + 0x19Fu) & ~0x1Fu) - 1u)   /* 1023 */
#define MAXDEPTH 256

#define NIL 0xFFFFFFFFu
#define SZ_MASK 0x00FFFFFFu
#define F_BUSY 0x01000000u
#define F_DED 0x02000000u                  /* linked in a dedicated free list */
#define F_F0 0x04000000u                   /* linked in FreeLists[0] */
#define F_MARK 0x08000000u                 /* address in Heap.marks */
#define F_SEG 0x10000000u                  /* address in Heap.seg_bases */
#define F_KEEP (F_MARK | F_SEG)

#define F0_WORDS ((VM_THRESHOLD >> 6) + 1)
#define F0_SUMS ((F0_WORDS + 63) >> 6)
#define MAXSEG 64

typedef struct { uint32_t meta, psz, link; } Cell;
typedef struct { uint64_t addr, size; } Ucr;
typedef struct {
    uint64_t base, first, end;
    Ucr *ucr;                              /* sorted uncommitted ranges */
    int nucr, cap;
} Segment;

static struct {
    uint64_t lo;                           /* address of unit 0 */
    uint64_t nunits;
    size_t map_bytes;
    Cell *c;
    uint32_t ded_tail[MAXLIST];            /* FreeLists[1..127]: last inserted */
    uint64_t ded_bits[2];
    uint32_t f0_head[VM_THRESHOLD + 1];    /* FreeLists[0] buckets: newest ... */
    uint32_t f0_tail[VM_THRESHOLD + 1];    /* ... oldest */
    uint64_t f0_bits[F0_WORDS], f0_sum[F0_SUMS];
    int64_t total_free;
    int lookaside_on;
    uint32_t la[MAXLIST][MAXDEPTH];
    int la_n[MAXLIST];
    int64_t ls[MAXLIST][5];                /* depth, allocs, misses, last allocs, last misses */
    int64_t decommit_count;
    int64_t nondedicated;
    int has_index;
    int64_t x_large, x_limit, x_minc, x_activity, x_ext;
    uint64_t segment_reserve;
    const uint32_t *segbases;
    int nsegbases, segbase_pos;
    Segment seg[MAXSEG];
    int nseg;
} H;

/* Provided by the includer: fail() aborts the current simulation (model exception), die()
 * aborts the program. Both never return. */
static void fail(const char *fmt, ...) ENGINE_NORETURN;
static void die(const char *fmt, ...) ENGINE_NORETURN;

static inline uint64_t ADDR(uint32_t u) { return H.lo + (uint64_t)u * UNIT; }
static inline uint32_t SZ(uint32_t u) { return H.c[u].meta & SZ_MASK; }
static inline int is_busy(uint32_t u) { return (H.c[u].meta & F_BUSY) != 0; }

static inline uint32_t unit_of(uint64_t addr)
{
    if (addr < H.lo || (addr - H.lo) % UNIT || (addr - H.lo) / UNIT >= H.nunits - 2)
        fail("KeyError(0x%llx)", (unsigned long long)addr);
    return (uint32_t)((addr - H.lo) / UNIT);
}

static inline uint64_t page_up(uint64_t x) { return (x + PAGE - 1) / PAGE * PAGE; }
static inline uint64_t page_down(uint64_t x) { return x / PAGE * PAGE; }
static inline uint64_t units(uint32_t size) { return ((uint64_t)(size ? size : 1) + 15) / 8; }

/* --- physical chunks ------------------------------------------------------------------- */
static inline void set_size(uint32_t u, uint64_t n)
{
    if (n == 0 || n > SZ_MASK || u + n >= H.nunits)
        fail("chunk size %llu at 0x%llx out of range", (unsigned long long)n,
             (unsigned long long)ADDR(u));
    H.c[u].meta = (H.c[u].meta & ~SZ_MASK) | (uint32_t)n;
    H.c[u + n].psz = (uint32_t)n;
}

static inline void add_chunk(uint32_t u, uint64_t n, int busy)
{
    H.c[u].meta = (H.c[u].meta & F_KEEP) | (busy ? F_BUSY : 0);
    set_size(u, n);
}

static inline void remove_chunk(uint32_t u)
{
    if (!(H.c[u].meta & SZ_MASK))
        fail("KeyError(0x%llx)", (unsigned long long)ADDR(u));
    H.c[u].meta &= F_KEEP;
}

static inline uint32_t next_chunk(uint32_t u)
{
    uint32_t v = u + SZ(u);
    uint32_t m = H.c[v].meta;
    return ((m & SZ_MASK) && !(m & F_SEG)) ? v : NIL;
}

static inline uint32_t prev_chunk(uint32_t u)
{
    uint32_t p = H.c[u].psz;
    if (p == 0 || p > u)
        return NIL;
    return SZ(u - p) == p ? u - p : NIL;
}

static inline int is_free(uint32_t u) { return u != NIL && !is_busy(u); }

static Segment *segment_of(uint64_t addr)
{
    for (int i = 0; i < H.nseg; i++)
        if (H.seg[i].base <= addr && addr < H.seg[i].end)
            return &H.seg[i];
    fail("KeyError('0x%llx')", (unsigned long long)addr);
}

/* --- uncommitted ranges ---------------------------------------------------------------- */
static void ucr_insert_at(Segment *s, int i, uint64_t addr, uint64_t size)
{
    if (s->nucr == s->cap) {
        s->cap = s->cap ? 2 * s->cap : 16;
        s->ucr = ENGINE_REALLOC(s->ucr, (size_t)s->cap * sizeof(Ucr));
        if (!s->ucr)
            die("out of memory");
    }
    memmove(&s->ucr[i + 1], &s->ucr[i], (size_t)(s->nucr - i) * sizeof(Ucr));
    s->ucr[i].addr = addr;
    s->ucr[i].size = size;
    s->nucr++;
}

static void ucr_delete(Segment *s, int i)
{
    memmove(&s->ucr[i], &s->ucr[i + 1], (size_t)(s->nucr - i - 1) * sizeof(Ucr));
    s->nucr--;
}

/* Segment.add_ucr (RtlpInsertUnCommittedPages): insert, merging adjacent ranges. */
static void add_ucr(Segment *s, uint64_t addr, uint64_t size)
{
    int lo = 0, hi = s->nucr;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        const Ucr *m = &s->ucr[mid];
        if (m->addr < addr || (m->addr == addr && m->size < size))
            lo = mid + 1;
        else
            hi = mid;
    }
    int i = lo;
    ucr_insert_at(s, i, addr, size);
    if (i + 1 < s->nucr && s->ucr[i].addr + s->ucr[i].size == s->ucr[i + 1].addr) {
        s->ucr[i].size += s->ucr[i + 1].size;
        ucr_delete(s, i + 1);
    }
    if (i > 0 && s->ucr[i - 1].addr + s->ucr[i - 1].size == s->ucr[i].addr) {
        s->ucr[i - 1].size += s->ucr[i].size;
        ucr_delete(s, i);
    }
}

static uint64_t ucr_pages(const Segment *s)
{
    uint64_t t = 0;
    for (int i = 0; i < s->nucr; i++)
        t += s->ucr[i].size;
    return t / PAGE;
}

static uint64_t ucr_largest(const Segment *s)
{
    uint64_t m = 0;
    for (int i = 0; i < s->nucr; i++)
        if (s->ucr[i].size > m)
            m = s->ucr[i].size;
    return m;
}

/* --- free lists ------------------------------------------------------------------------ */
/* Dedicated lists: c[u].link = older neighbour, c[u + 1].link = newer neighbour. */
static inline void ded_push(uint32_t n, uint32_t u)
{
    uint32_t t = H.ded_tail[n];
    H.c[u].link = t;
    H.c[u + 1].link = NIL;
    if (t != NIL)
        H.c[t + 1].link = u;
    else
        H.ded_bits[n >> 6] |= 1ull << (n & 63);
    H.ded_tail[n] = u;
}

static inline uint32_t ded_pop(uint32_t n)
{
    uint32_t u = H.ded_tail[n];
    uint32_t o = H.c[u].link;
    H.ded_tail[n] = o;
    if (o != NIL)
        H.c[o + 1].link = NIL;
    else
        H.ded_bits[n >> 6] &= ~(1ull << (n & 63));
    H.c[u].meta &= ~F_DED;
    return u;
}

static inline void ded_del(uint32_t n, uint32_t u)
{
    uint32_t o = H.c[u].link, w = H.c[u + 1].link;
    if (w != NIL)
        H.c[w].link = o;
    else
        H.ded_tail[n] = o;
    if (o != NIL)
        H.c[o + 1].link = w;
    if (H.ded_tail[n] == NIL)
        H.ded_bits[n >> 6] &= ~(1ull << (n & 63));
}

static inline uint32_t ded_first_ge(uint32_t k)
{
    if (k < 64) {
        uint64_t b = H.ded_bits[0] & (~0ull << k);
        if (b)
            return (uint32_t)__builtin_ctzll(b);
        k = 64;
    }
    if (k < 128) {
        uint64_t b = H.ded_bits[1] & (~0ull << (k - 64));
        if (b)
            return 64 + (uint32_t)__builtin_ctzll(b);
    }
    return NIL;
}

/* FreeLists[0]: bucket per size; c[u].link = next (older), c[u + 1].link = previous (newer). */
static inline void f0_setbit(uint32_t n)
{
    uint32_t w = n >> 6;
    if (!H.f0_bits[w])
        H.f0_sum[w >> 6] |= 1ull << (w & 63);
    H.f0_bits[w] |= 1ull << (n & 63);
}

static inline void f0_clrbit(uint32_t n)
{
    uint32_t w = n >> 6;
    H.f0_bits[w] &= ~(1ull << (n & 63));
    if (!H.f0_bits[w])
        H.f0_sum[w >> 6] &= ~(1ull << (w & 63));
}

static inline void f0_push(uint32_t n, uint32_t u)
{
    uint32_t h = H.f0_head[n];
    H.c[u].link = h;
    H.c[u + 1].link = NIL;
    if (h != NIL)
        H.c[h + 1].link = u;
    else {
        H.f0_tail[n] = u;
        f0_setbit(n);
    }
    H.f0_head[n] = u;
}

static inline void f0_del(uint32_t n, uint32_t u)
{
    uint32_t nx = H.c[u].link, pv = H.c[u + 1].link;
    if (pv != NIL)
        H.c[pv].link = nx;
    else
        H.f0_head[n] = nx;
    if (nx != NIL)
        H.c[nx + 1].link = pv;
    else
        H.f0_tail[n] = pv;
    if (H.f0_head[n] == NIL)
        f0_clrbit(n);
}

/* smallest non-empty bucket >= n */
static uint32_t f0_first_ge(uint32_t n)
{
    if (n > VM_THRESHOLD)
        return NIL;
    uint32_t w = n >> 6;
    uint64_t b = H.f0_bits[w] & (~0ull << (n & 63));
    if (b)
        return (w << 6) + (uint32_t)__builtin_ctzll(b);
    w++;
    if (w >= F0_WORDS)
        return NIL;
    uint32_t s = w >> 6;
    uint64_t sb = H.f0_sum[s] & (~0ull << (w & 63));
    for (;;) {
        if (sb) {
            uint32_t ww = (s << 6) + (uint32_t)__builtin_ctzll(sb);
            return (ww << 6) + (uint32_t)__builtin_ctzll(H.f0_bits[ww]);
        }
        if (++s >= F0_SUMS)
            return NIL;
        sb = H.f0_sum[s];
    }
}

/* largest non-empty bucket */
static uint32_t f0_last(void)
{
    for (int s = F0_SUMS - 1; s >= 0; s--)
        if (H.f0_sum[s]) {
            uint32_t ww = ((uint32_t)s << 6) + 63 - (uint32_t)__builtin_clzll(H.f0_sum[s]);
            return (ww << 6) + 63 - (uint32_t)__builtin_clzll(H.f0_bits[ww]);
        }
    return NIL;
}

/* RtlpInitializeListIndex */
static void create_index(void)
{
    int64_t large = 0;
    for (uint32_t k = f0_first_ge(LARGE); k != NIL; k = f0_first_ge(k + 1))
        for (uint32_t u = H.f0_head[k]; u != NIL; u = H.c[u].link)
            large++;
    H.has_index = 1;
    H.x_large = H.x_limit = H.x_minc = large;
    H.x_activity = 1;
    H.x_ext = 0;
}

/* RtlpUpdateIndexInsertBlock */
static inline void index_insert(uint32_t n)
{
    H.nondedicated++;
    if (!H.has_index) {
        if (H.nondedicated >= 0x20)
            create_index();
        return;
    }
    if (n >= LARGE) {
        H.x_large++;
        if (H.x_activity) {
            H.x_activity++;
            if (H.x_large > H.x_limit)
                H.x_limit = H.x_large;
        }
    }
}

/* RtlpUpdateIndexRemoveBlock */
static inline void index_remove(uint32_t n)
{
    H.nondedicated--;
    if (H.has_index && n >= LARGE) {
        H.x_large--;
        if (H.x_activity) {
            H.x_activity++;
            if (H.x_large < H.x_minc)
                H.x_minc = H.x_large;
        }
    }
}

static inline void insert_one(uint32_t u)
{
    uint32_t m = H.c[u].meta;
    uint32_t n = m & SZ_MASK;
    if (m & (F_DED | F_F0))
        fail("block 0x%llx inserted twice", (unsigned long long)ADDR(u));
    m &= ~(F_BUSY | F_MARK);
    if (n < MAXLIST) {
        H.c[u].meta = m | F_DED;
        ded_push(n, u);
        H.total_free += n;
    } else {
        H.c[u].meta = m | F_F0;
        f0_push(n, u);
        H.total_free += n;
        index_insert(n);
    }
}

/* RtlpInsertFreeBlock (splitting blocks above the maximum block size) */
static void insert_block(uint32_t u)
{
    uint32_t n = SZ(u);
    while (n > VM_THRESHOLD) {
        uint32_t piece = n == VM_THRESHOLD + 1 ? VM_THRESHOLD - 0x10 : VM_THRESHOLD;
        set_size(u, piece);
        uint32_t rest = u + piece;
        add_chunk(rest, n - piece, 0);
        insert_one(u);
        u = rest;
        n -= piece;
    }
    insert_one(u);
}

static inline void unlink_block(uint32_t u)
{
    uint32_t m = H.c[u].meta;
    uint32_t n = m & SZ_MASK;
    m &= ~F_MARK;
    if (n < MAXLIST) {
        if (!(m & F_DED))
            fail("KeyError(0x%llx) unlinking from FreeLists[%u]", (unsigned long long)ADDR(u), n);
        ded_del(n, u);
        H.c[u].meta = m & ~F_DED;
    } else {
        if (!(m & F_F0))
            fail("KeyError(0x%llx) unlinking from FreeLists[0]", (unsigned long long)ADDR(u));
        f0_del(n, u);
        H.c[u].meta = m & ~F_F0;
        index_remove(n);
    }
    H.total_free -= n;
}

/* RtlpCoalesceFreeBlocks */
static uint32_t coalesce(uint32_t u)
{
    uint32_t p = prev_chunk(u);
    if (is_free(p) && (uint64_t)SZ(p) + SZ(u) <= VM_THRESHOLD) {
        unlink_block(p);
        set_size(p, (uint64_t)SZ(p) + SZ(u));
        remove_chunk(u);
        u = p;
    }
    uint32_t nx = next_chunk(u);
    if (is_free(nx) && (uint64_t)SZ(u) + SZ(nx) <= VM_THRESHOLD) {
        unlink_block(nx);
        set_size(u, (uint64_t)SZ(u) + SZ(nx));
        remove_chunk(nx);
    }
    return u;
}

/* --- allocation ------------------------------------------------------------------------ */
static void adjust(uint32_t n)
{
    int64_t *st = H.ls[n];
    int64_t allocs = st[1] - st[3];
    int64_t misses = st[2] - st[4];
    st[3] = st[1];
    st[4] = st[2];
    if (misses >= allocs)
        misses = allocs;
    if (allocs == 0)
        allocs = 1;
    int64_t ratio = misses * 1000 / allocs;
    int64_t depth = st[0];
    if (allocs < 25)
        depth = depth <= 14 ? 4 : depth - 10;
    else if (ratio < 5)
        depth = depth > 5 ? depth - 1 : 4;
    else {
        depth = depth + 256 * ratio / 2000 + 5;
        if (depth > 256)
            depth = 256;
    }
    st[0] = depth;
}

static uint32_t split(uint32_t u, uint32_t n)
{
    int64_t rest = (int64_t)SZ(u) - n;
    H.c[u].meta |= F_BUSY;
    if (rest == 0 || rest == 1)
        return u;
    if (rest < 0)
        fail("split of a %u-unit block for %u units", SZ(u), n);
    set_size(u, n);
    uint32_t r = u + n;
    add_chunk(r, (uint64_t)rest, 0);
    uint32_t nx = next_chunk(r);
    if (is_free(nx)) {
        unlink_block(nx);
        set_size(r, (uint64_t)SZ(r) + SZ(nx));
        remove_chunk(nx);
    }
    insert_block(r);
    return u;
}

static Segment *new_segment(uint64_t base, uint64_t first, uint64_t end)
{
    if (H.nseg >= MAXSEG)
        fail("too many segments");
    if (end > H.lo + (H.nunits - 4) * UNIT)
        fail("segment 0x%llx-0x%llx outside the simulated range", (unsigned long long)base,
             (unsigned long long)end);
    Segment *s = &H.seg[H.nseg++];
    s->base = base;
    s->first = first;
    s->end = end;
    s->nucr = 0;
    return s;
}

static uint32_t find_and_commit(Segment *s, uint64_t size)
{
    for (int i = 0; i < s->nucr; i++) {
        Ucr *r = &s->ucr[i];
        if (r->size >= size) {
            if (r->size - size <= DECOMMIT_BLOCK * UNIT && r->size < VM_THRESHOLD * UNIT)
                size = r->size;
            uint64_t addr = r->addr;
            r->addr += size;
            r->size -= size;
            if (r->size == 0)
                ucr_delete(s, i);
            uint32_t u = unit_of(addr);
            add_chunk(u, size / UNIT, 0);
            return u;
        }
    }
    return NIL;
}

/* RtlpExtendHeap: returns a free block (inserted in the free lists) of at least n units */
static uint32_t extend(uint32_t n)
{
    if (H.has_index) {
        if (H.x_limit < 0x80)
            H.x_limit++;
        H.x_ext++;
    }
    uint64_t size = page_up((uint64_t)n * UNIT);
    uint64_t pages = size / PAGE;
    for (int i = 0; i < H.nseg; i++) {
        Segment *s = &H.seg[i];
        if (pages <= ucr_pages(s) && size <= ucr_largest(s)) {
            uint32_t b = find_and_commit(s, size);
            if (b != NIL) {
                b = coalesce(b);
                insert_block(b);
                return b;
            }
        }
    }
    if (H.segbase_pos >= H.nsegbases)
        fail("IndexError('pop from an empty deque')");
    uint64_t base = H.segbases[H.segbase_pos++];
    uint64_t need = (uint64_t)n * UNIT + PAGE;
    uint64_t reserve = need > H.segment_reserve ? need : H.segment_reserve;
    H.segment_reserve += reserve;
    uint64_t commit = page_up(need > SEGMENT_COMMIT ? need : SEGMENT_COMMIT);
    Segment *s = new_segment(base, base + 0x40, base + reserve);
    uint32_t ub = unit_of(base);
    H.c[ub].meta |= F_SEG;
    if (commit < reserve)
        add_ucr(s, base + commit, reserve - commit);
    add_chunk(ub, (s->first - base) / UNIT, 1);
    uint32_t uf = unit_of(s->first);
    add_chunk(uf, (base + commit - s->first) / UNIT, 0);
    insert_block(uf);
    return uf;
}

static void flush_largest(void);

static uint32_t h_alloc(uint32_t size)
{
    uint64_t n64 = units(size);
    if (H.lookaside_on && n64 < MAXLIST) {
        uint32_t n = (uint32_t)n64;
        int64_t *st = H.ls[n];
        if (st[1] - st[3] >= st[0] * 128)
            adjust(n);
        st[1]++;
        if (H.la_n[n])
            return (uint32_t)(ADDR(H.la[n][--H.la_n[n]]) + UNIT);
        st[2]++;
    }
    if (n64 > VM_THRESHOLD)
        fail("NotImplementedError('virtual allocation of %llu units')", (unsigned long long)n64);
    uint32_t n = (uint32_t)n64;
    uint32_t u = NIL;
    if (n < MAXLIST) {
        if (H.ded_tail[n] != NIL) {
            u = ded_pop(n);
            H.total_free -= n;
            H.c[u].meta |= F_BUSY;
            return (uint32_t)(ADDR(u) + UNIT);
        }
        uint32_t k = ded_first_ge(n + 1);
        if (k != NIL) {
            u = ded_pop(k);
            H.total_free -= k;
        }
    }
    if (u == NIL) {
        uint32_t k = f0_first_ge(n);
        if (k != NIL) {
            if (H.has_index && H.x_activity && n > DECOMMIT_BLOCK && k > 4 * n) {
                /* cached big block much larger than the request: flush the largest, extend */
                flush_largest();
            } else {
                u = H.f0_head[k];
                unlink_block(u);
            }
        }
    }
    if (u == NIL) {
        u = extend(n);
        unlink_block(u);
    }
    return (uint32_t)(ADDR(split(u, n)) + UNIT);
}

/* --- free ------------------------------------------------------------------------------ */
static void decommit(uint32_t u);

static void decommit_now(Segment *s, uint32_t u, uint64_t start, uint64_t size,
                         uint64_t preceding, uint64_t trailing, uint64_t dend)
{
    uint32_t deferred[2];
    int nd = 0;
    remove_chunk(u);
    add_ucr(s, start, size);
    if (preceding) {
        add_chunk(u, preceding, 0);
        uint32_t b = coalesce(u);
        if (SZ(b) >= DECOMMIT_BLOCK)
            deferred[nd++] = b;
        else
            insert_block(b);
    }
    if (trailing) {
        uint32_t t = unit_of(dend);
        add_chunk(t, trailing, 0);
        uint32_t b = coalesce(t);
        if (SZ(b) >= DECOMMIT_BLOCK)
            deferred[nd++] = b;
        else
            insert_block(b);
    }
    if (nd) {
        int had = H.has_index;          /* x = self.index, captured before the loop */
        int64_t saved = had ? H.x_activity : 0;
        if (had)
            H.x_activity = 0;
        for (int i = 0; i < nd; i++)
            decommit(deferred[i]);
        if (had)
            H.x_activity = saved;
    }
}

static void flush_contents(void);

/* RtlpDeCommitFreeBlock for an unlinked free block */
static void decommit(uint32_t u)
{
    uint32_t n = SZ(u);
    uint64_t addr = ADDR(u);
    Segment *s = segment_of(addr);
    int first = prev_chunk(u) == NIL;
    int last = next_chunk(u) == NIL;
    uint64_t start = page_up(addr);
    uint64_t preceding = (start - addr) / UNIT;
    if (preceding == 1) {
        start += PAGE;
        preceding = 0x201;
    }
    uint64_t end = addr + (uint64_t)n * UNIT;
    uint64_t dend = page_down(end);
    uint64_t trailing = (end - dend) / UNIT;
    if (trailing == 1) {
        dend -= PAGE;
        trailing = 0x201;
    }
    uint64_t size = dend > start ? dend - start : 0;
    if (size == 0)
        insert_block(u);
    else if (s->nucr >= 16 && (first || last))
        decommit_now(s, u, start, size, preceding, trailing, dend);
    else if (!H.has_index) {
        /* Heap->DecommitCount: the cache is created on the 256th decommit, which proceeds */
        H.decommit_count++;
        if (H.decommit_count == 256)
            create_index();
        decommit_now(s, u, start, size, preceding, trailing, dend);
    } else if (H.x_activity == 0 || (first && trailing == 0))
        decommit_now(s, u, start, size, preceding, trailing, dend);
    else if (H.x_large < H.x_limit)
        insert_block(u);
    else {
        uint32_t k = f0_last();
        if (k != NIL && k > (uint64_t)n + 0x200) {
            insert_block(u);
            flush_largest();
        } else
            decommit_now(s, u, start, size, preceding, trailing, dend);
    }
    if (H.has_index && H.x_activity >= 0x400)
        flush_contents();
}

/* RtlpFlushLargestCacheBlock */
static void flush_largest(void)
{
    if (!H.has_index || H.x_activity == 0)
        return;
    uint32_t k = f0_last();
    if (k == NIL)
        return;
    int64_t saved = H.x_activity;
    H.x_activity = 0;
    uint32_t u = H.f0_tail[k];
    unlink_block(u);
    H.c[u].meta |= F_BUSY;
    decommit(u);
    H.x_activity = saved;
}

/* RtlpFlushCacheContents */
static uint32_t *victims;
static size_t victims_cap;

static void flush_contents(void)
{
    if (!H.has_index || H.x_activity == 0)
        return;
    int64_t threshold = H.x_limit - H.x_minc;
    if (H.x_large > threshold) {
        int64_t to_flush = H.x_large - threshold;
        int64_t remaining = H.x_large;
        size_t nv = 0;
        H.x_activity = 0;
        /* snapshot order: sizes >= LARGE ascending, newest first; the loop only unlinks */
        for (uint32_t k = f0_first_ge(LARGE); k != NIL; k = f0_first_ge(k + 1)) {
            uint32_t u = H.f0_head[k];
            while (u != NIL) {
                uint32_t next = H.c[u].link;
                if ((H.c[u].meta & F_MARK) || to_flush >= remaining) {
                    unlink_block(u);
                    H.c[u].meta |= F_BUSY;
                    if (nv == victims_cap) {
                        victims_cap = victims_cap ? 2 * victims_cap : 256;
                        victims = ENGINE_REALLOC(victims, victims_cap * sizeof *victims);
                        if (!victims)
                            die("out of memory");
                    }
                    victims[nv++] = u;
                    to_flush--;
                } else
                    H.c[u].meta |= F_MARK;
                remaining--;
                u = next;
            }
        }
        /* decommit() can re-enter flush_contents() only with activity 0 (a no-op), so the
           shared victims buffer is not reused while this loop runs */
        for (size_t i = 0; i < nv; i++)
            decommit(victims[i]);
    }
    H.x_limit = H.x_large;
    H.x_minc = H.x_large;
    H.x_activity = 1;
}

static uint32_t chunk_of_ptr(uint32_t ptr)
{
    if (ptr < UNIT)
        fail("KeyError(%lld)", (long long)ptr - (long long)UNIT);
    uint32_t u = unit_of((uint64_t)ptr - UNIT);
    if (!(H.c[u].meta & SZ_MASK))
        fail("KeyError(%u)", ptr - UNIT);
    return u;
}

static void h_free(uint32_t ptr)
{
    uint32_t u = chunk_of_ptr(ptr);
    uint32_t n = SZ(u);
    if (H.lookaside_on && n < MAXLIST) {
        if (H.la_n[n] < H.ls[n][0]) {
            H.la[n][H.la_n[n]++] = u;
            return;
        }
    }
    H.c[u].meta &= ~F_BUSY;
    u = coalesce(u);
    n = SZ(u);
    if (n < MAXLIST) {
        insert_block(u);
        return;
    }
    if (n >= DECOMMIT_BLOCK && H.total_free + n >= DECOMMIT_TOTAL) {
        decommit(u);
        return;
    }
    if (H.total_free + n > DECOMMIT_TOTAL && n >= 0x200 &&
        (prev_chunk(u) == NIL || next_chunk(u) == NIL)) {
        decommit(u);
        return;
    }
    insert_block(u);
}

/* RtlReAllocateHeap without moving: shrink, or RtlpGrowBlockInPlace. Returns 0 if not possible. */
static int realloc_inplace(uint32_t ptr, uint32_t size)
{
    uint32_t u = chunk_of_ptr(ptr);
    uint32_t n_old = SZ(u);
    uint64_t n = units(size);
    if (n == n_old)
        return 1;
    if (n < n_old) {
        uint32_t rest = n_old - (uint32_t)n;
        if (rest >= 2) {
            set_size(u, n);
            uint32_t r = u + (uint32_t)n;
            add_chunk(r, rest, 0);
            r = coalesce(r);
            insert_block(r);
        }
        return 1;
    }
    if (n > VM_THRESHOLD)
        return 0;
    uint32_t nx = next_chunk(u);
    uint64_t avail;
    if (nx == NIL) {
        /* last entry before uncommitted memory: commit pages right after the block */
        uint64_t end = ADDR(u) + (uint64_t)n_old * UNIT;
        Segment *s = segment_of(ADDR(u));
        uint64_t want = page_up((n - n_old) * UNIT);
        int found = 0;
        for (int i = 0; i < s->nucr; i++) {
            Ucr *r = &s->ucr[i];
            if (r->addr == end && r->size >= want) {
                if (r->size - want <= DECOMMIT_BLOCK * UNIT && r->size < VM_THRESHOLD * UNIT)
                    want = r->size;
                r->addr += want;
                r->size -= want;
                if (r->size == 0)
                    ucr_delete(s, i);
                uint32_t e = unit_of(end);
                add_chunk(e, want / UNIT, 0);
                nx = coalesce(e);
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
        avail = (uint64_t)n_old + SZ(nx);
        if (avail < n) {
            insert_block(nx);
            return 0;
        }
        remove_chunk(nx);
    } else {
        if (is_busy(nx) || (uint64_t)n_old + SZ(nx) < n)
            return 0;
        unlink_block(nx);
        avail = (uint64_t)n_old + SZ(nx);
        remove_chunk(nx);
    }
    uint64_t rest = avail - n;
    if (rest <= 2) {
        set_size(u, avail);
        return 1;
    }
    set_size(u, n);
    uint32_t r = u + (uint32_t)n;
    add_chunk(r, rest, 0);
    r = coalesce(r);
    insert_block(r);
    return 1;
}

static uint32_t h_realloc(uint32_t ptr, uint32_t size)
{
    if (realloc_inplace(ptr, size))
        return ptr;
    uint32_t p = h_alloc(size);
    h_free(ptr);
    return p;
}

/* Heap(base, base + 0x680, base + 0x1000, base + 0x10000, segment_bases, lookaside=False) */
static void heap_reset(uint32_t base, const uint32_t *bases, int nbases)
{
    uint64_t lo = base, hi = (uint64_t)base + 0x10000u, res = SEGMENT_RESERVE;
    for (int i = 0; i < nbases; i++) {
        /* new segments reserve SegmentReserve (doubling) since n * 8 + 4096 <= 1 MiB */
        if (bases[i] < lo)
            lo = bases[i];
        if ((uint64_t)bases[i] + res > hi)
            hi = (uint64_t)bases[i] + res;
        if (res < (1ull << 40))
            res <<= 1;
    }
    if ((base - lo) % UNIT)
        die("misaligned heap base");
    for (int i = 0; i < nbases; i++)
        if ((bases[i] - lo) % UNIT)
            die("misaligned segment base 0x%x", bases[i]);
    if (H.c)
        ENGINE_UNMAP(H.c, H.map_bytes);
    H.lo = lo;
    H.nunits = (hi - lo) / UNIT + 4;
    if (H.nunits >= NIL)
        die("address range too large");
    H.map_bytes = (size_t)H.nunits * sizeof(Cell);
    H.c = ENGINE_MAP(H.map_bytes);
    if (!H.c)
        die("cannot map %zu bytes", H.map_bytes);
    for (unsigned i = 0; i < MAXLIST; i++) {
        H.ded_tail[i] = NIL;
        H.la_n[i] = 0;
        H.ls[i][0] = 4;
        H.ls[i][1] = H.ls[i][2] = H.ls[i][3] = H.ls[i][4] = 0;
    }
    H.ded_bits[0] = H.ded_bits[1] = 0;
    memset(H.f0_head, 0xFF, sizeof H.f0_head);
    memset(H.f0_tail, 0xFF, sizeof H.f0_tail);
    memset(H.f0_bits, 0, sizeof H.f0_bits);
    memset(H.f0_sum, 0, sizeof H.f0_sum);
    H.total_free = 0;
    H.lookaside_on = 0;
    H.decommit_count = 0;
    H.nondedicated = 0;
    H.has_index = 0;
    H.x_large = H.x_limit = H.x_minc = H.x_activity = H.x_ext = 0;
    H.segment_reserve = SEGMENT_RESERVE;
    H.segbases = bases;
    H.nsegbases = nbases;
    H.segbase_pos = 0;
    H.nseg = 0;
    Segment *s = new_segment(base, (uint64_t)base + 0x680, (uint64_t)base + 0x10000);
    add_ucr(s, (uint64_t)base + 0x1000, 0x10000 - 0x1000);
    add_chunk(unit_of(base), 0x680 / UNIT, 1);                    /* HEAP header entry */
    add_chunk(unit_of((uint64_t)base + 0x680), (0x1000 - 0x680) / UNIT, 0);
    insert_block(unit_of((uint64_t)base + 0x680));
}

#endif /* XPHEAP_ENGINE_H */
