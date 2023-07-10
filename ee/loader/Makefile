EE_OBJS = main.o compat.o xparam.o patch.o ee_core_config.o ioprp.o
EE_INCS = -I../ee_core/include
EE_LIBS = -lfileXio -lpatches

EE_NEWLIB_NANO = 1
EE_COMPACT_EXECUTABLE = 1

# Configuration options
BUILTIN_COMMON ?= 0
BUILTIN_DRIVERS ?= 0

# Add embedded files
ifeq ($(BUILTIN_COMMON),1)
EE_BIN_NAME_SUFFIX = _plus
EE_CFLAGS += -DBUILTIN_COMMON
EE_IRX_FILES += \
	cdvdfsv.irx \
	bdm_cdvdman.irx \
	imgdrv.irx \
	isofs.irx \
	resetspu.irx \
	eesync.irx \
	udnl.irx \
	iomanX.irx \
	fileXio.irx \
	bdm.irx \
	bdmfs_fatfs.irx

EE_ELF_FILES += \
	ee_core.elf
endif
ifeq ($(BUILTIN_DRIVERS),1)
EE_BIN_NAME_SUFFIX = _full
EE_CFLAGS += -DBUILTIN_DRIVERS
EE_IRX_FILES += \
	smap.irx \
	ata_bd.irx \
	usbd_mini.irx \
	usbmass_bd_mini.irx \
	mx4sio_bd_mini.irx \
	ps2dev9.irx \
	iLinkman.irx \
	IEEE1394_bd_mini.irx
endif
EE_OBJS += $(addsuffix _irx.o, $(basename $(EE_IRX_FILES)))
EE_OBJS += $(addsuffix _elf.o, $(basename $(EE_ELF_FILES)))

# Where to find the modules
vpath %.irx ../../iop/cdvdfsv/irx/
vpath %.irx ../../iop/cdvdman/irx/
vpath %.irx ../../iop/smap/irx/
vpath %.irx ../../iop/imgdrv/irx/
vpath %.irx ../../iop/isofs/irx/
vpath %.irx ../../iop/resetspu/irx/
vpath %.elf ../ee_core/
vpath %.irx $(PS2SDK)/iop/irx/

# Rule to generate them
%_irx.o: %.irx
	bin2c $< $*_irx.c $*_irx
	mips64r5900el-ps2-elf-gcc -c $*_irx.c -o $*_irx.o
	rm $*_irx.c

%_elf.o: %.elf
	bin2c $< $*_elf.c $*_elf
	mips64r5900el-ps2-elf-gcc -c $*_elf.c -o $*_elf.o
	rm $*_elf.c

EE_BIN_NAME = neutrino$(EE_BIN_NAME_SUFFIX)
EE_BIN = $(EE_BIN_NAME).elf
EE_BIN_PACKED = $(EE_BIN_NAME)_packed.elf

$(EE_BIN_PACKED): $(EE_BIN)
	ps2-packer $< $@ > /dev/null

all: $(EE_BIN_PACKED)


# MODE2 games ?
#GAME ='DVD/Jackie Chan Adventures (E).iso'
#GAME ='DVD/MotorStorm Arctic Edge (E).iso'
#GAME ='DVD/Need for Speed Pro Street (E).iso'
#GAME ='DVD/Ratchet Clank - Going Commando (E).iso'
#GAME ='DVD/Ratchet Clank - Up Your Arsenal (U).iso'

# NOT WORKING!
#GAME ='DVD/Jak 3 (E).iso'
#GAME ='DVD/Prince of Persia Two Thrones (U).iso'
#GAME = 'CD/Quake III Revolution (E).iso'

# UDPBD
GAME = 'CD/ICO (U).iso'
#GAME = 'DVD/DragonBall Z Budokai Tenkaichi 3 (E).iso'
#GAME = 'DVD/Gun (U).iso'
#GAME = 'DVD/Jackie Chan Adventures (E).iso'
#GAME = 'DVD/Jak 3 (E).iso'
run: all copy
	ps2client -h 192.168.1.10 execee host:$(EE_BIN_PACKED) -drv=udpbd -iso=mass:$(GAME)

clean:
	rm -rf $(EE_OBJS) *_irx.o *_elf.o
	rm -rf $(EE_BIN) $(EE_BIN_PACKED)
	rm -rf modules

copy:
	rm -rf modules
	mkdir -p modules
	cp ../../iop/cdvdfsv/irx/cdvdfsv.irx      modules
	cp ../../iop/cdvdman/irx/bdm_cdvdman.irx  modules
	cp ../../iop/smap/irx/smap.irx            modules
	cp ../../iop/imgdrv/irx/imgdrv.irx        modules
	cp ../../iop/isofs/irx/isofs.irx          modules
	cp ../../iop/resetspu/irx/resetspu.irx    modules
	cp ../ee_core/ee_core.elf                 modules
	cp $(PS2SDK)/iop/irx/eesync.irx           modules
	cp $(PS2SDK)/iop/irx/udnl.irx             modules
	cp $(PS2SDK)/iop/irx/iomanX.irx           modules
	cp $(PS2SDK)/iop/irx/fileXio.irx          modules
	cp $(PS2SDK)/iop/irx/bdm.irx              modules
	cp $(PS2SDK)/iop/irx/bdmfs_fatfs.irx      modules
	cp $(PS2SDK)/iop/irx/ata_bd.irx           modules
	cp $(PS2SDK)/iop/irx/usbd_mini.irx        modules
	cp $(PS2SDK)/iop/irx/usbmass_bd_mini.irx  modules
	cp $(PS2SDK)/iop/irx/mx4sio_bd_mini.irx   modules
	cp $(PS2SDK)/iop/irx/ps2dev9.irx          modules
	cp $(PS2SDK)/iop/irx/iLinkman.irx         modules
	cp $(PS2SDK)/iop/irx/IEEE1394_bd_mini.irx modules

include ../../Defs.make
include ../Rules.make
