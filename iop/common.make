# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

IOP_BIN ?= $(shell basename $(CURDIR)).irx
IOP_OBJS_DIR = obj/
IOP_SRC_DIR = src/
IOP_BIN_DIR = irx/
IOP_INCS += -Isrc/ -Isrc/include -Iinclude/
all: $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN_DIR)
