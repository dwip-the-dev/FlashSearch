#ifndef FLASHSEARCH_H
#define FLASHSEARCH_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <immintrin.h>
#include <pthread.h>

#define MAX_THREADS 32
#define MAX_PATTERN 256

typedef struct {
    const char *data;
    size_t start, end;
    volatile size_t pos;
    const char *pattern;
    size_t pattern_len;
    atomic_bool *kill;
    atomic_ullong *scanned;
} Worker;

typedef struct {
    atomic_bool found;
    atomic_size_t position;
    const char *result;
    atomic_ullong cycles_start;
    atomic_ullong cycles_end;
    atomic_ullong bytes_scanned;
} Context;

const char *flashsearch_raw(const char *data, size_t len,
                           const char *pattern, size_t pattern_len,
                           int threads, Context *ctx);

const char *flashsearch_ultimate(const char *data, size_t len,
                                const char *pattern, size_t pattern_len,
                                int threads, Context *ctx);

const char *flashsearch_json(const char *data, size_t len,
                            const char *field, const char *value,
                            int threads, Context *ctx);

const char *flashsearch_json_ultimate(const char *data, size_t len,
                                     const char *field, const char *value,
                                     int is_number, int threads, Context *ctx);

const char *flashsearch_hyper(const char *data, size_t len,
                             const char *pattern, size_t pattern_len,
                             int threads, Context *ctx);

double flashsearch_gbps(const Context *ctx, double ms);
void flashsearch_print(const Context *ctx, double ms, size_t total);

#endif
