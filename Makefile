EESIO_DEBUG?=0
IOPCORE_DEBUG?=0

all:
	$(MAKE) -C iop/atad_emu     all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/bdfs         all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdfsv      all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdman_emu  all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdman_esr1 all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdman_esr2 all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/dev9         all DEBUG=$(IOPCORE_DEBUG) DEV9_HIDDEN=1
	$(MAKE) -C iop/dev9         all DEBUG=$(IOPCORE_DEBUG) DEV9_NO_SHUTDOWN=1
	$(MAKE) -C iop/fakemod      all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/fhi_bd       all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/fhi_bd_defrag all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/fhi_file     all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/gapfill      all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/hdlfs        all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/imgdrv       all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/mc_emu       all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/memcheck     all DEBUG=1
	$(MAKE) -C iop/patch_freemem all DEBUG=1
	$(MAKE) -C iop/patch_membo  all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/patch_rc_uya all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/smap_udpbd   all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/smap_udptty  all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/usbd_null    all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C ee/ee_core       all EESIO_DEBUG=$(EESIO_DEBUG)
	$(MAKE) -C ee/loader        all DEBUG=0

copy:
	$(MAKE) -C ee/loader    copy

format:
	find . -type f -a \( -iname \*.h -o -iname \*.c \) | xargs clang-format -i

clean:
	$(MAKE) -C iop/atad_emu     clean
	$(MAKE) -C iop/bdfs         clean
	$(MAKE) -C iop/cdvdfsv      clean
	$(MAKE) -C iop/cdvdman_emu  clean
	$(MAKE) -C iop/cdvdman_esr1 clean
	$(MAKE) -C iop/cdvdman_esr2 clean
	$(MAKE) -C iop/dev9         clean DEV9_HIDDEN=1
	$(MAKE) -C iop/dev9         clean DEV9_NO_SHUTDOWN=1
	$(MAKE) -C iop/fakemod      clean
	$(MAKE) -C iop/fhi_bd       clean
	$(MAKE) -C iop/fhi_bd_defrag clean
	$(MAKE) -C iop/fhi_file     clean
	$(MAKE) -C iop/gapfill      clean
	$(MAKE) -C iop/hdlfs        clean
	$(MAKE) -C iop/imgdrv       clean
	$(MAKE) -C iop/mc_emu       clean
	$(MAKE) -C iop/memcheck     clean
	$(MAKE) -C iop/patch_freemem clean
	$(MAKE) -C iop/patch_membo  clean
	$(MAKE) -C iop/patch_rc_uya clean
	$(MAKE) -C iop/smap_udpbd   clean
	$(MAKE) -C iop/smap_udptty  clean
	$(MAKE) -C iop/usbd_null    clean
	$(MAKE) -C ee/ee_core       clean
	$(MAKE) -C ee/loader        clean

# Start on PS2 (ps2link/ps2client)
run:
	$(MAKE) -C ee/loader     run

# Start on PS2 (ps2link/ps2client), using mmce device and quickboot
run_mmce_qb:
	$(MAKE) -C ee/loader     run_mmce_qb

# Copy neutrino to UDPBD shared drive, then run nhddl
UDPBD_BD = /dev/zd0p1
run_nhddl: all copy
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
sim:
	$(MAKE) -C ee/loader     sim

# Mount first partition of block device used in PCSX2 testing (ATA or USB)
sim_mount:
	losetup -Pf ee/loader/bd_exfat.raw

RELEASE_DIR = ./releases/neutrino_$(shell git describe --tags)
release: all copy
	mkdir -p                     $(RELEASE_DIR)
	cp    README.md              $(RELEASE_DIR)
	cp -R ee/loader/config       $(RELEASE_DIR)
	cp -R ee/loader/modules      $(RELEASE_DIR)
	cp    ee/loader/neutrino.elf $(RELEASE_DIR)
	cp    ee/loader/version.txt  $(RELEASE_DIR)
	7z a -t7z $(RELEASE_DIR).7z $(RELEASE_DIR)/*
