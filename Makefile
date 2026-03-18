CC = clang
DEBUG = true
OPTIMISE = true
CFLAGS += -Iemu -std=gnu23
ifeq ($(DEBUG), true)
	CFLAGS += -DDEBUG -g -Wno-unused-function
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

.PHONY: all clean run crun asm ints bios kernel

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


bios: asm
	@./build/asm bios/main.s bios.out

kernel: asm
	@./build/asm kernel/main.s kernel.out

clean:
	@rm -rf $(BUILD_DIR)
	@rm -f $(TARGET)
	@rm -f *.dump *.out
	@rm -f orion.img

run: all
	@./$(TARGET) kernel.out bios.out

crun: clean run