IOP_SRC_DIR ?= src/
IOP_INC_DIR ?= src/
IOP_BIN_DIR ?= irx/

IOP_BIN ?= $(shell basename $(CURDIR)).irx
IOP_BIN := $(IOP_BIN:%=$(IOP_BIN_DIR)%)

all:: $(IOP_BIN)

clean::
	rm -f -r $(IOP_OBJS_DIR) $(IOP_BIN_DIR)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal

# include neutrino headers first. Some headers have the same name as ps2sdk ones.
IOP_CFLAGS := -Iinclude -I../common $(IOP_CFLAGS)

ifeq ($(DEBUG),1)
IOP_CFLAGS += -DDEBUG
endif
