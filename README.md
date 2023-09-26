# neutrino
Small, Fast and Modular PS2 Device Emulator

## Design
A neutrino is a particle with almost zero mass, and that's what this device emulator's primary goal is. To have almost 0 mass when emulating devices to maximize compatibility.

Neutrino also does not have a user interface, instead it's meant to be integrated as a backend to a frontend (user interface). Possible user interfaces can be uLaunchELF, XEB+, OPL or any new user interface. This makes neutrino much more easy to maintain, and allows many more applications to be made using neutrino as the backend.

With neutrino all modules are... modular. They are in a separate folder called `modules`. This allows the user add new and improved modules. What modules are loaded is fully configurable using TOML config files in the `config` folder.

## Environments
An environment in neutrino describes what IOP modules are loaded and defines what features the environment has. In neutrino there are 3 environments:
- Boot Environment (BE): environment neutrino is loaded from (by uLE / ps2link / ...), containing neutrino, configuration files and drivers
- Load Environment (LE): neutrino's loader.elf reboots into the LE, containing virtual disk images
- Emulation Environment (EE): neutrino's ee_core.elf reboots into the EE, emulating devices

## Backing Store Driver
A backing store driver provides a storage location for storing virtual disk images. For instance of DVD's, HDD's or MC's.
The following backing storage devices are supported:
- USB
- MX4SIO
- ATA (internal HDD)
- UDPBD
- iLink / IEEE1394

These are all BDM drivers, or "Block Devices". On all devices the following file systems are supported:
- exFat
- FAT32 (/16/12)

During the loading phase, the exFat driver will be loaded and all files are accessable as `mass0:<file>` or `mass1:<file>`.

## CD/DVD emulation
The following CD/DVD emulation drivers are supported:
- No: using original CD's / DVD's in the optical drive
- ESR: using ESR patched DVD's in the optical drive
- File: using an iso file from the backing store

## ATA HDD emulation
The following HDD emulation drivers are supported:
- No: using ATA HDD in the PS2
- File: using a virtual HDD image file from the backing store

## Usage instructions
```
Usage: neutrino.elf options

Options:
  -bsd=<driver>     Backing store drivers, supported are:
                    - no (default)
                    - ata
                    - usb
                    - mx4sio
                    - udpbd
                    - ilink

  -dvd=<mode>       DVD emulation mode, supported are:
                    - no (default)
                    - esr
                    - <file>

  -ata0=<mode>      ATA HDD 0 emulation mode, supported are:
                    - no (default)
                    - <file>
                    NOTE: only both emulated, or both real.
                          mixing not possible
  -ata0id=<mode>    ATA 0 HDD ID emulation mode, supported are:
                    - no (default)
                    - <file>
                    NOTE: only supported if ata0 is present
  -ata1=<mode>      See -ata0=<mode>

  -mc0=<mode>       MC0 emulation mode, supported are:
                    - no (default)
                    - <file>
  -mc1=<mode>       See -mc0=<mode>

  -elf=<file>       ELF file to boot, supported are:
                    - auto (elf file from cd/dvd) (default)
                    - <file>

  -mt=<type>        Select media type, supported are:
                    - cd
                    - dvd
                    Defaults to cd for size<=650MiB, and dvd for size>650MiB

  -gc=<compat>      Game compatibility modes, supported are:
                    - 0: Disable builtin compat flags
                    - 1: IOP: Accurate reads (sceCdRead)
                    - 2: IOP: Sync reads (sceCdRead)
                    - 3: EE : Unhook syscalls
                    - 5: IOP: Emulate DVD-DL
                    Multiple options possible, for example -gc=23

  -eC               Enable debug colors
  -eL               Enable logo (adds rom0:PS2LOGO to arguments)

  --b               Break, all following parameters are passed to the ELF

Usage examples:
  neutrino.elf -bsd=usb -dvd=mass:path/to/filename.iso
  neutrino.elf
  neutrino.elf -dvd=esr
  neutrino.elf -elf=rom0:OSDSYS --b SkipMc SkipHdd BootBrowser
```
