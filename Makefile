CC ?= gcc
CFLAGS ?= -O3 -march=native -flto -std=c11 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS ?= -flto
LDLIBS ?= -lm

BIN_DIR := bin
OBJ_DIR := obj

COMMON_OBJ := $(OBJ_DIR)/expm1_fixed.o

all: $(BIN_DIR)/accuracy $(BIN_DIR)/bench_perf

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

$(OBJ_DIR)/expm1_fixed.o: src/expm1_fixed.c include/expm1_fixed.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/accuracy: src/accuracy.c $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BIN_DIR)/bench_perf: src/bench_perf.c $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -fno-builtin-expm1f $^ $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

.PHONY: all clean
