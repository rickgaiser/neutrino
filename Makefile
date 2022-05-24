all:
	$(MAKE) -C iop/cdvdfsv   all
	$(MAKE) -C iop/cdvdman USE_BDM=1 IOPCORE_DEBUG=1 all
	$(MAKE) -C iop/eesync    all
	$(MAKE) -C iop/imgdrv    all
	$(MAKE) -C iop/isofs     all
	$(MAKE) -C iop/resetspu  all
	$(MAKE) -C iop/udnl      all
	$(MAKE) -C iop/udnl-t300 all
	$(MAKE) -C ee/loader     all
	$(MAKE) -C ee/ee_core    all

format:
	find . -type f -a \( -iname \*.h -o -iname \*.c \) | xargs clang-format -i

clean:
	$(MAKE) -C iop/cdvdfsv   clean
	$(MAKE) -C iop/cdvdman USE_BDM=1 IOPCORE_DEBUG=0 clean
	$(MAKE) -C iop/eesync    clean
	$(MAKE) -C iop/imgdrv    clean
	$(MAKE) -C iop/isofs     clean
	$(MAKE) -C iop/resetspu  clean
	$(MAKE) -C iop/udnl      clean
	$(MAKE) -C iop/udnl-t300 clean
	$(MAKE) -C ee/loader     clean
	$(MAKE) -C ee/ee_core    clean

run:
	$(MAKE) -C ee/loader     run