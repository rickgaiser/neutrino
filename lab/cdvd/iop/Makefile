IOP_BIN  = cdvd.irx
IOP_OBJS = main.o imports.o

IOP_CFLAGS += -Wall

all: $(IOP_BIN)

clean:
	rm -f -r $(IOP_OBJS) $(IOP_BIN)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
