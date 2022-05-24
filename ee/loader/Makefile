EE_BIN = modloader.elf
EE_OBJS = modloader.o patch.o ee_core_config.o ioprp.o
EE_INCS = -I../ee_core/include
EE_LIBS = -lfileXio

all: $(EE_BIN)

# SCUS_971.13.ICO.iso
# SCUS_973.28.Gran Turismo 4.iso
# SLES_501.26.Quake III Revolution.iso
# SLES_539.74.Dragon Quest 8.iso
# SLES_549.45.DragonBall Z Budokai Tenkaichi 3.iso
# SLES_551.87.Wall-E.zso
run: all copy
	ps2client -h 192.168.1.10 execee host:$(EE_BIN) udpbd 'mass:DVD/SCUS_973.28.Gran Turismo 4.iso'

reset: clean
	ps2client -h 192.168.1.10 reset

clean:
	rm -f -r $(EE_OBJS) $(EE_BIN)

copy:
	rm -rf modules
	mkdir -p modules
	cp ../../iop/cdvdfsv/cdvdfsv.irx     modules
	cp ../../iop/cdvdman/bdm_cdvdman.irx modules
	cp ../../iop/eesync/eesync.irx       modules
	cp ../../iop/imgdrv/imgdrv.irx       modules
	cp ../../iop/isofs/isofs.irx         modules
	cp ../../iop/resetspu/resetspu.irx   modules
	cp ../../iop/udnl/udnl.irx           modules
	cp ../../iop/IOPRP.img               modules
	cp ../ee_core/ee_core.elf            modules
	cp $(PS2SDK)/iop/irx/iomanX.irx modules
	cp $(PS2SDK)/iop/irx/fileXio.irx modules
	cp $(PS2SDK)/iop/irx/bdm.irx modules
	cp $(PS2SDK)/iop/irx/bdmfs_vfat.irx modules
	cp $(PS2SDK)/iop/irx/usbd.irx modules
	cp $(PS2SDK)/iop/irx/usbmass_bd.irx modules
	cp $(PS2SDK)/iop/irx/mx4sio_bd.irx modules
	cp $(PS2SDK)/iop/irx/ps2dev9.irx modules
	cp $(PS2SDK)/iop/irx/netman.irx modules
	cp $(PS2SDK)/iop/irx/smap.irx modules
	cp $(PS2SDK)/iop/irx/ps2ip-nm.irx modules
	cp $(PS2SDK)/iop/irx/udptty.irx modules

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal_cpp