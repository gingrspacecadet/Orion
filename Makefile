CC := gcc
CFLAGS := -ggdb -MMD -MP -Wall -Wextra -Werror -Wno-unused -Iinclude -std=gnu11
LDFLAGS := 

BUILD := build

EXE_TARGETS := emu asm disasm objdump ld
IMG_TARGETS := boot kernel

.PHONY: all clean
all: $(EXE_TARGETS) $(IMG_TARGETS)
run: emu boot kernel
	@$(BUILD)/emu $(BUILD)/boot.bin $(BUILD)/kernel.bin
clean:
	@rm -rf $(BUILD)

$(BUILD)/frag/%.c.o: %.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/frag/%.s.o: %.s $(BUILD)/asm
	@mkdir -p "$(dir $@)"
	$(BUILD)/asm $< $@

define EXE_TEMPLATE
override $(1)_SRC := $$(shell find $(1)/ -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
override $(1)_OBJ := $$(addprefix $$(BUILD)/frag/, $$($(1)_SRC:.c=.c.o))
override $(1)_DEPS := $$(addprefix $$(BUILD)/frag/, $$($(1)_SRC:.c=.c.d))

-include $$($(1)_DEPS)

$(1): $$(BUILD)/$(1)

$$(BUILD)/$(1): $$($(1)_OBJ)
	@mkdir -p "$$(dir $$@)"
	$(CC) $$($(1)_OBJ) $(LDFLAGS) -o $$@

endef

define IMG_TEMPLATE

override $(1)_SRC := $$(shell find $(1)/ -type f -name '*.s' 2>/dev/null | LC_ALL=C sort)
override $(1)_OBJ := $$(addprefix $$(BUILD)/frag/,$$($(1)_SRC:.s=.s.o))

$(1): $$(BUILD)/$(1).bin

$$(BUILD)/$(1).bin: $$($(1)_OBJ) $(BUILD)/ld $(1)/$(1).ld
	@mkdir -p "$$(dir $$@)"
	$(BUILD)/ld $$($(1)_OBJ) -o $$@ -f -T $(1)/$(1).ld

endef

$(foreach t,$(EXE_TARGETS),$(eval $(call EXE_TEMPLATE,$(t))))
$(foreach t,$(IMG_TARGETS),$(eval $(call IMG_TEMPLATE,$(t))))

TEST_SRC := $(shell find tests/ -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
TEST_OBJ := $(addprefix $(BUILD)/frag/,$(TEST_SRC:.c=.c.o))
TEST_EMU_OBJ := $(filter-out $(BUILD)/frag/emu/main.c.o,$(emu_OBJ))

-include $(TEST_OBJ:.o=.d)

$(BUILD)/test_runner: $(TEST_EMU_OBJ) $(TEST_OBJ) tests/tests.ld
	@mkdir -p "$(dir $@)"
	$(CC) $(TEST_EMU_OBJ) $(TEST_OBJ) $(LDFLAGS) -Wl,-T,tests/tests.ld -o $@

.PHONY: test
test: $(BUILD)/test_runner
	./$(BUILD)/test_runner