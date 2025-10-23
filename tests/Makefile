# Test Makefile for Memory Management Unit Tests
CC=gcc
CFLAGS=-Wall -Wextra -std=gnu11 -I../include -g -O0
LDFLAGS=-lm

BUILD_DIR=build
TEST_DIR=unit

# Source files from kernel
KERNEL_SOURCES=../kernel/mm/buddy.c \
               ../kernel/mm/slab.c \
               ../kernel/mm/pool.c \
               ../kernel/mm/cow.c \
               ../kernel/mm/demand_paging.c \
               ../kernel/mm/page_cache.c \
               ../kernel/mm/heap.c

# Test files
TEST_SOURCES=$(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS=$(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%,$(TEST_SOURCES))

.PHONY: all clean test run-tests

all: $(TEST_BINS)

$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(KERNEL_SOURCES)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: all run-tests

run-tests:
	@echo "Running all tests..."
	@for test in $(TEST_BINS); do \
		echo "\n=== Running $$test ==="; \
		./$$test || exit 1; \
	done
	@echo "\n=== All tests passed! ==="

clean:
	rm -rf $(BUILD_DIR)
