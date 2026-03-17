EE_BIN    = cdvd.elf
EE_OBJS   = main.o
EE_LIBS   = -ldebug -lpatches -lxcdvd -liopreboot
EE_CFLAGS += -Wall -Werror

all: $(EE_BIN)

clean:
	rm -f $(EE_OBJS) $(EE_BIN)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
