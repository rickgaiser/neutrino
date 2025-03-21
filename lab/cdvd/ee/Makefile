EE_BIN = cdvd.elf
EE_OBJS = main.o
EE_LIBS = -ldebug -lpatches -lxcdvd
EE_CFLAGS += -Wall -Werror

# Add embedded IRX files
EE_IRX_FILES=\
	cdvd.irx
EE_IRX_OBJS = $(addsuffix _irx.o, $(basename $(EE_IRX_FILES)))
EE_OBJS += $(EE_IRX_OBJS)

# Where to find the IRX files
vpath %.irx ../iop

# Rule to generate them
%_irx.o: %.irx
	bin2c $< $*_irx.c $*_irx
	mips64r5900el-ps2-elf-gcc -c $*_irx.c -o $*_irx.o

all: $(EE_BIN)

run: all
	ps2client -h 192.168.1.10 execee host:$(EE_BIN)

sim: all
	flatpak --filesystem=host run net.pcsx2.PCSX2 $(PWD)/$(EE_BIN)

clean:
	rm -f -r $(EE_OBJS) $(EE_BIN) *_irx.c

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
