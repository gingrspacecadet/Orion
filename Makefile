CC     ?= gcc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -fno-common
LDFLAGS?=
RM     ?= rm -rf

-include template.mk

MODULES := emu

$(foreach mod,$(MODULES),$(eval $(call BUILD_MODULE,$(mod))))

.PHONY: all clean
all: $(MODULE_BINS)

clean:
	$(RM) build