IOP_OBJS = udnl.o udnl_asm.o imports.o

BIN2C = $(PS2SDK)/bin/bin2c
BIN2S = $(PS2SDK)/bin/bin2s
BIN2O = $(PS2SDK)/bin/bin2o

ifeq ($(DECI2_DEBUG),1)
IOP_CFLAGS += -D__DECI2_DEBUG
IOP_OBJS += IOPRP_DECI2_img.o
else
IOP_OBJS += IOPRP_img.o
endif

%_img.c: %.img
	$(BIN2C) $< $@ IOPRP_img

include ../Defs.make
include ../Rules.bin.make
include ../Rules.make
