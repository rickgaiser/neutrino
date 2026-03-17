ISO      = cdvd.iso
ISO_DIR  = iso
MODULES  = $(ISO_DIR)/MODULES
IOPRP    = ee/ioprp

all:
	$(MAKE) -C iop all
	$(MAKE) -C ee all

iso: all
	cp ee/cdvd.elf    $(ISO_DIR)/CDVD.ELF
	cp iop/cdvd.irx   $(ISO_DIR)/CDVD.IRX
	# IOPRP images in MODULES/ subdir — PS2 convention for SifIopReset cdrom0 paths
	mkdir -p $(MODULES)
	cp $(IOPRP)/ioprp14.img    $(MODULES)/IRP14.IMG
	cp $(IOPRP)/ioprp15.img    $(MODULES)/IRP15.IMG
	cp $(IOPRP)/ioprp16.img    $(MODULES)/IRP16.IMG
	cp $(IOPRP)/ioprp165.img   $(MODULES)/IRP165.IMG
	cp $(IOPRP)/ioprp202.img   $(MODULES)/IRP202.IMG
	cp $(IOPRP)/ioprp205.img   $(MODULES)/IRP205.IMG
	cp $(IOPRP)/ioprp21.img    $(MODULES)/IRP21.IMG
	cp $(IOPRP)/ioprp210.img   $(MODULES)/IRP210.IMG
	cp $(IOPRP)/ioprp211.img   $(MODULES)/IRP211.IMG
	cp $(IOPRP)/ioprp213.img   $(MODULES)/IRP213.IMG
	cp $(IOPRP)/ioprp214.img   $(MODULES)/IRP214.IMG
	cp $(IOPRP)/ioprp224.img   $(MODULES)/IRP224.IMG
	cp $(IOPRP)/ioprp23.img    $(MODULES)/IRP23.IMG
	cp $(IOPRP)/ioprp234.img   $(MODULES)/IRP234.IMG
	cp $(IOPRP)/ioprp241.img   $(MODULES)/IRP241.IMG
	cp $(IOPRP)/ioprp242.img   $(MODULES)/IRP242.IMG
	cp $(IOPRP)/ioprp243.img   $(MODULES)/IRP243.IMG
	cp $(IOPRP)/ioprp250.img   $(MODULES)/IRP250.IMG
	cp $(IOPRP)/ioprp253.img   $(MODULES)/IRP253.IMG
	cp $(IOPRP)/ioprp255.img   $(MODULES)/IRP255.IMG
	cp $(IOPRP)/ioprp260.img   $(MODULES)/IRP260.IMG
	cp $(IOPRP)/ioprp271.img   $(MODULES)/IRP271.IMG
	cp $(IOPRP)/ioprp271_2.img $(MODULES)/IRP2712.IMG
	cp $(IOPRP)/ioprp280.img   $(MODULES)/IRP280.IMG
	cp $(IOPRP)/ioprp300.img   $(MODULES)/IRP300.IMG
	cp $(IOPRP)/ioprp300_2.img $(MODULES)/IRP3002.IMG
	cp $(IOPRP)/ioprp300_3.img $(MODULES)/IRP3003.IMG
	cp $(IOPRP)/ioprp300_4.img $(MODULES)/IRP3004.IMG
	cp $(IOPRP)/ioprp310.img   $(MODULES)/IRP310.IMG
	cp $(IOPRP)/dnas280.img    $(MODULES)/DNAS280.IMG
	cp $(IOPRP)/dnas300.img    $(MODULES)/DNAS300.IMG
	cp $(IOPRP)/dnas300_2.img  $(MODULES)/DNS3002.IMG
	# Test data file — 512KB (256+ sectors for read tests)
	dd if=/dev/urandom of=$(ISO_DIR)/TEST.BIN bs=1024 count=512
	mkisofs -o $(ISO) $(ISO_DIR)

install: iso
	cp -f $(ISO) /mnt/truenas_data_exthdd/spellen/PS2/CD/

sim: iso
	flatpak --filesystem=host run net.pcsx2.PCSX2 $(PWD)/$(ISO)

clean:
	$(MAKE) -C iop clean
	$(MAKE) -C ee clean
	rm -f $(ISO_DIR)/CDVD.ELF $(ISO_DIR)/CDVD.IRX $(ISO_DIR)/TEST.BIN
	rm -rf $(MODULES)
	rm -f $(ISO)
