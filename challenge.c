#include "flashsearch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

void *load_file(const char *fn, size_t *sz) {
    int fd = open(fn, O_RDONLY);
    if (fd < 0) return NULL;
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }
    
    *sz = st.st_size;
    void *a = mmap(NULL, *sz, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    return a != MAP_FAILED ? a : NULL;
}

void unload_file(void *x, size_t s) {
    if (x) munmap(x, s);
}

void make_data(const char *fn, long n) {
    printf("Making %ldM...\n", n / 1000000);
    
    FILE *f = fopen(fn, "w");
    if (!f) return;
    
    setvbuf(f, NULL, _IOFBF, 65536);
    
    fprintf(f, "[\n");
    
    for (long i = 0; i < n; i++) {
        fprintf(f, "{\"id\":%ld,\"key\":\"key%08ld\",\"value\":%ld,\"tag\":\"tag%04ld\"}",
                i, i, i * 3, i % 10000);
        
        if (i < n - 1) fprintf(f, ",\n");
        
        if (i % (n / 10) == 0 && i > 0) {
            printf("  Made %ldM\n", i / 1000000);
        }
    }
    
    fprintf(f, "\n]\n");
    fclose(f);
    
    struct stat st;
    stat(fn, &st);
    printf("Made: %s (%.2f GB)\n\n", fn, st.st_size / 1e9);
}

void run_tests(const char *fn, int maxt) {
    struct stat st;
    if (stat(fn, &st) != 0) {
        printf("No file: %s\n", fn);
        return;
    }
    
    size_t fs = st.st_size;
    printf("File: %s (%.2f GB)\n", fn, fs / 1e9);
    
    void *a = load_file(fn, &fs);
    if (!a) {
        printf("Can't load\n");
        return;
    }
    
    printf("\n=== TESTS ===\n\n");
    
    struct {
        const char *pat;
        const char *des;
        int pos;
    } tests[] = {
        {"\"key\":\"key00000123\"", "Early", 0},
        {"\"id\":5000000", "Middle", 1},
        {"\"id\":9999999", "Late", 2},
        {"\"tag\":\"tag1234\"", "Many", 1},
        {"\"nonexistent\":\"xyz123\"", "None", -1},
    };
    
    int ntest = sizeof(tests) / sizeof(tests[0]);
    
    for (int tt = 0; tt < ntest; tt++) {
        printf("Test %d: %s\n", tt + 1, tests[tt].des);
        printf("Pat: %s\n\n", tests[tt].pat);
        
        size_t pl = strlen(tests[tt].pat);
        double best = 0;
        int bestt = 0;
        
        for (int t = 1; t <= maxt; t = t <= 8 ? t * 2 : t + 8) {
            if (t > maxt) t = maxt;
            
            printf("  %2d t: ", t);
            fflush(stdout);
            
            Context ctx;
            struct timespec s, e;
            
            volatile char *cc = malloc(10000000);
            if (cc) {
                for (int i = 0; i < 10000000; i += 64) cc[i] = i;
                free((void*)cc);
            }
            
            clock_gettime(CLOCK_MONOTONIC, &s);
            
            const char *r = flashsearch_hyper((const char*)a, fs,
                                                  tests[tt].pat, pl,
                                                  t, &ctx);
            
            clock_gettime(CLOCK_MONOTONIC, &e);
            
            double ms = (e.tv_sec - s.tv_sec) * 1000.0 +
                       (e.tv_nsec - s.tv_nsec) / 1e6;
            
            double gb = flashsearch_gbps(&ctx, ms);
            uint64_t sc = atomic_load(&ctx.bytes_scanned);
            
            printf("%6.1f ms, %5.1f GB/s", ms, gb);
            
            if (r) {
                printf(" ✓ Found\n");
            } else {
                printf(" ✗ Not (%.1fMB)\n", sc / 1e6);
            }
            
            if (gb > best) {
                best = gb;
                bestt = t;
            }
            
            usleep(30000);
        }
        
        printf("  Best: %.1f GB/s with %d t\n\n", best, bestt);
    }
    
    printf("=== FULL ===\n");
    
    const char *fp = "\"impossible\":\"pattern\"";
    size_t fpl = strlen(fp);
    
    int ot = maxt / 2;
    if (ot < 4) ot = 4;
    
    printf("Scan all with %d t...\n", ot);
    
    Context ctx;
    struct timespec s, e;
    clock_gettime(CLOCK_MONOTONIC, &s);
    
    flashsearch_hyper((const char*)a, fs, fp, 
                     fpl, ot, &ctx);
    
    clock_gettime(CLOCK_MONOTONIC, &e);
    
    double ms = (e.tv_sec - s.tv_sec) * 1000.0 +
               (e.tv_nsec - s.tv_nsec) / 1e6;
    
    double gb = flashsearch_gbps(&ctx, ms);
    uint64_t sc = atomic_load(&ctx.bytes_scanned);
    
    printf("Full: %.1f ms\n", ms);
    printf("Speed: %.1f GB/s\n", gb);
    printf("Bytes: %.1f MB (%.1f%%)\n", 
           sc / 1e6, (sc > fs ? 100.0 : (sc * 100.0) / fs));
    
    printf("\n=== RATE ===\n");
    
    if (gb >= 15.0) {
        printf("🚀🚀🚀 WOW!\n");
    } else if (gb >= 10.0) {
        printf("🚀🚀 NICE\n");
    } else if (gb >= 6.0) {
        printf("🚀 OKAY\n");
    } else if (gb >= 4.0) {
        printf("⚡ DECENT\n");
    } else if (gb >= 2.0) {
        printf("✅ FINE\n");
    } else {
        printf("📊 MEH\n");
    }
    
    unload_file(a, fs);
}

int main() {
    printf("\n");
    printf("╔══════════════════════════════╗\n");
    printf("║      CHALLENGE MODE          ║\n");
    printf("╚══════════════════════════════╝\n");
    printf("\n");
    
    const char *fn = "data.json";
    long n = 10000000;
    int maxt = 16;
    
    printf("Setup:\n");
    printf("  File: %s\n", fn);
    printf("  Recs: %ld mil\n", n / 1000000);
    printf("  Size: ~1GB\n");
    printf("  Max t: %d\n", maxt);
    printf("\n");
    
    struct stat st;
    if (stat(fn, &st) != 0) {
        printf("Making...\n");
        make_data(fn, n);
    } else {
        printf("Use old...\n");
    }
    
    run_tests(fn, maxt);
    
    printf("\n");
    printf("══════════════════════════════\n");
    printf("          FINISHED            \n");
    printf("══════════════════════════════\n");
    printf("\n");
    printf("Hints:\n");
    printf("  1. 4-8 threads\n");
    printf("  2. Long pat better\n");
    printf("  3. Run again warm\n");
    printf("  4. Fit RAM\n");
    printf("\n");
    
    return 0;
}
