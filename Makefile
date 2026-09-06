.SUFFIXES:

define TARGET_TEMPLATE
override $(1)_SRC := $$(shell find $(1)/ -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
override $(1)_OBJ := $$(addprefix $$(BUILD)/frag/$(1)/, $$(notdir $$($(1)_SRC:.c=.c.o)))
override $(1)_DEPS := $$(addprefix $$(BUILD)/frag/, $$($(1)_SRC:.c=.c.d))

-include $$($(1)_DEPS)

$(1): $$(BUILD)/$(1)

$$(BUILD)/$(1): $$($(1)_OBJ)
	@mkdir -p "$$(dir $$@)"
	$(CC) $$($(1)_OBJ) $(LDFLAGS) -o $$@

$$(BUILD)/frag/$(1)/%.c.o: $(1)/%.c
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) -c $$< -o $$@

endef

CC := gcc
CFLAGS := -Wall -Wextra -std=gnu23 -MMD -MP -Wno-sign-compare -Wno-unused -Wno-override-init -Iinclude $(shell pkg-config --cflags sdl2)
LDFLAGS := $(shell pkg-config --libs sdl2)

# optimisations
ifeq ($(DEBUG),1)
    CFLAGS += -DDEBUG -g -O0
    LDFLAGS += -g
else
    OPT_FLAGS := -O3 -march=native -flto=auto -fno-plt -fomit-frame-pointer
    
    ifeq ($(PGO),gen)
        OPT_FLAGS += -fprofile-generate
    else ifeq ($(PGO),use)
        OPT_FLAGS += -fprofile-use -fprofile-correction
    endif

    CFLAGS += $(OPT_FLAGS)
    LDFLAGS += $(OPT_FLAGS) -s
endif

BUILD := build

TARGETS := asm disasm emu ld

.PHONY: all
all: $(TARGETS) boot

override BOOT_SRC := $(shell find boot/ -type f -name '*.s' 2>/dev/null | LC_ALL=C sort)
override BOOT_OBJ_ALL := $(addprefix $(BUILD)/boot/, $(notdir $(BOOT_SRC:.s=.o)))
override BOOT_MAIN := $(BUILD)/boot/main.o
override BOOT_OBJ := $(if $(filter $(BOOT_MAIN),$(BOOT_OBJ_ALL)),$(BOOT_MAIN) $(filter-out $(BOOT_MAIN),$(BOOT_OBJ_ALL)),$(BOOT_OBJ_ALL))

.PHONY: boot
boot: $(BUILD)/boot/boot

$(BUILD)/boot:
	@mkdir -p "$@"

$(BUILD)/boot/%.o: boot/%.s | $(BUILD)/boot $(BUILD)/asm
	@mkdir -p "$(dir $@)"
	$(BUILD)/asm $< $@

$(BUILD)/boot/boot: $(BOOT_OBJ) | $(BUILD)/ld
	@mkdir -p "$(dir $@)"
	$(BUILD)/ld $(BOOT_OBJ) -o $@ -T boot/boot.ld

.PHONY: run
run: boot emu
	$(BUILD)/emu $(BUILD)/boot/boot

.PHONY: clean
clean:
	@rm -rf $(BUILD)

$(foreach t,$(TARGETS),$(eval $(call TARGET_TEMPLATE,$(t))))