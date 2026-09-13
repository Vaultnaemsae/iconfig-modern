# Vaultnaemsae website copy: iConfig Modern

## Placement

Software → Legacy Hardware Tools

> Community-maintained tools for legacy music hardware.

## Card

### iConfig Modern

Community-maintained Apple Silicon rebuild of iConnectivity iConfig 4.2.7 for
legacy iConnectivity hardware.

**Beta · macOS 14+ · Apple Silicon**

Links at publication:

- Source: `https://github.com/Vaultnaemsae/iconfig-modern`
- Download: the `v0.1.0-beta.1` GitHub prerelease asset

## Project page

iConfig Modern keeps the open-source iConnectivity iConfig 4.2.7 desktop
application usable on current Apple Silicon Macs. The maintained build uses Qt
6 and preserves the original device protocol and application behavior while
repairing modern-macOS compatibility and reliability defects.

Tested hardware: **iConnectAUDIO4+, firmware 2.0.5**. The first beta is
**Apple Silicon (`arm64`) only** and targets **macOS 14 or later**. Every bundled
binary has been audited at macOS 14.0 or earlier; runtime launch on a physical
macOS 14 system remains to be independently confirmed.

Preset Restore is **Experimental**. A single reversible restore transaction and
exact rollback have been validated, but arbitrary full-device restores have
not. Firmware flashing has not been validated. Back up device configuration
before intentional writes.

iConfig Modern is a community-maintained compatibility project. It is not an
official current iConnectivity product and is not presented as a commercial
Vaultnaemsae product. iConnectivity names and product images identify compatible
legacy hardware and do not imply endorsement.

Keep iConfig Modern and LF+ as separate projects, repositories, downloads, and
releases while using the shared Legacy Hardware Tools presentation.
