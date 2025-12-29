#include "flashsearch.h"
#include <string.h>
#include <x86intrin.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

unsigned long long rdtsc() {
    unsigned int dummy;
    return __rdtscp(&dummy);
}

const char *avx_find(const char *h, size_t hl,
                     const char *n, size_t nl,
                     atomic_ullong *sc,
                     atomic_bool *stop,
                     size_t *bs) {
    if (nl == 0 || nl > hl) return NULL;
    if (nl == 1) return memchr(h, n[0], hl);
    
    unsigned int nf = 0;
    size_t cl = nl < 4 ? nl : 4;
    memcpy(&nf, n, cl);
    
    __m256i fv = _mm256_set1_epi8(n[0]);
    __m256i sv = _mm256_set1_epi8(n[1]);
    
    size_t ii = 0;
    size_t lb = 0;
    size_t cc = 0;
    
    for (; ii + 128 <= hl; ii += 32) {
        if (++cc >= 8) {
            if (stop && atomic_load(stop)) {
                *bs = lb;
                return NULL;
            }
            cc = 0;
        }
        
        __m256i v1 = _mm256_loadu_si256((const __m256i*)(h + ii));
        __m256i v2 = _mm256_loadu_si256((const __m256i*)(h + ii + 32));
        __m256i v3 = _mm256_loadu_si256((const __m256i*)(h + ii + 64));
        __m256i v4 = _mm256_loadu_si256((const __m256i*)(h + ii + 96));
        
        __m256i c1 = _mm256_cmpeq_epi8(v1, fv);
        __m256i c2 = _mm256_cmpeq_epi8(v2, fv);
        __m256i c3 = _mm256_cmpeq_epi8(v3, fv);
        __m256i c4 = _mm256_cmpeq_epi8(v4, fv);
        
        if (nl > 1) {
            __m256i n1 = _mm256_loadu_si256((const __m256i*)(h + ii + 1));
            __m256i n2 = _mm256_loadu_si256((const __m256i*)(h + ii + 33));
            __m256i n3 = _mm256_loadu_si256((const __m256i*)(h + ii + 65));
            __m256i n4 = _mm256_loadu_si256((const __m256i*)(h + ii + 97));
            
            c1 = _mm256_and_si256(c1, _mm256_cmpeq_epi8(n1, sv));
            c2 = _mm256_and_si256(c2, _mm256_cmpeq_epi8(n2, sv));
            c3 = _mm256_and_si256(c3, _mm256_cmpeq_epi8(n3, sv));
            c4 = _mm256_and_si256(c4, _mm256_cmpeq_epi8(n4, sv));
        }
        
        int m1 = _mm256_movemask_epi8(c1);
        int m2 = _mm256_movemask_epi8(c2);
        int m3 = _mm256_movemask_epi8(c3);
        int m4 = _mm256_movemask_epi8(c4);
        
        while (m1) {
            int p = __builtin_ctz(m1);
            size_t idx = ii + p;
            
            unsigned int hf = 0;
            memcpy(&hf, h + idx, cl);
            
            if (hf == nf && memcmp(h + idx, n, nl) == 0) {
                *bs = lb + idx;
                return h + idx;
            }
            
            m1 &= m1 - 1;
        }
        
        while (m2) {
            int p = __builtin_ctz(m2);
            size_t idx = ii + 32 + p;
            
            unsigned int hf = 0;
            memcpy(&hf, h + idx, cl);
            
            if (hf == nf && memcmp(h + idx, n, nl) == 0) {
                *bs = lb + idx;
                return h + idx;
            }
            
            m2 &= m2 - 1;
        }
        
        while (m3) {
            int p = __builtin_ctz(m3);
            size_t idx = ii + 64 + p;
            
            unsigned int hf = 0;
            memcpy(&hf, h + idx, cl);
            
            if (hf == nf && memcmp(h + idx, n, nl) == 0) {
                *bs = lb + idx;
                return h + idx;
            }
            
            m3 &= m3 - 1;
        }
        
        while (m4) {
            int p = __builtin_ctz(m4);
            size_t idx = ii + 96 + p;
            
            unsigned int hf = 0;
            memcpy(&hf, h + idx, cl);
            
            if (hf == nf && memcmp(h + idx, n, nl) == 0) {
                *bs = lb + idx;
                return h + idx;
            }
            
            m4 &= m4 - 1;
        }
        
        lb += 128;
        
        if (ii + 1024 < hl) {
            _mm_prefetch(h + ii + 1024, _MM_HINT_T0);
        }
    }
    
    for (; ii < hl; ii++) {
        if (h[ii] == n[0]) {
            if (memcmp(h + ii, n, nl) == 0) {
                *bs = lb + ii;
                return h + ii;
            }
        }
    }
    
    *bs = lb + hl;
    return NULL;
}

typedef struct {
    Worker *ws;
    int nw;
    atomic_bool found;
    atomic_bool stop;
    const char *res;
    pthread_mutex_t mtx;
} Stealer;

void *worker_no_overlap(void *arg) {
    Worker *w = (Worker*)arg;
    Stealer *s = (Stealer*)w->kill;
    
    cpu_set_t cs;
    CPU_ZERO(&cs);
    static int cid = 0;
    int nc = sysconf(_SC_NPROCESSORS_ONLN);
    CPU_SET(cid++ % nc, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
    
    size_t ch = (w->end - w->start) / 16;
    if (ch < 1024 * 1024) ch = 1024 * 1024;
    
    while (!atomic_load(&s->stop)) {
        size_t ci = __sync_fetch_and_add(&w->pos, 1);
        if (ci >= 16) break;
        
        size_t cs = w->start + ci * ch;
        size_t ce = cs + ch;
        if (ce > w->end) ce = w->end;
        if (cs >= ce) continue;
        
        size_t bs = 0;
        const char *f = avx_find(w->data + cs, 
                                 ce - cs,
                                 w->pattern, w->pattern_len,
                                 w->scanned, 
                                 &s->stop,
                                 &bs);
        
        if (w->scanned) atomic_fetch_add(w->scanned, bs);
        
        if (f) {
            atomic_store(&s->stop, true);
            atomic_store(&s->found, true);
            
            pthread_mutex_lock(&s->mtx);
            if (s->res == NULL) {
                s->res = f;
            }
            pthread_mutex_unlock(&s->mtx);
            
            return (void*)f;
        }
        
        if (atomic_load(&s->found)) {
            return NULL;
        }
    }
    
    return NULL;
}

const char *flashsearch_ultimate_no_overlap(const char *d, size_t l,
                                           const char *p, size_t pl,
                                           int t, Context *ctx) {
    if (t < 1) t = 1;
    if (t > 32) t = 32;
    if (pl == 0 || pl > 256) return NULL;
    
    if (ctx) {
        atomic_store(&ctx->found, false);
        ctx->result = NULL;
        atomic_store(&ctx->bytes_scanned, 0);
        atomic_store(&ctx->cycles_start, rdtsc());
    }
    
    Stealer st;
    atomic_store(&st.found, false);
    atomic_store(&st.stop, false);
    st.res = NULL;
    pthread_mutex_init(&st.mtx, NULL);
    
    Worker ws[32];
    pthread_t pts[32];
    
    size_t ch = l / t;
    
    for (int i = 0; i < t; i++) {
        ws[i].data = d;
        ws[i].pattern = p;
        ws[i].pattern_len = pl;
        ws[i].kill = (atomic_bool*)&st;
        ws[i].scanned = ctx ? &ctx->bytes_scanned : NULL;
        
        ws[i].start = i * ch;
        ws[i].end = (i == t - 1) ? l : (i + 1) * ch;
        
        ws[i].pos = 0;
        
        pthread_create(&pts[i], NULL, worker_no_overlap, &ws[i]);
    }
    
    for (int i = 0; i < t; i++) {
        void *tr;
        pthread_join(pts[i], &tr);
        
        if (tr && st.res == NULL) {
            st.res = (const char*)tr;
        }
    }
    
    pthread_mutex_destroy(&st.mtx);
    
    if (st.res && ctx) {
        atomic_store(&ctx->found, true);
        ctx->result = st.res;
        atomic_store(&ctx->position, st.res - d);
        atomic_store(&ctx->cycles_end, rdtsc());
    }
    
    return st.res;
}

const char *flashsearch_hyper(const char *d, size_t l,
                             const char *p, size_t pl,
                             int t, Context *ctx) {
    return flashsearch_ultimate_no_overlap(d, l, p, pl, t, ctx);
}

const char *flashsearch_ultimate(const char *d, size_t l,
                                const char *p, size_t pl,
                                int t, Context *ctx) {
    return flashsearch_ultimate_no_overlap(d, l, p, pl, t, ctx);
}

double flashsearch_gbps(const Context *ctx, double ms) {
    if (!ctx || ms <= 0) return 0.0;
    unsigned long long b = atomic_load(&ctx->bytes_scanned);
    return (b / (ms / 1000.0)) / 1e9;
}

void flashsearch_print(const Context *ctx, double ms, size_t total) {
    if (!ctx) return;
    
    unsigned long long b = atomic_load(&ctx->bytes_scanned);
    unsigned long long c = atomic_load(&ctx->cycles_end) - atomic_load(&ctx->cycles_start);
    double gb = flashsearch_gbps(ctx, ms);
    
    printf("\n=== RESULTS ===\n");
    printf("Time: %.3f ms\n", ms);
    printf("Scanned: %.1f MB (%.1f%%)\n", b / 1e6, (b * 100.0) / total);
    printf("Speed: %.1f GB/s\n", gb);
    printf("Cycles: %llu\n", c);
    if (b > 0) printf("Cycles/byte: %.1f\n", (double)c / b);
}
