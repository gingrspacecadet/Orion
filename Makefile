CC = gcc
CFLAGS += -Wall -Wextra -g -DDEBUG -Wno-unused-function
# LDFLAGS = $(shell pkg-config --cflags --libs sdl2 SDL2_ttf)

BUILD_DIR = build
SRC_DIR = emu

SRC = $(shell find $(SRC_DIR) -name '*.c')
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

TARGET = build/orion

.PHONY: all clean run crun asm ints bios kernel

all: bios kernel $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

asm: $(BUILD_DIR)
	@$(CC) $(CFLAGS) asm/main.c -o $(BUILD_DIR)/asm

bios: asm
	@./build/asm bios/main.s bios.out

kernel: asm
	@./build/asm kernel/main.s kernel.out

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)

run: all
	@./$(TARGET) kernel.out bios.out

crun: clean run

ints: asm
	@mkdir -p build/ints
	@find ints -type f -print0 | while IFS= read -r -d '' f; do \
		./build/asm "$$f" "build/$$f"; \
	done