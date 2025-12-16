CC = clang
DEBUG = true
OPTIMISE = true
CFLAGS += -Iemu -std=gnu23
ifeq ($(DEBUG), true)
	CFLAGS += -DDEBUG -Wall -Wextra -Werror -Wpedantic -g -Wno-unused-function
endif
ifeq ($(OPTIMISE), true)
	CFLAGS +=  -O3 -flto -funroll-loops -fomit-frame-pointer
endif
LDFLAGS = $(shell pkg-config --cflags --libs sdl2) -lm

BUILD_DIR = build
SRC_DIR = emu

SRC = $(shell find $(SRC_DIR) -name '*.c')
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

TARGET = build/orion

.PHONY: all clean run crun asm ints bios kernel cc

all: bios kernel $(TARGET)

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
	@$(CC) -g cc/cc.c -o build/cc

bios: asm cc
	@build/cc bios/main.c > build/bios.s
	@./build/asm build/bios.s bios.out

kernel: asm cc
	@./build/cc kernel/main.c > build/kernel.s
	@./build/asm build/kernel.s kernel.out

clean:
	@rm -rf $(BUILD_DIR)
	@rm -f $(TARGET)
	@rm -f *.dump *.out
	@rm -f orion.img

run: all
	@./$(TARGET) kernel.out bios.out

crun: clean run