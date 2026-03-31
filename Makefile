CC ?= gcc

CFLAGS   ?= -O2 -g -Wall -Wextra
CPPFLAGS += -I. -I./linux-kernel
LDFLAGS  ?=

BIN_DIR   := bin
BUILD_DIR := build

READ_SRC       := dma_read/dma_read.c
WRITE_SRC      := dma_write/dma_write.c
WRITE_READ_SRC := dma_write_read/dma_write_read.c
UTILS_SRC := linux-kernel/tools/dma_utils.c

READ_OBJ       := $(BUILD_DIR)/dma_read.o
WRITE_OBJ      := $(BUILD_DIR)/dma_write.o
WRITE_READ_OBJ := $(BUILD_DIR)/dma_write_read.o
UTILS_OBJ      := $(BUILD_DIR)/dma_utils.o
GLOBAL_OBJ     := $(BUILD_DIR)/globals_stub.o
STUB_OBJ       := $(BUILD_DIR)/dma_from_device_stub.o

READ_BIN       := $(BIN_DIR)/dma_read
WRITE_BIN      := $(BIN_DIR)/dma_write
WRITE_READ_BIN := $(BIN_DIR)/dma_write_read

all: $(WRITE_READ_BIN)

$(BIN_DIR) $(BUILD_DIR):
	mkdir -p $@

$(WRITE_READ_OBJ): $(WRITE_READ_SRC) dma_write_read/dma_write_read.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(UTILS_OBJ): $(UTILS_SRC) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/globals_stub.c: | $(BUILD_DIR)
	printf '%s\n' \
	'int eop_flush = 0;' \
	> $@

$(GLOBAL_OBJ): $(BUILD_DIR)/globals_stub.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/dma_from_device_stub.c: | $(BUILD_DIR)
	printf '%s\n' \
	'int dma_from_device(const char *device, unsigned long long address, unsigned long long aperture, unsigned long long size, unsigned long long offset, unsigned long long count, const char *ofname) {' \
	'    (void)device; (void)address; (void)aperture; (void)size; (void)offset; (void)count; (void)ofname;' \
	'    return -1;' \
	'}' \
	> $@

$(STUB_OBJ): $(BUILD_DIR)/dma_from_device_stub.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(READ_BIN): $(READ_OBJ) $(UTILS_OBJ) $(GLOBAL_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(WRITE_BIN): $(WRITE_OBJ) $(UTILS_OBJ) $(GLOBAL_OBJ) $(STUB_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(WRITE_READ_BIN): $(WRITE_READ_OBJ) $(UTILS_OBJ) $(GLOBAL_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean