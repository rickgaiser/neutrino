IOP_OBJS = cdvdman.o ioops.o ncmd.o scmd.o searchfile.o streaming.o ioplib_util.o smsutils.o imports.o exports.o
USE_DEV9 ?= 0

ifeq ($(USE_BDM),1)
IOP_BIN  = bdm_cdvdman.irx
IOP_OBJS_DIR = obj.bdm/
IOP_OBJS += device-bdm.o
IOP_CFLAGS += -DBDM_DRIVER
USE_DEV9 = 1
endif

IOP_INCS += -I../common

ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG
endif

ifeq ($(USE_DEV9),1)
IOP_OBJS += dev9.o
IOP_CFLAGS += -D__USE_DEV9
endif

include $(PS2SDK)/Defs.make
include ../Rules.bin.make
include $(PS2SDK)/samples/Makefile.iopglobal
