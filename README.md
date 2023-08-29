# neutrino
Small, Fast and Modular PS2 Game Loader

## Design
A neutrino is a particle with almost zero mass, and that's what this game loaders primary goal is. To have almost 0 mass when running ingame to maximize game compatibility.

Neutrino also does not have a user interface, instead it's meant to be integrated as a backend to a frontend (user interface). Possible user interfaces can be uLaunchELF, XEB+, OPL or any new user interface. This makes neutrino much more easy to maintain, and allows many more game loaders to be made using neutrino as the backend.

With neutrino all modules are... modular. They are in a separate folder called `modules`. This allows the user to replace them with new and improved modules. For instance when there is an improved mx4sio driver: simply replace the mx4sio_bd.irx with the new version, and done. No need for a complete new backend or frontend.

## ISO file storage support
The following storage devices are supported:
- USB
- ATA (internal HDD)
- MX4SIO
- UDPBD
- iLink / IEEE1394

These are all BDM drivers, or "Block Devices". On all devices the following file systems are supported:
- exFat
- FAT32 (/16/12)

## File type support
Only .iso files are supported.

## CD/DVD support
It is possible to use the following CD's / DVD's:
- Original CD's / DVD's (region free)
- ESR patched DVD's

## Usage instructions
When running neutrino using ps2link, the following usage instructions will be shown when an invalid argument is passed:
```
Usage: neutrino.elf -drv=<driver> -iso=<path>\n");

Options:\n");
  -drv=<driver>     Select device driver, supported are:
                    - ata
                    - usb
                    - mx4sio
                    - udpbd
                    - ilink
                    - dvd
                    - esr
  -iso=<file>       Select iso file (full path!)
  -elf=<file>       Select elf file inside iso to boot
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
  -eC               Enable eecore debug colors

Usage examples:
  ps2client -h 192.168.1.10 execee host:neutrino.elf -drv=usb -iso=mass:path/to/filename.iso
  ps2client -h 192.168.1.10 execee host:neutrino.elf -drv=dvd
  ps2client -h 192.168.1.10 execee host:neutrino.elf -drv=esr
```
