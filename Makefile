.DEFAULT_GOAL := all

CC      := gcc
CFLAGS  ?= -O2 -g -std=c11 -Wall -Wextra -fno-common -Iinclude -MMD -MP -Icommon/include
LDFLAGS ?=

-include template.mk

MODULES := common emu asm
LIBRARY_MODULES := common

$(foreach mod,$(MODULES),$(eval $(call BUILD_MODULE,$(mod))))

ALL_OBJS := $(foreach mod,$(MODULES),$(OBJS_$(mod)))
-include $(ALL_OBJS:.o=.d)

.PHONY: all clean common
all: $(MODULE_BINS)

clean:
	rm -rf build