CC ?= gcc

CFLAGS   ?= -O2 -g -Wall -Wextra
CPPFLAGS += -I. -I./linux-kernel -I./common -I./dma_read -I./dma_write -I./bar_user
LDFLAGS  ?=

BIN_DIR   := bin
BUILD_DIR := build

MAIN_SRC        := main.c
PCIE_SRC        := common/pcie-device.c
READ_SRC        := dma_read/dma_read.c
WRITE_SRC       := dma_write/dma_write.c
BAR_SRC         := bar_user/bar_user.c
UTILS_SRC       := linux-kernel/tools/dma_utils.c

MAIN_OBJ        := $(BUILD_DIR)/main.o
PCIE_OBJ        := $(BUILD_DIR)/pcie_device.o
READ_OBJ        := $(BUILD_DIR)/dma_read.o
WRITE_OBJ       := $(BUILD_DIR)/dma_write.o
BAR_OBJ         := $(BUILD_DIR)/bar_user.o
UTILS_OBJ       := $(BUILD_DIR)/dma_utils.o
GLOBAL_OBJ      := $(BUILD_DIR)/globals_stub.o

APP_BIN         := $(BIN_DIR)/app_main

all: $(APP_BIN)

$(BIN_DIR) $(BUILD_DIR):
	mkdir -p $@

$(MAIN_OBJ): $(MAIN_SRC) dma_read/dma_read.h dma_write/dma_write.h bar_user/bar_user.h common/pcie-device.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(PCIE_OBJ): $(PCIE_SRC) common/pcie-device.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(READ_OBJ): $(READ_SRC) dma_read/dma_read.h common/pcie-device.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(WRITE_OBJ): $(WRITE_SRC) dma_write/dma_write.h common/pcie-device.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BAR_OBJ): $(BAR_SRC) bar_user/bar_user.h common/pcie-device.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(UTILS_OBJ): $(UTILS_SRC) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/globals_stub.c: | $(BUILD_DIR)
	printf '%s\n' \
	'int eop_flush = 0;' \
	> $@

$(GLOBAL_OBJ): $(BUILD_DIR)/globals_stub.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(APP_BIN): $(MAIN_OBJ) $(PCIE_OBJ) $(READ_OBJ) $(WRITE_OBJ) $(BAR_OBJ) $(UTILS_OBJ) $(GLOBAL_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean