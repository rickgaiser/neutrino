EESIO_DEBUG?=0
IOPCORE_DEBUG?=0

clean:
	$(MAKE) -C iop/atad_emu      clean
	$(MAKE) -C iop/bdfs          clean
	$(MAKE) -C iop/cdvdfsv       clean
	$(MAKE) -C iop/cdvdman_emu   clean
	$(MAKE) -C iop/cdvdman_esr1  clean
	$(MAKE) -C iop/cdvdman_esr2  clean
	$(MAKE) -C iop/dev9          clean DEV9_HIDDEN=1
	$(MAKE) -C iop/dev9          clean DEV9_NO_SHUTDOWN=1
	$(MAKE) -C iop/fakemod       clean
	$(MAKE) -C iop/fhi_bd        clean
	$(MAKE) -C iop/fhi_bd_defrag clean
	$(MAKE) -C iop/fhi_file      clean
	$(MAKE) -C iop/gapfill       clean
	$(MAKE) -C iop/hdlfs         clean
	$(MAKE) -C iop/imgdrv        clean
	$(MAKE) -C iop/mc_emu        clean
	$(MAKE) -C iop/memcheck      clean
	$(MAKE) -C iop/patch_freemem clean
	$(MAKE) -C iop/patch_membo   clean
	$(MAKE) -C iop/patch_rc_uya  clean
	$(MAKE) -C iop/smap_udpbd    clean
	$(MAKE) -C iop/smap_udptty   clean
	$(MAKE) -C iop/usbd_null     clean
	$(MAKE) -C ee/ee_core        clean
	$(MAKE) -C ee/loader         clean

all:
	$(MAKE) -C iop/atad_emu      all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/bdfs          all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdfsv       all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdman_emu   all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdman_esr1  all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdman_esr2  all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/dev9          all DEBUG=$(IOPCORE_DEBUG) DEV9_HIDDEN=1
	$(MAKE) -C iop/dev9          all DEBUG=$(IOPCORE_DEBUG) DEV9_NO_SHUTDOWN=1
	$(MAKE) -C iop/fakemod       all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/fhi_bd        all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/fhi_bd_defrag all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/fhi_file      all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/gapfill       all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/hdlfs         all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/imgdrv        all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/mc_emu        all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/memcheck      all DEBUG=1
	$(MAKE) -C iop/patch_freemem all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/patch_membo   all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/patch_rc_uya  all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/smap_udpbd    all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/smap_udptty   all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/usbd_null     all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C ee/ee_core        all EESIO_DEBUG=$(EESIO_DEBUG)
	$(MAKE) -C ee/loader         all DEBUG=0

copy:
	rm -rf ee/loader/modules
	mkdir -p ee/loader/modules
	cp iop/atad_emu/irx/atad_emu.irx           ee/loader/modules
	cp iop/bdfs/irx/bdfs.irx                   ee/loader/modules
	cp iop/cdvdfsv/irx/cdvdfsv.irx             ee/loader/modules
	cp iop/cdvdman_emu/irx/cdvdman_emu.irx     ee/loader/modules
	cp iop/cdvdman_esr1/irx/cdvdman_esr1.irx   ee/loader/modules
	cp iop/cdvdman_esr2/irx/cdvdman_esr2.irx   ee/loader/modules
	cp iop/dev9/irx/dev9_hidden.irx            ee/loader/modules
	cp iop/dev9/irx/dev9_ns.irx                ee/loader/modules
	cp iop/fakemod/irx/fakemod.irx             ee/loader/modules
	cp iop/fhi_bd/irx/fhi_bd.irx               ee/loader/modules
	cp iop/fhi_bd_defrag/irx/fhi_bd_defrag.irx ee/loader/modules
	cp iop/hdlfs/irx/hdlfs.irx                 ee/loader/modules
	cp iop/imgdrv/irx/imgdrv.irx               ee/loader/modules
	cp iop/mc_emu/irx/mc_emu.irx               ee/loader/modules
	cp iop/patch_freemem/irx/patch_freemem.irx ee/loader/modules
	cp iop/patch_membo/irx/patch_membo.irx     ee/loader/modules
	cp iop/patch_rc_uya/irx/patch_rc_uya.irx   ee/loader/modules
	cp iop/smap_udpbd/irx/smap_udpbd.irx       ee/loader/modules
	cp iop/smap_udptty/irx/smap_udptty.irx     ee/loader/modules
	cp ee/ee_core/ee_core.elf                  ee/loader/modules
	cp $(PS2SDK)/iop/irx/udnl.irx              ee/loader/modules
	cp $(PS2SDK)/iop/irx/udnl-t300.irx         ee/loader/modules
	cp $(PS2SDK)/iop/irx/eesync.irx            ee/loader/modules
	cp $(PS2SDK)/iop/irx/iomanX.irx            ee/loader/modules
	cp $(PS2SDK)/iop/irx/fileXio.irx           ee/loader/modules
	cp $(PS2SDK)/iop/irx/bdm.irx               ee/loader/modules
	cp $(PS2SDK)/iop/irx/bdmfs_fatfs.irx       ee/loader/modules
	cp $(PS2SDK)/iop/irx/ata_bd.irx            ee/loader/modules
	cp $(PS2SDK)/iop/irx/ps2hdd-bdm.irx        ee/loader/modules
	cp $(PS2SDK)/iop/irx/usbd_mini.irx         ee/loader/modules
	cp $(PS2SDK)/iop/irx/usbmass_bd_mini.irx   ee/loader/modules
	cp $(PS2SDK)/iop/irx/mx4sio_bd_mini.irx    ee/loader/modules
	cp $(PS2SDK)/iop/irx/iLinkman.irx          ee/loader/modules
	cp $(PS2SDK)/iop/irx/IEEE1394_bd_mini.irx  ee/loader/modules
	cp $(PS2SDK)/iop/irx/mmceman.irx           ee/loader/modules
	cp $(PS2SDK)/iop/irx/mmcefhi.irx           ee/loader/modules

copy_extra:
	cp iop/fhi_file/irx/fhi_file.irx           ee/loader/modules
	cp iop/gapfill/irx/gapfill.irx             ee/loader/modules
	cp iop/memcheck/irx/memcheck.irx           ee/loader/modules
	cp iop/usbd_null/irx/usbd_null.irx         ee/loader/modules

format:
	find . -type f -a \( -iname \*.h -o -iname \*.c \) | xargs clang-format -i

# Start on PS2 (ps2link/ps2client)
run: all copy copy_extra
	$(MAKE) -C ee/loader run

# Start on PS2 (ps2link/ps2client), using mmce device and quickboot
run_mmce_qb: all copy copy_extra
	$(MAKE) -C ee/loader run_mmce_qb

# Copy neutrino to UDPBD shared drive, then run nhddl
UDPBD_BD = /dev/zd0p1
run_nhddl: all copy copy_extra
	mkdir -p temp
	sudo mount $(UDPBD_BD) temp
	sudo cp    README.md              temp
	sudo cp -R ee/loader/config       temp
	sudo cp -R ee/loader/modules      temp
	sudo cp    ee/loader/neutrino.elf temp
	sudo cp    ee/loader/version.txt  temp
	sudo umount $(UDPBD_BD)
	rmdir temp
	ps2client -h 192.168.1.10 execee host:nhddl.elf

# Start on PCSX2
sim: all copy copy_extra
	$(MAKE) -C ee/loader sim

# Mount first partition of block device used in PCSX2 testing (ATA or USB)
sim_mount:
	losetup -Pf ee/loader/bd_exfat.raw

RELEASE_DIR = ./releases/neutrino_$(shell git describe --tags --exclude=latest)
release: all copy
	mkdir -p                     $(RELEASE_DIR)
	cp    README.md              $(RELEASE_DIR)
	cp -R ee/loader/config       $(RELEASE_DIR)
	cp -R ee/loader/modules      $(RELEASE_DIR)
	cp    ee/loader/neutrino.elf $(RELEASE_DIR)
	cp    ee/loader/version.txt  $(RELEASE_DIR)
	7z a -t7z $(RELEASE_DIR).7z $(RELEASE_DIR)/*
