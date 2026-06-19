PROJECT := bareutils
PROJECT_NAMESPACE := bareutils
VERSION := 0.0.0
BUILD := release
BARELIB_PREFIX := barelib
EXTRA_LD := -L$(BARELIB_PREFIX)/out/$(BUILD)/lib -lbarelib
STATIC ?= 0
ifeq ($(STATIC),1)
	EXTRA_LD += -static -static-libgcc
endif
include $(BARELIB_PREFIX)/maketools/config.mk
CFLAGS += -I$(BARELIB_PREFIX)/include -Iinclude -DBAREUTILS_VERSION=\"$(VERSION)\"
EXDIR = src
BIN_SRCS := $(filter-out src/multicall.c, $(wildcard src/*.c))
BINS := $(patsubst src/%.c,$(BINDIR)/%,$(BIN_SRCS))
BINARIES := $(BINS)
.PHONY: all bins multicall clean debug release install uninstall examples
TARGETS := $(BINS)
all: bins
bins: $(TARGETS)
multicall: $(BINDIR)/bareutils
debug release:
	$(MAKE) BUILD=$@ all
include $(BARELIB_PREFIX)/maketools/build.mk
ifeq ($(MULTICALL),1)
all: multicall
BB_OBJDIR := $(OUTDIR)/bb-obj
BB_SRCS := $(BIN_SRCS)
BB_OBJS := $(patsubst src/%.c,$(BB_OBJDIR)/%.o,$(BB_SRCS))
$(BB_OBJDIR)/applets.h: $(BB_SRCS) | $(BB_OBJDIR)
	@echo -e "GEN applets.h"
	@printf '%s\n' '#ifndef APPLETS_H' '#define APPLETS_H' '' > $@
	@for f in $(BB_SRCS); do \
	    name=$$(basename $$f .c); \
	    echo "int $${name}_main(int, char**);"; \
	done >> $@
	@printf '\ntypedef struct {\n    const char *name;\n    int (*fn)(int, char**);\n} applet_t;\n\nstatic const applet_t applets[] = {\n' >> $@
	@for f in $(BB_SRCS); do \
	    name=$$(basename $$f .c); \
	    echo "    { \"$$name\", $${name}_main },"; \
	done >> $@
	@printf '    { NULL, NULL },\n};\n\n#endif\n' >> $@
$(BB_OBJDIR)/%.o: src/%.c | $(BB_OBJDIR)
	@echo -e CC $(ECHO) $<
	@$(CC) $(ACTIVE_CFLAGS) $(FPIC) -Dmain=$*_main -c $< -o $@
$(BB_OBJDIR)/multicall.o: src/multicall.c $(BB_OBJDIR)/applets.h | $(BB_OBJDIR)
	@echo -e CC $(ECHO) $<
	@$(CC) $(ACTIVE_CFLAGS) -I$(BB_OBJDIR) -c $< -o $@
$(BINDIR)/bareutils: $(BB_OBJS) $(BB_OBJDIR)/multicall.o | $(BINDIR)
	@echo -e CC $(ECHO) "multicall -> $@"
	@$(CC) $(ACTIVE_CFLAGS) $^ -o $@ $(ACTIVE_LDFLAGS)
$(BB_OBJDIR):
	@mkdir -p $@
endif
include $(BARELIB_PREFIX)/maketools/install.mk
include $(BARELIB_PREFIX)/maketools/configure.mk
examples:
	@mkdir -p $(OUTDIR)/etc/runit/services/getty-tty1
	@cp -r examples/runit $(OUTDIR)/etc
	@chmod +x $(OUTDIR)/etc/runit/1
	@chmod +x $(OUTDIR)/etc/runit/2
	@chmod +x $(OUTDIR)/etc/runit/3
	@chmod +x $(OUTDIR)/etc/runit/ctrlaltdel
	@chmod +x $(OUTDIR)/etc/runit/services/getty-tty1/run
todos:
	@grep -rni --color "TODO:" src include
