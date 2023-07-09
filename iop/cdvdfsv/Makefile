IOP_OBJS = cdvdfsv.o searchfile.o ncmd.o scmd.o imports.o exports.o

IOP_INCS += -I../common

ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG
endif

include ../../Defs.make
include ../Rules.bin.make
include ../Rules.make
