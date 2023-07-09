DEBUG=0

all:
	$(MAKE) -C iop/cdvdfsv  all DEBUG=$(DEBUG)
	$(MAKE) -C iop/cdvdman  all DEBUG=$(DEBUG) USE_BDM=1
	$(MAKE) -C iop/smap     all DEBUG=$(DEBUG)
	$(MAKE) -C iop/imgdrv   all DEBUG=$(DEBUG)
	$(MAKE) -C iop/isofs    all DEBUG=$(DEBUG)
	$(MAKE) -C iop/resetspu all DEBUG=$(DEBUG)
	$(MAKE) -C ee/ee_core   all
	$(MAKE) -C ee/loader    all

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
	$(MAKE) -C iop/resetspu  clean
	$(MAKE) -C ee/ee_core    clean
	$(MAKE) -C ee/loader     clean

run:
	$(MAKE) -C ee/loader     run
