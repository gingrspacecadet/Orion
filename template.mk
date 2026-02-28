define BUILD_MODULE
SRCDIR_$(1) := $(1)/src
LIBDIR_$(1) := $(1)/lib
BUILDDIR_$(1) := build/$(1)
BUILDLIBDIR_$(1) := build/$(1)/lib

# recursively gather C sources, including subdirectories
SRCS_$(1) := $$(shell find $$(SRCDIR_$(1)) -name '*.c' 2>/dev/null)
LIBSRCS_$(1) := $$(shell find $$(LIBDIR_$(1)) -name '*.c' 2>/dev/null)

OBJS_$(1) := $$(patsubst $$(SRCDIR_$(1))/%.c,$$(BUILDDIR_$(1))/%.o,$$(SRCS_$(1)))
LIBOBJS_$(1) := $$(patsubst $$(LIBDIR_$(1))/%.c,$$(BUILDLIBDIR_$(1))/%.o,$$(LIBSRCS_$(1)))

LIB_$(1) := $$(BUILDDIR_$(1))/lib$(1).a
BIN_$(1) := $$(BUILDDIR_$(1))/$(1)

CFLAGS_$(1) := -I$(1)/include

ifneq ($$(strip $$(SRCS_$(1))),)
MODULE_BINS += $$(BIN_$(1))
endif

.PHONY: $(1)
ifneq ($$(strip $$(SRCS_$(1))),)
$(1): $$(BIN_$(1))
else
$(1): $$(LIB_$(1))
endif

ifneq ($$(strip $$(OBJS_$(1)) $$(LIBOBJS_$(1))),)
ifneq ($$(strip $$(LIBOBJS_$(1))),)
$$(BIN_$(1)): $$(OBJS_$(1)) $$(LIB_$(1)) $$(foreach lib,$$(LIBRARY_MODULES),$$(if $$(filter-out $(1),$$(lib)),$$(BUILDDIR_$$(lib))/lib$$(lib).a))
	$$(CC) $$(LDFLAGS) $$(CFLAGS_$(1)) -o $$@ $$(OBJS_$(1)) $$(LIB_$(1)) $$(foreach lib,$$(LIBRARY_MODULES),$$(if $$(filter-out $(1),$$(lib)),$$(BUILDDIR_$$(lib))/lib$$(lib).a))
else
$$(BIN_$(1)): $$(OBJS_$(1)) $$(foreach lib,$$(LIBRARY_MODULES),$$(if $$(filter-out $(1),$$(lib)),$$(BUILDDIR_$$(lib))/lib$$(lib).a))
	$$(CC) $$(LDFLAGS) $$(CFLAGS_$(1)) -o $$@ $$^
endif
else
$$(BIN_$(1)):
	@echo "Nothing to build for module $(1)"
endif

ifneq ($$(strip $$(LIBOBJS_$(1))),)
$$(LIB_$(1)): $$(LIBOBJS_$(1))
	@mkdir -p $$(BUILDDIR_$(1))
	ar rcs $$@ $$^
else
$$(LIB_$(1)):
	@true
endif

$$(BUILDDIR_$(1)):
	@mkdir -p $$@

$$(BUILDLIBDIR_$(1)):
	@mkdir -p $$@

ifneq ($$(strip $$(SRCS_$(1))),)
$$(BUILDDIR_$(1))/%.o: $$(SRCDIR_$(1))/%.c | $$(BUILDDIR_$(1))
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) $$(CFLAGS_$(1)) -c -o $$@ $$<
endif

ifneq ($$(strip $$(LIBSRCS_$(1))),)
$$(BUILDLIBDIR_$(1))/%.o: $$(LIBDIR_$(1))/%.c | $$(BUILDLIBDIR_$(1))
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) $$(CFLAGS_$(1)) -c -o $$@ $$<
endif
endef
