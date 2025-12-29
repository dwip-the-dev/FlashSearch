CC = gcc
CFLAGS = -O3 -march=native -mtune=native -flto -funroll-loops
CFLAGS += -pthread -mavx2 -mfma
CFLAGS += -D_GNU_SOURCE
CFLAGS += -fomit-frame-pointer
CFLAGS += -Wno-unused-result -Wno-implicit-function-declaration

CFLAGS_EXTREME = -Ofast -march=native -mtune=native -flto -funroll-all-loops
CFLAGS_EXTREME += -pthread -mavx2 -mfma -mbmi -mbmi2 -mlzcnt
CFLAGS_EXTREME += -D_GNU_SOURCE
CFLAGS_EXTREME += -fomit-frame-pointer -fno-stack-protector
CFLAGS_EXTREME += -falign-functions=64 -falign-loops=64 -falign-jumps=64
CFLAGS_EXTREME += -fno-semantic-interposition -fno-plt
CFLAGS_EXTREME += -fno-trapping-math -fassociative-math -freciprocal-math
CFLAGS_EXTREME += -ffinite-math-only -fno-signed-zeros -fno-math-errno
CFLAGS_EXTREME += -funsafe-math-optimizations -ffast-math
CFLAGS_EXTREME += -fprefetch-loop-arrays
CFLAGS_EXTREME += -mprefer-vector-width=256

CFLAGS_DEBUG = -O0 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined

LDFLAGS = -lpthread -lm -lrt
LDFLAGS_EXTREME = -lpthread -lm -lrt -no-pie -Wl,-O3,-z,now
LDFLAGS_DEBUG = -lpthread -lm -lrt -fsanitize=address,undefined

TARGET = flashsearch
SOURCES = flashsearch.c benchmark.c
HEADER = flashsearch.h

CHALLENGE_SOURCES = flashsearch.c challenge.c

.PHONY: all run clean debug extreme profile help

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADER)
	@echo "Building..."
	@$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)
	@echo "Built: $(TARGET)"

run: $(TARGET)
	@echo ""
	@echo "Running..."
	@echo ""
	@./$(TARGET)

extreme: CFLAGS = $(CFLAGS_EXTREME)
extreme: LDFLAGS = $(LDFLAGS_EXTREME)
extreme: $(TARGET)
	@strip --strip-all $(TARGET) 2>/dev/null || true
	@echo "Extreme done!"

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: LDFLAGS = $(LDFLAGS_DEBUG)
debug: $(TARGET)
	@echo "Debug build"

profile:
	@echo "Building profile..."
	@$(CC) $(CFLAGS) -fprofile-generate $(SOURCES) -o $(TARGET)_profile $(LDFLAGS)
	@echo "Profile built: $(TARGET)_profile"

profile-opt:
	@echo "Building with profile..."
	@$(CC) $(CFLAGS) -fprofile-use -fprofile-correction $(SOURCES) -o $(TARGET)_opt $(LDFLAGS)
	@strip --strip-all $(TARGET)_opt 2>/dev/null || true
	@echo "Opt build: $(TARGET)_opt"

challenge: $(CHALLENGE_SOURCES) $(HEADER)
	@echo "Building challenge..."
	@$(CC) $(CFLAGS_EXTREME) $(CHALLENGE_SOURCES) -o flashsearch_challenge $(LDFLAGS_EXTREME)
	@strip --strip-all flashsearch_challenge 2>/dev/null || true
	@echo "Challenge built"

run-challenge: challenge
	@echo ""
	@echo "Running challenge..."
	@echo ""
	@./flashsearch_challenge

clean:
	@rm -f $(TARGET) $(TARGET)_* flashsearch_challenge *.o *.gcda *.gcno *.profdata *.json
	@echo "Cleaned"

help:
	@echo "Commands:"
	@echo "  make           - Build normal"
	@echo "  make run       - Build and run"
	@echo "  make extreme   - Build extreme"
	@echo "  make debug     - Build debug"
	@echo "  make profile   - Build profile"
	@echo "  make profile-opt - Build with profile"
	@echo "  make clean     - Clean all"
	@echo "  make help      - Show help"

$(TARGET): $(SOURCES) $(HEADER)

.INTERMEDIATE: $(TARGET)_profile
