IOP_OBJS = cdvdman.o ioops.o ncmd.o scmd.o searchfile.o streaming.o ioplib_util.o smsutils.o imports.o exports.o

ifeq ($(USE_BDM),1)
IOP_BIN  = $(IOP_BIN_DIR)bdm_cdvdman.irx
IOP_OBJS_DIR = obj.bdm/
IOP_OBJS += device-bdm.o
IOP_CFLAGS += -DBDM_DRIVER
IOP_LIBS += -L$(PS2SDK)/iop/lib -lbdm
endif

ifeq ($(USE_FILE),1)
IOP_BIN  = $(IOP_BIN_DIR)file_cdvdman.irx
IOP_OBJS_DIR = obj.file/
IOP_OBJS += device-file.o
IOP_CFLAGS += -DFILE_DRIVER
endif

IOP_INCS += -I../common

ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG
endif

include ../common.make
include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal

