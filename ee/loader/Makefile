EE_BIN = neutrino.elf
EE_BIN_STRIPPED = neutrino_stripped.elf
EE_BIN_PACKED = neutrino_packed.elf
EE_OBJS = main.o patch.o ee_core_config.o ioprp.o
EE_INCS = -I../ee_core/include
EE_LIBS = -lfileXio

$(EE_BIN_STRIPPED): $(EE_BIN)
	$(EE_STRIP) -o $@ $<

$(EE_BIN_PACKED): $(EE_BIN_STRIPPED)
	ps2-packer $< $@ > /dev/null

all: $(EE_BIN_PACKED)


# USB
GAME = 'SCUS_974.81.God of War II.iso'
#GAME = 'SCES_550.19.Ratchet Clank - Size Matters.iso'
#GAME = 'SCUS_971.13.ICO.iso'
#GAME = 'SCUS_971.24.Jak and Daxter - The Precursor Legacy.iso'

#GAME = 'SCES_516.07.Ratchet Clank - Going Commando.iso'
#GAME = 'SCES_520.04.Killzone.iso'
#GAME = 'SCES_524.24.Ace Combat Squadron Leader.iso'
#GAME = 'SCES_524.60.Jak 3.iso'
#GAME = 'SCES_532.85.Ratchet Gladiator.iso'
#GAME = 'SCES_533.26.Shadow of the Colossus.iso'
#GAME = 'SCES_555.73.MotorStorm Arctic Edge.iso'
#GAME = 'SCUS_973.28.Gran Turismo 4.iso'
#GAME = 'SCUS_973.53.Ratchet Clank - Up Your Arsenal.iso'
run: all copy
	ps2client -h 192.168.1.10 execee host:$(EE_BIN_PACKED) usb mass:DVD/$(GAME)

reset: clean
	ps2client -h 192.168.1.10 reset

clean:
	rm -f -r $(EE_OBJS) $(EE_BIN) $(EE_BIN_STRIPPED) $(EE_BIN_PACKED)

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
	cp $(PS2SDK)/iop/irx/usbd.irx             modules
	cp $(PS2SDK)/iop/irx/usbd_mini.irx        modules
	cp $(PS2SDK)/iop/irx/usbmass_bd.irx       modules
	cp $(PS2SDK)/iop/irx/usbmass_bd_mini.irx  modules
	cp $(PS2SDK)/iop/irx/mx4sio_bd.irx        modules
	cp $(PS2SDK)/iop/irx/mx4sio_bd_mini.irx   modules
	cp $(PS2SDK)/iop/irx/ps2dev9.irx          modules
	cp $(PS2SDK)/iop/irx/iLinkman.irx         modules
	cp $(PS2SDK)/iop/irx/IEEE1394_bd.irx      modules
	cp $(PS2SDK)/iop/irx/IEEE1394_bd_mini.irx modules

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal_cpp
