define BUILD_MODULE
SRCDIR_$(1) := $(1)/src
BUILDDIR_$(1) := build/$(1)
SRCS_$(1) := $$(wildcard $$(SRCDIR_$(1))/*.c)
OBJS_$(1) := $$(patsubst $$(SRCDIR_$(1))/%.c,$$(BUILDDIR_$(1))/%.o,$$(SRCS_$(1)))
BIN_$(1) := $$(BUILDDIR_$(1))/$(1)

MODULE_BINS += $$(BIN_$(1))

.PHONY: $(1)
$(1): $$(BIN_$(1))

$$(BIN_$(1)): $$(OBJS_$(1))
	$$(CC) $$(LDFLAGS) -o $$@ $$^

$$(BUILDDIR_$(1)):
	@mkdir -p $$@

$$(BUILDDIR_$(1))/%.o: $$(SRCDIR_$(1))/%.c | $$(BUILDDIR_$(1))
	$$(CC) $$(CFLAGS) -c -o $$@ $$<
endef