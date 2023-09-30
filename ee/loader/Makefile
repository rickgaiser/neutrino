EE_OBJS = main.o compat.o xparam.o patch.o ee_core_config.o ioprp.o toml.o
EE_INCS = -I../ee_core/include
EE_LIBS = -lfileXio -lpatches

EE_NEWLIB_NANO = 1
EE_COMPACT_EXECUTABLE = 1

EE_BIN_NAME = neutrino
EE_BIN = $(EE_BIN_NAME)_unpacked.elf
EE_BIN_PACKED = $(EE_BIN_NAME).elf

$(EE_BIN_PACKED): $(EE_BIN)
	ps2-packer $< $@ > /dev/null

all: $(EE_BIN_PACKED)

# TODO:
# SCUS_971.01 Twisted Metal Black (same as opl, when quit to menu the games crash with black screen)
# Max Payne 2 freezes when loading Part 1, Chapter 3 on neutrino USB - same on OPL ?
#GAME = 'DVD/Twisted Metal - Black (E).iso' # psxUNK -> IOP crash (executing data?)
#GAME = 'DVD/Cy Girls (U) (Disc 1) (Ice).iso'
#GAME = 'DVD/Cy Girls (U) (Disc 2) (Aska).iso'
#GAME = 'DVD/Mobile Suit Gundam - Zeonic Front (U).iso'
#GAME = 'DVD/Inuyasha - Feudal Combat (U).iso'
#GAME = "DVD/Disney's Treasure Planet (E).iso"
#GAME = "CD/Pink Pong (E).iso" # BSOD on USB only. In PCSX2 with USB it works?

# Games that use the HDD in one way or another
#GAME = 'DVD/The Sims 2 (U).iso'
#GAME = 'DVD/Metal Saga (U).iso'
# Test SOCOM II with DLC on HDD
# https://psrewired.com/guides/socom2#install-dlc-pcsx2
#GAME = 'DVD/SOCOM II - U.S. Navy SEALs (E).iso'
#ATA0 = 'HDD/SOCOM II HDD.raw'

# These games allocate ALL IOP RAM as an anti-piracy method
# Special IOP code is needed (in cdvdman?) to counter this
#GAME = 'DVD/Ratchet - Deadlocked (U).iso'
#GAME = 'DVD/Ratchet Gladiator (E).iso'

# ALL  : BSOD when returning back to menu
# Game uses different ELF files for menu and game
#GAME = 'CD/Densha de Go! 3 - Tsuukin-hen (J).iso'

# CD "MODE2" based games, with multiple tracks
# not supported
#GAME = 'CD/Dynasty Warriors 2 (E) (Track 1).bin' # SLES_500.57

# USB   : BSOD ?!
# MX4SIO: works
# UDPBD : works
#GAME = 'CD/Super Bust-A-Move (U).iso'

# USB  : works only with mode2
# UDPBD: works
#GAME = 'DVD/Jackie Chan Adventures (E).iso'

# ALL  : NOT WORKING! - random crashes (on OPL too)
#GAME = 'DVD/Gun (U).iso'

# ALL  : works
# This game uses a high priority IOP thread to wait for sceCdRead to complete
# is a storage device uses a lower priority thread, this will never get executed
# resulting in the game to freeze.
# mode2 can 'fix' this, but the proper fix is to make sure sceCdRead executes
# at a priority <9.
#GAME = 'DVD/Power Rangers - Super Legends (E).iso'

# Tested, working offline
#   ONLINE this game uses a huge amount of IOP RAM. This causes IRX modules to fail to load
#   causing random crashes such as black screen or network adapter not detected.
GAME = 'DVD/Ratchet Clank - Up Your Arsenal (U).iso'
#GAME = 'DVD/Ratchet Clank 3 (E).iso'

# Tested, working
#GAME = 'CD/ICO (U).iso'
#GAME = 'DVD/DragonBall Z Budokai Tenkaichi 3 (E).iso'
#GAME = 'DVD/PES 2023 (E).iso'
#GAME = 'DVD/Sonic Unleashed (E).iso'
#GAME = 'DVD/007 - Agent Under Fire (E).iso'
#GAME = 'DVD/MotorStorm Arctic Edge (E).iso'
#GAME = 'DVD/Need for Speed Pro Street (E).iso'
#GAME = 'DVD/Ratchet Clank - Going Commando (E).iso'
#GAME = 'CD/Quake III Revolution (E).iso'
#GAME = 'DVD/Super Dragon Ball Z (E).iso'
#GAME = 'CD/Tekken Tag Tournament (E) v2.iso'
#GAME = 'CD/Madden NFL 2001 (U).iso'
#GAME = 'CD/Madden NFL 2002 (U).iso'
#GAME = 'CD/NASCAR Thunder 2002 (U).iso'
#GAME = 'CD/Downforce (U).iso'
#GAME = 'DVD/Winx Club (E).iso'
#GAME = 'DVD/World Destruction League - Thunder Tanks (E).iso'
#GAME = 'CD/World Destruction League - Thunder Tanks (U).iso'
#GAME = 'DVD/Spyro - Enter the Dragonfly (E).iso'
#GAME = 'DVD/Spyro - Enter the Dragonfly (U).iso'
#GAME = 'DVD/Jak and Daxter - The Precursor Legacy (U).iso' # OVERLORD.IRX
#GAME = 'DVD/Jak 3 (E).iso' # OVERLRD2.IRX, needs mode3
#GAME = 'DVD/Jak 3 (U).iso' # OVERLRD2.IRX, needs mode3
#GAME = 'DVD/Prince of Persia - The Two Thrones (U).iso'
#GAME = 'CD/Star Wars - Racer Revenge (E).iso'
#GAME = 'CD/Star Wars - Racer Revenge (U).iso'
#GAME = 'CD/LEGO Racers 2 (E).iso' # uses a lot of IOP RAM for caching files (many iop debug printf's about it)
#GAME = 'DVD/Metal Gear Solid 3 - Snake Eater (E).iso' # VMC unreliable
#GAME = "DVD/Splinter Cell - Pandora Tomorrow (E).iso"
#GAME = "DVD/Splinter Cell - Double Agent (E).iso"
#GAME = "DVD/Splinter Cell - Chaos Theory (E).iso"
#GAME = "DVD/Jak X (E).iso"
#GAME = "DVD/007 - Nightfire (E).iso"
#GAME = "DVD/Splinter Cell (E).iso" # Skips FMV (IOP memory overrun issue? fixed!)
#GAME = "DVD/Splashdown (E).iso" # IOP memory allocation issue: allocates too little for the DVD reads that follow (fixed!)

# Example arguments
#-bsd=udpbd -dvd=mass:$(GAME)
#-bsd=udpbd -dvd=mass:$(GAME) -mc0=mass:VMC/mymc.bin
#-bsd=udpbd -dvd=mass:$(GAME) -ata0=mass:$(ATA0)
#-bsd=udpbd -dvd=mass:$(GAME) -ata0=mass:$(ATA0) -mc0=mass:VMC/mymc.bin
#-bsd=udpbd -dvd=mass:"DVD/Splinter Cell (E).iso" -elf="cdrom0:\\LOADER.PS2;1" --b 0_0_2 -wantload -ll -diskimage -restart -l=1 -v=13117
#-bsd=ata-hdl -dvd=hdl:$(GAME)

# Exmaple arguments (not working yet)
#-bsd=udpbd -mc0=mass:VMC/mymc.bin -elf=rom0:OSDSYS --b SkipMc SkipHdd BootBrowser

run: all copy
	ps2client -h 192.168.1.10 execee host:$(EE_BIN_PACKED) -bsd=udpbd -dvd=mass:$(GAME)

sim: all copy
	flatpak --filesystem=host run net.pcsx2.PCSX2 $(PWD)/ee/loader/$(EE_BIN_PACKED)

clean:
	rm -rf $(EE_OBJS) *_irx.o *_elf.o
	rm -rf $(EE_BIN) $(EE_BIN_PACKED)
	rm -rf modules

copy:
	rm -rf modules
	mkdir -p modules
	cp ../../iop/atad_emu/irx/atad_emu.irx         modules
	cp ../../iop/cdvdfsv/irx/cdvdfsv.irx           modules
	cp ../../iop/cdvdman_emu/irx/cdvdman_emu.irx   modules
	cp ../../iop/cdvdman_esr1/irx/cdvdman_esr1.irx modules
	cp ../../iop/cdvdman_esr2/irx/cdvdman_esr2.irx modules
	cp ../../iop/fakemod/irx/fakemod.irx           modules
	cp ../../iop/fhi_bdm/irx/fhi_bdm.irx           modules
	cp ../../iop/hdlfs/irx/hdlfs.irx               modules
	cp ../../iop/imgdrv/irx/imgdrv.irx             modules
	cp ../../iop/isofs/irx/isofs.irx               modules
	cp ../../iop/mc_emu/irx/mc_emu.irx             modules
	cp ../../iop/smap_udpbd/irx/smap_udpbd.irx     modules
	cp ../ee_core/ee_core.elf                      modules
	cp $(PS2SDK)/iop/irx/eesync.irx                modules
	cp $(PS2SDK)/iop/irx/iomanX.irx                modules
	cp $(PS2SDK)/iop/irx/fileXio.irx               modules
	cp $(PS2SDK)/iop/irx/bdm.irx                   modules
	cp $(PS2SDK)/iop/irx/bdmfs_fatfs.irx           modules
	cp $(PS2SDK)/iop/irx/ata_bd.irx                modules
	cp $(PS2SDK)/iop/irx/ps2atad.irx               modules
	cp $(PS2SDK)/iop/irx/ps2hdd-osd.irx            modules
	cp $(PS2SDK)/iop/irx/usbd_mini.irx             modules
	cp $(PS2SDK)/iop/irx/usbmass_bd_mini.irx       modules
	cp $(PS2SDK)/iop/irx/mx4sio_bd_mini.irx        modules
	cp $(PS2SDK)/iop/irx/ps2dev9.irx               modules
	cp $(PS2SDK)/iop/irx/iLinkman.irx              modules
	cp $(PS2SDK)/iop/irx/IEEE1394_bd_mini.irx      modules

include ../../Defs.make
include ../Rules.make
