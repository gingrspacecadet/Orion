.SUFFIXES:

define TARGET_TEMPLATE
override $(1)_SRC := $$(shell find $(1)/ -type f -name '*.c' 2>/dev/null | LC_ALL=C sort)
override $(1)_OBJ := $$(addprefix $$(BUILD)/frag/$(1)/, $$(notdir $$($(1)_SRC:.c=.c.o)))
override $(1)_DEPS := $$(addprefix $$(BUILD)/frag/, $$($(1)_SRC:.c=.c.d))

-include $$($(1)_DEPS)

$(1): $$(BUILD)/$(1)

$$(BUILD)/$(1): $$($(1)_OBJ)
	@mkdir -p "$$(dir $$@)"
	$(CC) $(LDFLAGS) $$($(1)_OBJ) -o $$@

$$(BUILD)/frag/$(1)/%.c.o: $(1)/%.c
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) -c $$< -o $$@

endef

CC := gcc -I. -g
CFLAGS := -Wall -Wextra -std=gnu23 -MMD -MP -Wno-unused-function -Wno-sign-compare
LDFLAGS :=

BUILD := build

TARGETS := asm disasm emu

.Phony: all
all: $(TARGETS)

.Phony: clean
clean:
	@rm -rf $(BUILD)

$(foreach t,$(TARGETS),$(eval $(call TARGET_TEMPLATE,$(t))))
