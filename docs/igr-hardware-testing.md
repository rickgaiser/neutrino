# In-game reset hardware-testing notes

This branch preserves the current OPL-derived Neutrino reset implementation.
It adds:

- controller-combo detection on physical controller ports 0 and 1;
- repeated libpad/libpad2 discovery for games that load their pad library
  after the initial ELF starts;
- an optional short front-panel power-button reset path;
- a two-stage clean return to the configured launcher ELF;
- explicit loader/EE-core ABI validation and visible launch errors; and
- compatibility Mode 6 (`DISABLE_IGR`) for titles that cannot safely use the
  resident controller hook.

The controller combo is **L1+L2+R1+R2+Start+Select**. The destination is passed
with `-igr-path`; `-igr-power=1` opts into front-panel short-press reset.

This is not yet a universal compatibility claim. Reset has worked on real
hardware in several titles, but other games have frozen, damaged the resident
worker, or required Mode 6. Q-Ball and similar titles remain targeted
compatibility work. Before a release, test game boot, normal controller input,
reset from both controller ports, audio after return, a second game launch,
and repeated reset cycles across a representative title set.

`ee/ee_core/include/padpatterns.h` is tracked with the port so a clean clone
does not depend on an unpublished local build input. It retains the upstream
Open PS2 Loader AFL-3.0 attribution.

## Preservation build results

- Two independent EE-core builds were byte-identical: 34,528 bytes, SHA-256
  `1B8F7A4AB702AFC3C5A3BB09B085107DD112C9A70EE6CF9D77F3F976DBA90194`.
- The loader built successfully: 76,756 bytes, SHA-256
  `518F09CF62B70294AD2788D3E616C33048936E954ACD7807AD694CED67B8C873`.
- A full build with the newer local PS2SDK stopped in the existing cdvdfsv
  IRX fixup step because that toolchain rejects a zero-valued exported text
  symbol. Produce release bundles with the project's pinned
  `ps2max/dev:v20260228` environment; that container was not installed on the
  preservation workstation.
