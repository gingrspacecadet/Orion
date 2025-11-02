CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = $(shell pkg-config --cflags --libs sdl2 SDL2_ttf)

BUILD_DIR = build
SRC_DIR = src

SRC = $(shell find $(SRC_DIR) -name '*.c')
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

TARGET = build/orion

.PHONY: all clean run crun asm ints

all: $(TARGET) ints

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

asm:
	$(CC) $(CFLAGS) asm/main.c -o $(BUILD_DIR)/asm

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)

run: all
	@./$(TARGET) a.out

crun: clean run

ints: asm
	@mkdir -p build/ints
	@find ints -type f -print0 | while IFS= read -r -d '' f; do \
		./build/asm "$$f" "build/$$f"; \
	done