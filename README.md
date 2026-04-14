```
bareutils
=========

bareutils are a collection of userspace utilities designed to be almost compatible
with existing utilities like GNU and busybox ones. Built with barelib as well.


Build system:
-------------
All of the bare software uses the GNU Make build system.
In addition to GNU Make, it uses some barelib-provided targets.

Building:
---------
Building bareutils is quite easy, you can:
- `make -j$(nproc)` (Release)
- `make BUILD=debug -j$(nproc)` (Debug)
- `make examples` (Stage examples to OUTDIR)
- `make install` (Install Release)
- `make BUILD=debug install` (Install Debug)

Configuration:
--------------
Configuring bareutils is done on build time. This is done by creating the
`.config.mk` file which is sourced by barelib's configure.mk maketool.

Example `.config.mk`:
---------------------
# disables the `ls` binary from compiling.
$(BINDIR)/ls:
    $(DISABLED)
```
