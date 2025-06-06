# Copyright 2009-2010, Ifcaro, jimmikaelkael & Polo
# Copyright 2006-2008 Polo
# Licenced under Academic Free License version 3.0
# Review OPNPS2LD README & LICENSE files for further details.
#
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Standard Makefile

EE_BIN = ee_core.elf
EE_SRC_DIR = src/
EE_OBJS_DIR = obj/

GSMCORE_EE_OBJS = gsm_api.o ee_exception_l2.o
CHEATCORE_EE_OBJS = cheat_engine.o cheat_api.o

EE_OBJS = main.o iopmgr.o loadfile.o util.o patches.o patches_asm.o \
	  asm.o crt0.o $(GSMCORE_EE_OBJS) $(CHEATCORE_EE_OBJS)
MAPFILE = ee_core.map

EE_INCS := -I. -Iinclude -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(GSKIT)/include
EE_CFLAGS = -D_EE -Os -G0 -Wall -Werror -fdata-sections -ffunction-sections $(EE_INCS)

ifeq ($(EESIO_DEBUG),1)
EE_CFLAGS += -D__EESIO_DEBUG
endif

ifeq ($(DTL_T10000),1)
	EE_CFLAGS += -D_DTL_T10000
endif

ifeq ($(LOAD_DEBUG_MODULES),1)
EE_CFLAGS += -D__LOAD_DEBUG_MODULES
	ifeq ($(DECI2_DEBUG),1)
	EE_CFLAGS += -D__DECI2_DEBUG
	endif
endif

EE_OBJS := $(EE_OBJS:%=$(EE_OBJS_DIR)%)

EE_LDFLAGS = -nostartfiles -nostdlib -Tlinkfile -L$(PS2SDK)/ee/lib -s -Wl,-Map,$(MAPFILE) -Wl,-zmax-page-size=128 -Wl,--gc-sections
EE_LIBS += -lpatches

EE_LIBS += -lkernel -lgcc

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.c
	@mkdir -p $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.S
	@mkdir -p $(EE_OBJS_DIR)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_OBJS_DIR)%.o : $(EE_SRC_DIR)%.s
	@mkdir -p $(EE_OBJS_DIR)
	$(EE_AS) $(EE_ASFLAGS) $< -o $@

$(EE_BIN) : $(EE_OBJS)
	$(EE_CC) $(EE_CFLAGS) $(EE_LDFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LIBS)

all: $(EE_BIN)

clean:
	rm -f -r $(EE_OBJS_DIR) $(EE_BIN) $(MAPFILE)

clean_all:
	rm -f -r $(EE_OBJS_DIR) $(EE_BIN) $(MAPFILE)

include $(PS2SDK)/samples/Makefile.pref
