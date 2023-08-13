EESIO_DEBUG?=0
IOPCORE_DEBUG?=0

all:
	$(MAKE) -C iop/cdvdfsv  all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/cdvdman  all DEBUG=$(IOPCORE_DEBUG) USE_BDM=1
	$(MAKE) -C iop/smap     all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/imgdrv   all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C iop/isofs    all DEBUG=$(IOPCORE_DEBUG)
	$(MAKE) -C ee/ee_core   all EESIO_DEBUG=$(EESIO_DEBUG)
	$(MAKE) -C ee/loader    all DEBUG=0

copy:
	$(MAKE) -C ee/loader    copy

format:
	find . -type f -a \( -iname \*.h -o -iname \*.c \) | xargs clang-format -i

clean:
	$(MAKE) -C iop/cdvdfsv   clean
	$(MAKE) -C iop/cdvdman   clean USE_BDM=1
	$(MAKE) -C iop/smap      clean
	$(MAKE) -C iop/imgdrv    clean
	$(MAKE) -C iop/isofs     clean
	$(MAKE) -C ee/ee_core    clean
	$(MAKE) -C ee/loader     clean

run:
	$(MAKE) -C ee/loader     run

sim:
	$(MAKE) -C ee/loader     sim

opl:
	ps2client -h 192.168.1.10 execee host:OPNPS2LD.ELF
