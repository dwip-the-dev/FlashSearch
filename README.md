# ⚡ FlashSearch - Ultra-Fast String Search Engine

**Lightning-fast string searching with AVX2 and multi-threading optimization.** Achieves up to **9 GB/s** search speeds on Google CloudShell!

## 🚀 Performance Highlights

| Test Case | Speed | Notes |
|-----------|-------|-------|
| Full Scan (not found) | **23.2 GB/s** | Optimal case with early rejection |
| Typical Search | **5.5 GB/s** | Real-world pattern matching |
| Early Find | **2.2 GB/s** | Quick termination |
| Single Thread | **4.6 GB/s** | Still highly optimized |

## 📊 Benchmark Results (Google CloudShell)

```
╔══════════════════════════════════╗
║       FLASHSEARCH TEST           ║
╚══════════════════════════════════╝

Configuration:
  Dataset: 10M JSON records (0.69GB)
  Threads: 1-16 optimized scaling
  CPU: CloudShell VM (2.2GHz)
  Storage: NVMe SSD

Best Results:
  • Search Speed: 5.5 GB/s
  • Full Scan: 9.0 GB/s  
  • "Not Found": 23.2 GB/s
  • Optimal Threads: 4-8
```

## 🎯 Features

- **AVX2 SIMD Optimized**: Processes 32 bytes per instruction
- **Multi-threaded**: Scales efficiently across CPU cores
- **Zero-overlap Search**: No redundant scanning between threads
- **Profile-Guided Optimization**: Auto-tunes for your hardware
- **Memory Mapped Files**: Efficient large file handling
- **Early Termination**: Stops immediately when pattern found

## 🛠️ Installation

```bash
# Clone repository
git clone https://github.com/dwip-the-dev/FlashSearch.git
cd FlashSearch

# Build with standard optimizations
make

# Build with extreme optimizations
make extreme

# Build for profiling (PGO)
make profile
./flashsearch_profile  # Run 3-4 times
make profile-opt
```

## 📈 Quick Start

```bash
# Build and run benchmark
make run

# Or run challenge mode
make challenge
make run-challenge
```

## 🏗️ Build Options

| Command | Description |
|---------|-------------|
| `make` | Standard optimized build |
| `make extreme` | Maximum optimizations (AVX2, BMI, etc.) |
| `make debug` | Debug build with sanitizers |
| `make profile` | Profile-guided optimization build |
| `make clean` | Clean all build artifacts |

## 🧠 How It Works

### Core Algorithm
1. **Memory Mapping**: Files are `mmap()`'d for zero-copy access
2. **AVX2 SIMD**: Uses 256-bit registers to compare 32 bytes at once
3. **Thread Pool**: Divides work without overlap between threads
4. **Early Stopping**: All threads stop immediately when pattern found
5. **Cache Optimization**: CPU cache-aware memory access patterns

### Thread Optimization
- Each thread gets non-overlapping chunks
- Sub-divides into 16 sub-chunks for work stealing
- CPU affinity pinning for better cache locality
- Atomic operations for coordination

## 📁 Project Structure

```
FlashSearch/
├── benchmark.c          # Performance test suite
├── challenge.c          # Ultimate challenge mode
├── flashsearch.c       # Core search algorithm
├── flashsearch.h       # Header file with API
├── Makefile           # Build system
└── README.md          # This file
```

## 🔧 API Usage

```c
#include "flashsearch.h"

// Basic search
Context ctx;
const char *result = flashsearch_hyper(
    data, data_len, 
    pattern, pattern_len,
    thread_count, &ctx
);

// Get performance metrics
double speed_gbps = flashsearch_gbps(&ctx, elapsed_ms);
```

## 🏆 Performance Tips

1. **Use 4-8 threads** (optimal for most systems)
2. **Longer patterns** reduce false positives
3. **Run multiple times** to warm CPU caches
4. **Ensure dataset fits in available memory**
5. **Use PGO** for hardware-specific tuning

## 🔬 Technical Details

### AVX2 Implementation
```c
// Processes 128 bytes per iteration (4x AVX2 vectors)
__m256i v1 = _mm256_loadu_si256((const __m256i*)data);
__m256i c1 = _mm256_cmpeq_epi8(v1, pattern_vec);
int mask = _mm256_movemask_epi8(c1);
```

### Memory Access Pattern
- 64-byte cache line aligned reads
- Hardware prefetching hints
- Non-temporal access patterns for large scans

## 📈 Benchmark Patterns Tested

1. **Early Find**: `"key":"key00000123"` (first 0.001%)
2. **Middle Find**: `"id":5000000` 
3. **Late Find**: `"id":9999999` (last record)
4. **Multiple Matches**: `"tag":"tag1234"`
5. **Not Found**: `"nonexistent":"xyz123"` (full scan)

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Submit a pull request
4. Include benchmarks showing improvement

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- Google CloudShell for testing infrastructure
- GCC compiler team for excellent optimizations
- Intel for AVX2 instruction set

---

**Made with ❤️ by [dwip-the-dev](https://github.com/dwip-the-dev)**

*Star this repo if you found it useful!*
