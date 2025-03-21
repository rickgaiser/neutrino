all:
	$(MAKE) -C iop all
	$(MAKE) -C ee all

clean:
	$(MAKE) -C iop clean
	$(MAKE) -C ee clean

sim: all
	flatpak --filesystem=host run net.pcsx2.PCSX2 $(PWD)/ee/cdvd.elf
