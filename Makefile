CC := gcc
CFLAGS := -ggdb -MMD -MP -Wall -Wextra -Werror -Wno-unused -Iinclude -std=gnu11
LDFLAGS := 

BUILD := build

TARGETS := emu asm disasm objdump ld

.PHONY: all clean
all: $(TARGETS) boot
run: emu boot
	@$(BUILD)/emu $(BUILD)/boot.bin
clean:
	@rm -rf $(BUILD)

BOOT_SRC := $(shell find boot/ -type f -name '*.s' 2>/dev/null | LC_ALL=C sort)
BOOT_OBJ := $(addprefix $(BUILD)/frag/,$(BOOT_SRC:.s=.s.o))

boot: $(BUILD)/boot.bin

$(BUILD)/frag/%.s.o: %.s asm
	@mkdir -p "$(dir $@)"
	$(BUILD)/asm $< $@

$(BUILD)/boot.bin: $(BOOT_OBJ) ld
	@mkdir -p "$(dir $@)"
	$(BUILD)/ld $(BOOT_OBJ) -o $@ -f -T boot/boot.ld

$(BUILD)/frag/%.c.o: %.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c $< -o $@

define TARGET_TEMPLATE
override $(1)_SRC := $$(shell find $(1)/ -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
override $(1)_OBJ := $$(addprefix $$(BUILD)/frag/$(1)/, $$(notdir $$($(1)_SRC:.c=.c.o)))
override $(1)_DEPS := $$(addprefix $$(BUILD)/frag/, $$($(1)_SRC:.c=.c.d))

-include $$($(1)_DEPS)

$(1): $$(BUILD)/$(1)

$$(BUILD)/$(1): $$($(1)_OBJ)
	@mkdir -p "$$(dir $$@)"
	$(CC) $$($(1)_OBJ) $(LDFLAGS) -o $$@

endef

$(foreach t,$(TARGETS),$(eval $(call TARGET_TEMPLATE,$(t))))

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