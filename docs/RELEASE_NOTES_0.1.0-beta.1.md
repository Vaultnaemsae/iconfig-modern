# iConfig Modern 0.1.0-beta.1

iConfig Modern is a community-maintained macOS compatibility release based on
iConnectivity iConfig 4.2.7. It is not an official current iConnectivity
product.

## Highlights

- native Apple Silicon (`arm64`) and Qt 6 support;
- current macOS launch, appearance, window-sizing, table-grid, and navigation
  repairs;
- repaired MIDI Controller Remap and MIDI Channel Remap presentation and
  editing;
- repaired MIDI port/controller filter and remap delayed-write identity;
- safer preset saving and an isolated, fail-closed preset Restore dispatcher;
- independent settings, preset storage, and single-instance identity, with
  explicit read-only import of validated legacy data;
- obsolete automatic and online firmware checking removed from the UI;
- new transparent, trademark-neutral macOS application icon.

## Tested configuration

- iConnectAUDIO4+, firmware 2.0.5
- Apple Silicon (`arm64`)
- intended minimum macOS 14.0
- Qt 6.11.2

Every embedded binary in the release bundle declares macOS 14.0 or earlier.
Runtime launch on a physical macOS 14 system has not yet been independently
validated and must not be inferred from that binary audit alone.

## Beta limitations and safety

Preset Restore is **Experimental**. One isolated configuration object has
completed a real write, ACK, re-query, exact rollback, and second re-query, but
arbitrary full-device restores have not been validated. Back up current device
configuration before intentional writes.

Firmware flashing is not validated. Do not perform firmware updates, factory
resets, or broad restores without a recovery plan.

Only iConnectAUDIO4+ firmware 2.0.5 has completed the maintained release test
matrix. Other legacy iConnectivity products may appear in the recovered source
but are not claimed as validated by this beta.
