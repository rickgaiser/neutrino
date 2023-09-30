EESIO_DEBUG?=0
IOPCORE_DEBUG?=0

all:
	$(MAKE) -C iop/atad_emu     all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdfsv      all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdman_emu  all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdman_esr1 all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdman_esr2 all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/fakemod      all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/fhi_bdm      all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/hdlfs        all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/imgdrv       all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/isofs        all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/mc_emu       all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/smap_udpbd   all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C ee/ee_core       all EESIO_DEBUG=$(EESIO_DEBUG)
	$(MAKE) -C ee/loader        all DEBUG=0

copy:
	$(MAKE) -C ee/loader    copy

format:
	find . -type f -a \( -iname \*.h -o -iname \*.c \) | xargs clang-format -i

clean:
	$(MAKE) -C iop/atad_emu     clean
	$(MAKE) -C iop/cdvdfsv      clean
	$(MAKE) -C iop/cdvdman_emu  clean
	$(MAKE) -C iop/cdvdman_esr1 clean
	$(MAKE) -C iop/cdvdman_esr2 clean
	$(MAKE) -C iop/fakemod      clean
	$(MAKE) -C iop/fhi_bdm      clean
	$(MAKE) -C iop/hdlfs        clean
	$(MAKE) -C iop/imgdrv       clean
	$(MAKE) -C iop/isofs        clean
	$(MAKE) -C iop/mc_emu       clean
	$(MAKE) -C iop/smap_udpbd   clean
	$(MAKE) -C ee/ee_core       clean
	$(MAKE) -C ee/loader        clean

# Start on PS2 (ps2link/ps2client)
run:
	$(MAKE) -C ee/loader     run

# Start on PCSX2
sim:
	$(MAKE) -C ee/loader     sim

# Mount first partition of block device used in PCSX2 testing (ATA or USB)
sim_mount:
	sudo mount -o loop,offset=1048576 ee/loader/blockdev.raw mount

# Unmount block device used in PCSX2 testing
sim_unmount:
	sudo umount mount

opl:
	ps2client -h 192.168.1.10 execee host:OPNPS2LD.ELF
