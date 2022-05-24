# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004.
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

IOP_OBJS_DIR = obj/
IOP_BIN_DIR = bin/
IOP_SRC_DIR = src/
IOP_INC_DIR = include/

IOP_BIN  = $(IOP_BIN_DIR)dev9_dma.irx
INST_BIN = ../compiled_irx/dev9_dma.irx
IOP_OBJS = dev9_dma.o exports.o imports.o
IOP_INCS = -I$(PS2SDK)/iop/include

all: $(IOP_OBJS_DIR) $(IOP_BIN_DIR) $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN_DIR)
	rm -f $(INST_BIN)

$(INST_BIN): $(IOP_BIN)
	cp $(IOP_BIN) ../compiled_irx/
	cp $(IOP_BIN) ../../linux/firmware/ps2/

install: $(INST_BIN)

include $(PS2SDKSRC)/Defs.make
include $(PS2SDKSRC)/iop/Rules.make
include $(PS2SDKSRC)/iop/Rules.release
