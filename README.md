# neutrino
Small, Fast and Modular PS2 Device Emulator

## Design
A neutrino is a particle with almost zero mass, and that's what this device emulator's primary goal is. To have almost 0 mass when emulating devices to maximize compatibility.

Neutrino also does not have a user interface, instead it's meant to be integrated as a backend to a frontend (user interface). This makes neutrino much more easy to maintain, and allows many more applications to be made using neutrino as the backend.

With neutrino all modules are... modular. They are in a separate folder called `modules`. This allows the user add new and improved modules. What modules are loaded is fully configurable using TOML config files in the `config` folder.

## Environments
An environment in neutrino describes what IOP modules are loaded and defines what features the environment has. In neutrino there are 3 environments:
- Boot Environment (BE): environment neutrino is loaded from (by uLE / ps2link / ...), containing neutrino, configuration files and drivers
- Load Environment (LE): neutrino's loader.elf reboots into the LE, containing virtual disk images
- Emulation Environment (EE): neutrino's ee_core.elf reboots into the EE, emulating devices

## Backing Store Driver
A backing store driver provides a storage location for storing virtual disk images. For instance of DVD's, HDD's or MC's.
The following backing storage devices are supported:
- USB (`usb`)
- MX4SIO (`sdc`)
- ATA (internal HDD) (`ata`)
- UDPBD (`udp`)
- iLink / IEEE1394 (`sd`)

NOTE: Internal block device names between (parenthesis), these must be used for `bdfs:`

These are all BDM drivers, or "Block Devices". On all devices the following partitioning schemes are supported:
- MBR (Master Boot Record)
- GPT (GUID Partition Table)

And the following file systems:
- exFat/FAT32, accessable as `mass:<file>.iso`
- HDLoader, accessable as `hdl:<file>`, `hdl:<file>.iso`, `hdl:<part>` or `hdl:<part>.iso`
- Block Devices, accessable as `bdfs:<blockdevice>`. Like `bdfs:udp0p0`

Note that the HDLoader backing store is currently read-ony, and limited to only emulating the DVD.

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
Neutrino is a command line application. To get the most out of neutrino you will need to run it from the command line, for instance using [ps2link](https://github.com/ps2dev/ps2link) and [ps2client](https://github.com/ps2dev/ps2client).

Alternatively you can use a more user friendly GUI from one of the third-party projects (see below), but with a limited feature set.

Command line usage instructions:

```
Usage: neutrino.elf options

Options:
  -bsd=<driver>     Backing store drivers, supported are:
                    - no     (uses cdvd, default)
                    - ata    (block device)
                    - usb    (block device)
                    - mx4sio (block device)
                    - udpbd  (block device)
                    - ilink  (block device)
                    - mmce   (file system)

  -bsdfs=<driver>   Backing store fileystem drivers used for block device, supported are:
                    - exfat (default)
                    - hdl   (HD Loader)
                    - bd    (Block Device)
                    NOTE: Used only for block devices (see -bsd)

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
                    - 0: IOP: Fast reads (sceCdRead)
                    - 1: dummy
                    - 2: IOP: Sync reads (sceCdRead)
                    - 3: EE : Unhook syscalls
                    - 5: IOP: Emulate DVD-DL
                    - 7: IOP: Fix game buffer overrun
                    Multiple options possible, for example -gc=23

  -gsm=<mode>       GS video mode forcing (also know as GSM)
                    - 0: off (default)
                    - 1: on  576i/480i -> 576p/480p
                    - 2: on  576i/480i -> 576p/480p + line doubling

  -cwd=<path>       Change working directory

  -cfg=<file>       Load extra user/game specific config file (without .toml extension)

  -logo             Enable logo (adds rom0:PS2LOGO to arguments)
  -qb               Quick-Boot directly into load environment

  --b               Break, all following parameters are passed to the ELF

Usage examples:
  neutrino.elf -bsd=usb -dvd=mass:path/to/filename.iso
  neutrino.elf -bsd=ata -dvd=mass:path/to/filename.iso
  neutrino.elf -bsd=ata -bsdfs=hdl -dvd=hdl:filename.iso
  neutrino.elf -bsd=udpbd -bsdfs=bd -dvd=bdfs:udp0p0
```

## Third-Party Loaders
The following third-party projects use neutrino:

Loader | Author
-|-
[XEB+ neutrino Launcher Plugin](https://github.com/sync-on-luma/xebplus-neutrino-loader-plugin) | sync-on-luma
[NHDDL](https://github.com/pcm720/nhddl) | pcm720
[RETROLauncher](https://github.com/Spaghetticode-Boon-Tobias/RETROLauncher) | Boon Tobias

Add your project here? Send me a PR.
