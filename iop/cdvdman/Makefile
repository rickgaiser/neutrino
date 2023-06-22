IOP_OBJS = cdvdman.o ioops.o ncmd.o scmd.o searchfile.o streaming.o ioplib_util.o smsutils.o imports.o exports.o

ifeq ($(USE_BDM),1)
IOP_BIN  = bdm_cdvdman.irx
IOP_OBJS_DIR = obj.bdm/
IOP_OBJS += device-bdm.o
IOP_CFLAGS += -DBDM_DRIVER
endif

IOP_INCS += -I../common

ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG
endif

include ../Defs.make
include ../Rules.bin.make
include ../Rules.make
