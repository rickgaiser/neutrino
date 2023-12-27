# Enable to disable initialization of SMAP within DEV9 (i.e. clone adaptor compatibility).
DEV9_SKIP_SMAP_INIT ?= 0
# Enable to enable support for the PSX's devices
DEV9_PSX_SUPPORT ?= 1
# Disable to disable the workaround (skip SMAP initialization) for bad GameStar compatible adaptors.
DEV9_GAMESTAR_WORKAROUND ?= 1

# Enable this to hide the module from games, needed by neutrino
DEV9_HIDDEN ?= 0
# Enable this to disable shutting down the DEV9, needed by neutrino
DEV9_NO_SHUTDOWN ?= 0

IOP_OBJS = ps2dev9.o exports.o imports.o

ifeq ($(DEV9_SKIP_SMAP_INIT),1)
IOP_CFLAGS += -DDEV9_SKIP_SMAP_INIT=1
endif

ifeq ($(DEV9_PSX_SUPPORT),1)
IOP_CFLAGS += -DDEV9_PSX_SUPPORT=1
endif

ifeq ($(DEV9_GAMESTAR_WORKAROUND),1)
IOP_CFLAGS += -DDEV9_GAMESTAR_WORKAROUND=1
endif

ifeq ($(DEV9_HIDDEN),1)
IOP_CFLAGS += -DDEV9_NO_DEV9X
IOP_OBJS_DIR = obj.hidden/
IOP_BIN = dev9_hidden.irx
else ifeq ($(DEV9_NO_SHUTDOWN),1)
IOP_CFLAGS += -DDEV9_NO_SHUTDOWN
IOP_OBJS_DIR = obj.ns/
IOP_BIN = dev9_ns.irx
endif

include ../../Defs.make
include ../Rules.bin.make
include ../Rules.make
