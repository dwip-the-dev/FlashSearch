#include "flashsearch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

void *load_file(const char *fname, size_t *sz) {
    int fd = open(fname, O_RDONLY);
    if (fd < 0) return NULL;
    
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }
    
    *sz = st.st_size;
    void *addr = mmap(NULL, *sz, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    return addr != MAP_FAILED ? addr : NULL;
}

void unload_file(void *a, size_t s) {
    if (a) munmap(a, s);
}

void make_data(const char *fname, long num) {
    printf("Making %ldM records...\n", num / 1000000);
    
    FILE *f = fopen(fname, "w");
    if (!f) return;
    
    setvbuf(f, NULL, _IOFBF, 65536);
    
    fprintf(f, "[\n");
    
    for (long i = 0; i < num; i++) {
        fprintf(f, "{\"id\":%ld,\"key\":\"key%08ld\",\"value\":%ld,\"tag\":\"tag%04ld\"}",
                i, i, i * 3, i % 10000);
        
        if (i < num - 1) fprintf(f, ",\n");
        
        if (i % (num / 10) == 0 && i > 0) {
            printf("  Made %ldM\n", i / 1000000);
        }
    }
    
    fprintf(f, "\n]\n");
    fclose(f);
    
    struct stat st;
    stat(fname, &st);
    printf("File: %s (%.2f GB)\n\n", fname, st.st_size / 1e9);
}

void run_tests(const char *fname, int maxth) {
    struct stat st;
    if (stat(fname, &st) != 0) {
        printf("No file: %s\n", fname);
        return;
    }
    
    size_t fsize = st.st_size;
    printf("File: %s (%.2f GB)\n", fname, fsize / 1e9);
    
    void *addr = load_file(fname, &fsize);
    if (!addr) {
        printf("Can't load\n");
        return;
    }
    
    printf("\n=== TESTS ===\n\n");
    
    struct {
        const char *patt;
        const char *desc;
        int where;
    } tests[] = {
        {"\"key\":\"key00000123\"", "Find early", 0},
        {"\"id\":5000000", "Find middle", 1},
        {"\"id\":9999999", "Find late", 2},
        {"\"tag\":\"tag1234\"", "Find many", 1},
        {"\"nonexistent\":\"xyz123\"", "Find none", -1},
    };
    
    int ntests = sizeof(tests) / sizeof(tests[0]);
    
    for (int t = 0; t < ntests; t++) {
        printf("Test %d: %s\n", t + 1, tests[t].desc);
        printf("Pattern: %s\n\n", tests[t].patt);
        
        size_t plen = strlen(tests[t].patt);
        double best = 0;
        int bestt = 0;
        
        for (int th = 1; th <= maxth; th = th <= 8 ? th * 2 : th + 8) {
            if (th > maxth) th = maxth;
            
            printf("  %2d th: ", th);
            fflush(stdout);
            
            Context ctx;
            struct timespec s, e;
            
            volatile char *c = malloc(10000000);
            if (c) {
                for (int i = 0; i < 10000000; i += 64) c[i] = i;
                free((void*)c);
            }
            
            clock_gettime(CLOCK_MONOTONIC, &s);
            
            const char *res = flashsearch_hyper((const char*)addr, fsize,
                                                  tests[t].patt, plen,
                                                  th, &ctx);
            
            clock_gettime(CLOCK_MONOTONIC, &e);
            
            double ms = (e.tv_sec - s.tv_sec) * 1000.0 +
                       (e.tv_nsec - s.tv_nsec) / 1e6;
            
            double gbps = flashsearch_gbps(&ctx, ms);
            uint64_t scanned = atomic_load(&ctx.bytes_scanned);
            
            printf("%6.1f ms, %5.1f GB/s", ms, gbps);
            
            if (res) {
                printf(" ✓ Found\n");
            } else {
                printf(" ✗ Not found (%.1fMB)\n", scanned / 1e6);
            }
            
            if (gbps > best) {
                best = gbps;
                bestt = th;
            }
            
            usleep(30000);
        }
        
        printf("  Best: %.1f GB/s with %d th\n\n", best, bestt);
    }
    
    printf("=== FULL SCAN ===\n");
    
    const char *fullpatt = "\"impossible\":\"pattern\"";
    size_t fullplen = strlen(fullpatt);
    
    int optth = maxth / 2;
    if (optth < 4) optth = 4;
    
    printf("Scan all with %d th...\n", optth);
    
    Context ctx;
    struct timespec s, e;
    clock_gettime(CLOCK_MONOTONIC, &s);
    
    flashsearch_hyper((const char*)addr, fsize, fullpatt, 
                     fullplen, optth, &ctx);
    
    clock_gettime(CLOCK_MONOTONIC, &e);
    
    double ms = (e.tv_sec - s.tv_sec) * 1000.0 +
               (e.tv_nsec - s.tv_nsec) / 1e6;
    
    double gbps = flashsearch_gbps(&ctx, ms);
    uint64_t scanned = atomic_load(&ctx.bytes_scanned);
    
    printf("Full: %.1f ms\n", ms);
    printf("Speed: %.1f GB/s\n", gbps);
    printf("Bytes: %.1f MB (%.1f%%)\n", 
           scanned / 1e6, (scanned > fsize ? 100.0 : (scanned * 100.0) / fsize));
    
    printf("\n=== RATING ===\n");
    
    if (gbps >= 15.0) {
        printf("🚀🚀🚀 LEGENDARY!\n");
    } else if (gbps >= 10.0) {
        printf("🚀🚀 GREAT\n");
    } else if (gbps >= 6.0) {
        printf("🚀 GOOD\n");
    } else if (gbps >= 4.0) {
        printf("⚡ OK\n");
    } else if (gbps >= 2.0) {
        printf("✅ FINE\n");
    } else {
        printf("📊 MEH\n");
    }
    
    unload_file(addr, fsize);
}

int main() {
    printf("\n");
    printf("╔══════════════════════════════════╗\n");
    printf("║       FLASHSEARCH TEST           ║\n");
    printf("╚══════════════════════════════════╝\n");
    printf("\n");
    
    const char *fname = "data.json";
    long num = 10000000;
    int maxth = 16;
    
    printf("Config:\n");
    printf("  File: %s\n", fname);
    printf("  Records: %ld mil\n", num / 1000000);
    printf("  Size: ~1GB\n");
    printf("  Max th: %d\n", maxth);
    printf("\n");
    
    struct stat st;
    if (stat(fname, &st) != 0) {
        printf("Making data...\n");
        make_data(fname, num);
    } else {
        printf("Using old data...\n");
    }
    
    run_tests(fname, maxth);
    
    printf("\n");
    printf("══════════════════════════════════\n");
    printf("            DONE                  \n");
    printf("══════════════════════════════════\n");
    printf("\n");
    printf("Tips:\n");
    printf("  1. Use 4-8 threads\n");
    printf("  2. Long patterns better\n");
    printf("  3. Run again to warm\n");
    printf("  4. Fit in RAM\n");
    printf("\n");
    
    return 0;
}
