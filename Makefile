.SUFFIXES:

include template.mk

CC := gcc -I.
CFLAGS := -Wall -Wextra -std=gnu23 -MMD -MP -Wno-unused-function -Wno-sign-compare
LDFLAGS :=

BUILD := build

TARGETS := asm disasm emu

.Phony: all
all: $(TARGETS)

.Phony: clean
clean:
	@rm -rf $(BUILD)

$(foreach t,$(TARGETS),$(eval $(call program_TEMPLATE,$(t))))
