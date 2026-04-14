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
BIN_SRCS := $(wildcard src/*.c)
BINS := $(patsubst src/%.c,$(BINDIR)/%,$(BIN_SRCS))
BINARIES := $(BINS)
.PHONY: all bins clean debug release install uninstall examples
TARGETS := $(BINS)
all: bins
bins: $(TARGETS)
debug release:
	$(MAKE) BUILD=$@ all
include $(BARELIB_PREFIX)/maketools/build.mk
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
