GIT_TAG = $(shell git describe --tags)

EE_OBJS = main.o xparam.o patch.o ioprp.o tomlc17.o iso_cnf.o
EE_LIBS = -lfileXio -lpatches
EE_CFLAGS = -DGIT_TAG=\"$(GIT_TAG)\"

EE_NEWLIB_NANO = 1
EE_COMPACT_EXECUTABLE = 1

EE_BIN_NAME = neutrino
EE_BIN = $(EE_BIN_NAME)_unpacked.elf
EE_BIN_PACKED = $(EE_BIN_NAME).elf

$(EE_BIN_PACKED): $(EE_BIN)
	ps2-packer $< $@ > /dev/null

version.txt: FORCE
	echo $(GIT_TAG) > $@
FORCE:

all: $(EE_BIN_PACKED) version.txt

# TODO:
# SCUS_971.01 Twisted Metal Black (same as opl, when quit to menu the games crash with black screen)
# Max Payne 2 freezes when loading Part 1, Chapter 3 on neutrino USB - same on OPL ?
#GAME = 'DVD/Twisted Metal - Black (E).iso' # psxUNK -> IOP crash (executing data?)
#GAME = 'DVD/Cy Girls (U) (Disc 1) (Ice).iso'
#GAME = 'DVD/Cy Girls (U) (Disc 2) (Aska).iso'
#GAME = "CD/Pink Pong (E).iso" # BSOD on USB only. In PCSX2 with USB it works?
#[SLUS_208.51] Ace Combat 5 - The Unsung War: If you try to load this save there will be no music in the Briefing and if you skip the cutscene you will get a black screen, the last working OPL was r726
#[SLPM_659.98] Vampire - Darkstalkers Collection: If you try to select a game you will get black screen
#[SLUS_202.13] Test Drive: Start any race and then restart, the graphics will glitch and also the game will freeze at some point (the last OPL that worked well with this game is r790)
#[SLUS_202.33] Mobile Suit Gundam Zeonic Front: I believe the last working OPL is r726 (only for USB, it won't render the graphics on SMB), you will get black screen if you select the mission and if you're not pressing X constantly and even then the game will not render the graphics
#
#GAME = 'DVD/Mobile Suit Gundam - Zeonic Front (U).iso' # opl github issue 1073: gfx missing, only on real ps2, works in pcsx2 # interlaced frame mode
#GAME = 'DVD/Giants - Citizen Kabuto (E).iso'           # opl github issue 1260: sfx issues + gfx text issues
#GAME = 'DVD/Inuyasha - Feudal Combat (U).iso' -gsm=fp  # neutrino github issue  8: UDPBD: sfx missing or random freezes when starting match
#GAME = 'DVD/Scooby-Doo! Mystery Mayhem (U).iso'        # neutrino github issue 19: When starting a new game, the cutscene freezes
#GAME = 'DVD/Silent Hill 3 (U).iso'                     # neutrino github issue 30: USB:   freezes on startup logo, only on real ps2, works in pcsx2
#GAME = 'DVD/The Incredibles - Rise of the Underminer (E).iso' -gsm=fp # sound issues, starting at second level
#GAME = 'DVD/Driv3r (U).iso'                            # neutrino github issue 56: USB: freezes and unfreezes during gameplay
#GAME = 'DVD/Syberia II (E).iso' # -gsm=fp # with GSM some things run at 2x speed?
#GAME = 'CD/Simple 2000 Series Ultimate Vol. 15 - Love - Ping Pong! (J).iso' -gc=02 # neutrino github issue 88: black screen on boot - fixed with mode0+2 ?
#GAME = 'CD/Pink Pong (E).iso' # same game as above
#GAME = 'CD/Simple 2000 Series Ultimate Vol. 5 - Love - Mahjong! (J).iso' -gc=02 # neutrino github issue 89: randomly hangs
#GAME = 'DVD/Amplitude (U).iso'

# This game gets stuck trying to start the first battle
#GAME = 'DVD/World Destruction League - Thunder Tanks (E).iso' -gsm=fp:fp2
#GAME = 'CD/World Destruction League - Thunder Tanks (U).iso' -gsm=fp:fp2

# Games that use the HDD in one way or another
#GAME = 'DVD/The Sims 2 (U).iso'
#GAME = 'DVD/Metal Saga (U).iso' -gsm=fp
# Test SOCOM II with DLC on HDD
# https://psrewired.com/guides/socom2#install-dlc-pcsx2
#GAME = 'DVD/SOCOM II - U.S. Navy SEALs (E).iso' # -gsm=fp # with GSM some things run at 2x speed?!
#GAME = 'DVD/SOCOM II - U.S. Navy SEALs (U).iso' # -gsm=fp # with GSM some things run at 2x speed?!
#ATA0 = 'HDD/bd_apa.raw' # 40GB general purpose apa formatted using uLE
#ATA0 = 'HDD/SOCOM II HDD.raw'

# These games allocate ALL IOP RAM as an anti-piracy method
# Special IOP code is needed (in cdvdman?) to counter this
#GAME = 'DVD/Ratchet - Deadlocked (U).iso'
#GAME = 'DVD/Ratchet Gladiator (E).iso'

# ALL  : BSOD when returning back to menu
# Game uses different ELF files for menu and game
#GAME = 'CD/Densha de Go! 3 - Tsuukin-hen (J).iso' # interlaced frame mode

# CD "MODE2" based games, with multiple tracks
# not supported
#GAME = 'CD/Dynasty Warriors 2 (E) (Track 1).bin' # SLES_500.57

# USB   : BSOD ?!
# MX4SIO: works
# UDPBD : works
#GAME = 'CD/Super Bust-A-Move (U).iso'

# Works only with mode 0 + 2 ?
#GAME = 'DVD/Jackie Chan Adventures (E).iso' -gc=02 # interlaced frame mode

# ALL  : NOT WORKING! - random crashes (on OPL too)
#GAME = 'DVD/Gun (U).iso' # use game settings for better progressive

# ALL  : works
# This game uses a high priority IOP thread to wait for sceCdRead to complete
# is a storage device uses a lower priority thread, this will never get executed
# resulting in the game to freeze.
# mode2 can 'fix' this, but the proper fix is to make sure sceCdRead executes
# at a priority <9.
#GAME = 'DVD/Power Rangers - Super Legends (E).iso' -gsm=fp

# Tested, working offline
#   ONLINE this game uses a huge amount of IOP RAM. This causes IRX modules to fail to load
#   causing random crashes such as black screen or network adapter not detected.
#GAME = 'DVD/Ratchet Clank - Up Your Arsenal (U).iso' -gsm=fp::1
#GAME = 'DVD/Ratchet Clank 3 (E).iso' -gsm=fp::1

# Tested, working
#GAME = 'CD/ICO (U).iso' # 240p game
GAME = 'DVD/DragonBall Z Budokai Tenkaichi 3 (E).iso' -gsm=fp -gc=0
#GAME = 'DVD/PES 2023 (E).iso'
#GAME = 'DVD/Sonic Unleashed (E).iso'
#GAME = 'DVD/007 - Agent Under Fire (E).iso'
#GAME = 'DVD/MotorStorm Arctic Edge (E).iso' -gsm=fp
#GAME = 'DVD/Need for Speed Pro Street (E).iso'
#GAME = 'DVD/Ratchet Clank - Going Commando (E).iso'
#GAME = 'CD/Quake III Revolution (E).iso' -gsm=fp
#GAME = 'DVD/Super Dragon Ball Z (E).iso'
#GAME = 'CD/Tekken Tag Tournament (E) v2.iso' # USB compatibility issues ? # interlaced frame mode
#GAME = 'CD/Madden NFL 2001 (U).iso'
#GAME = 'CD/Madden NFL 2002 (U).iso'
#GAME = 'CD/NASCAR Thunder 2002 (U).iso'
#GAME = 'CD/Downforce (U).iso' # interlaced frame mode
#GAME = 'DVD/Winx Club (E).iso'
#GAME = 'DVD/Spyro - Enter the Dragonfly (E).iso'
#GAME = 'DVD/Spyro - Enter the Dragonfly (U).iso'
#GAME = 'DVD/Jak and Daxter - The Precursor Legacy (U).iso' # OVERLORD.IRX
#GAME = 'DVD/Jak 3 (E).iso' # OVERLRD2.IRX, needs mode3
#GAME = 'DVD/Jak 3 (U).iso' # OVERLRD2.IRX, needs mode3
#GAME = 'DVD/Prince of Persia - The Two Thrones (U).iso'
#GAME = 'CD/Star Wars - Racer Revenge (E).iso'
#GAME = 'CD/Star Wars - Racer Revenge (U).iso'
#GAME = 'CD/LEGO Racers 2 (E).iso' -gsm=fp # uses a lot of IOP RAM for caching files (many iop debug printf's about it)
#GAME = 'DVD/Metal Gear Solid 3 - Snake Eater (E).iso' -gsm=fp # VMC unreliable
#GAME = "DVD/Splinter Cell - Pandora Tomorrow (E).iso"
#GAME = "DVD/Splinter Cell - Double Agent (E).iso"
#GAME = "DVD/Splinter Cell - Chaos Theory (E).iso"
#GAME = "DVD/Jak X (E).iso"
#GAME = "DVD/007 - Nightfire (E).iso"
#GAME = "DVD/Splinter Cell (E).iso" -gc=7 -gsm=fp:fp2 # Skips FMV (IOP memory overrun issue? fixed!)
#GAME = "DVD/Splashdown (E).iso" -gc=7 -gsm=fp # IOP memory allocation issue: allocates too little for the DVD reads that follow (fixed!)
#GAME = 'DVD/God of War II (U).iso' # use game settings for progressive
#GAME = 'DVD/Gran Turismo 4 (U).iso' # interlaced frame mode, use game settings for progressive
#GAME = 'CD/OPL_Accurate_Read_v1.0.iso'
#GAME = 'DVD/Kidou Senshi Gundam - Ichinen Sensou (J).iso'
#GAME = 'DVD/Auto Modellista (E).iso' -gsm=fp
#GAME = 'DVD/Disney's Treasure Planet (E).iso' # buffer overrun issue
#GAME = 'DVD/Donkey Xote (E).iso' -gc=7 # buffer overrun issue
#GAME = 'DVD/Formula One 2001 (E).iso' # cdvdman break command, currently works 99% good, but hacky # interlaced frame mode
#GAME = 'DVD/Jak and Daxter - Complete Trilogy (U).iso'
#GAME = 'DVD/Jak and Daxter - The Lost Frontier (U).iso'
#GAME = 'DVD/Crash of the Titans (E).iso'
#GAME = 'DVD/EyeToy - Play 2 (E).iso'
#GAME = 'DVD/EyeToy - Play 3 (E).iso' # interlaced frame mode
#GAME = 'DVD/GTA Vice City Stories (E).iso' -gsm=fp
#GAME = 'DVD/Medal of Honor - Frontline (U).iso' -gsm=fp::1
#GAME = 'DVD/Silent Hill 2 (U).iso' -gsm=fp::1
#GAME = 'DVD/Silent Hill 4 - The Room (E).iso' -gsm=fp::1
#GAME = 'DVD/SOCOM 3 - U.S. Navy SEALs (E).iso' # -gsm=fp # with GSM some things run at 2x speed?!
#GAME = 'CD/Ridge Racer V (U).iso' # interlaced frame mode
#GAME = 'DVD/Crash Twinsanity (U).iso'
#GAME = 'DVD/Fatal Frame (U).iso' # interlaced frame mode
#GAME = "CD/Donald Duck - Goin' Quackers (U).iso"
#GAME = 'DVD/Donald Duck - Quack Attack (E).iso'
#GAME = 'DVD/Shadow Hearts - Covenant (U) (Disc 1).iso'
#GAME = 'DVD/Shadow Hearts - Covenant (U) (Disc 2).iso'
#GAME = 'DVD/Ibara (J).iso'
#GAME = 'DVD/Dot Hack Part 1 - Infection (U).iso' -elf='cdrom0:\SLUS_202.67;1' # large buffer overrun issue, fixed
#GAME = 'DVD/Street Fighter Alpha Anthology (U).iso'

# Example arguments
#-bsd=udpbd -dvd=mass:$(GAME)
#-bsd=udpbd -dvd=mass:$(GAME) -mc0=mass:VMC/mymc.bin
#-bsd=udpbd-hdd -dvd=mass:$(GAME) -ata0=mass:$(ATA0)
#-bsd=udpbd-hdd -dvd=mass:$(GAME) -ata0=mass:$(ATA0) -mc0=mass:VMC/mymc.bin
#-bsd=udpbd -dvd=mass:"DVD/Splinter Cell (E).iso" -elf="cdrom0:\\LOADER.PS2;1" --b 0_0_2 -wantload -ll -diskimage -restart -l=1 -v=13117
#-bsd=ata -bsdfs=hdl -dvd=hdl:$(GAME)

# Exmaple arguments (not working yet)
#-bsd=udpbd -mc0=mass:VMC/mymc.bin -elf=rom0:OSDSYS --b SkipMc SkipHdd BootBrowser

ARGS = -bsd=udpbd -dvd=mass:$(GAME)
#ARGS = -bsd=mmce -dvd=mmce1:$(GAME)
#ARGS = -bsd=mmce -dvd=mmce1:'DVD/SOCOM II - U.S. Navy SEALs (E).iso' -ata0=mmce1:'HDD/SOCOM II HDD.raw' -gc=0
#ARGS = -bsd=usb -dvd=mass:'DVD/SOCOM II - U.S. Navy SEALs (E).iso' -ata0=mass:'HDD/SOCOM II HDD.raw' -gc=0
#ARGS = -bsd=udpbd-hdd -dvd=mass:$(GAME) -ata0=mass:$(ATA0) -gc=0
#ARGS = -bsd=udpbd -dvd=bdfs:udp0p0 -bsdfs=bd

run: all
	ps2client -h 192.168.1.10 execee host:$(EE_BIN_PACKED) $(ARGS)

run_mmce_qb: all
	ps2client -h 192.168.1.10 -t 1 execiop host:modules/iomanX.irx
	ps2client -h 192.168.1.10 -t 1 execiop host:modules/fileXio.irx
	ps2client -h 192.168.1.10 -t 1 execiop host:modules/mmceman.irx
	ps2client -h 192.168.1.10 execee host:$(EE_BIN_PACKED) $(ARGS) -qb

sim: all
	flatpak --filesystem=host run net.pcsx2.PCSX2 $(PWD)/ee/loader/$(EE_BIN_PACKED)

# PCSX should be able to start with these, but the filename cannot contain spaces :(
# -gameargs "host:neutrino.elf $(ARGS)"

clean:
	rm -rf $(EE_OBJS) *_irx.o *_elf.o
	rm -rf $(EE_BIN) $(EE_BIN_PACKED)
	rm -rf modules

include $(PS2SDK)/Defs.make
include ../Rules.make
