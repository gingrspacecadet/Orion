CC = clang
DEBUG = true
CFLAGS += -Wall -Wextra -Werror -Wpedantic -g -Wno-unused-function -Iemu -std=gnu23 -O3
ifeq ($(DEBUG), true)
	CFLAGS += -DDEBUG
endif
LDFLAGS = $(shell pkg-config --cflags --libs sdl2) -lm

BUILD_DIR = build
SRC_DIR = emu

SRC = $(shell find $(SRC_DIR) -name '*.c')
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

TARGET = build/orion

.PHONY: all clean run crun asm ints bios kernel cc

all: cc bios kernel $(TARGET)

$(TARGET): $(OBJ)
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

asm: $(BUILD_DIR)
	@$(CC) $(CFLAGS) asm/main.c -o $(BUILD_DIR)/asm

cc: $(BUILD_DIR)
	@$(MAKE) --no-print-directory -s -C cc
	@cd cc && ./ccomp test.c

bios: asm
	@./build/asm bios/main.s bios.out > /dev/null

kernel: asm
	@./build/asm cc/out.s kernel.out  > /dev/null

clean:
	@rm -rf $(BUILD_DIR)
	@rm -f $(TARGET)
	@rm -f *.dump *.out

run: all
	@./$(TARGET) kernel.out bios.out

crun: clean run

ints: asm
	@mkdir -p build/ints
	@find ints -type f -print0 | while IFS= read -r -d '' f; do \
		./build/asm "$$f" "build/$$f"; \
	done