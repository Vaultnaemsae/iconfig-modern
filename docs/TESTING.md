# Testing

## Offline suites

`./dev test` builds and runs:

- Controller Remap UI/model mapping;
- Channel Remap UI/model mapping;
- remap delayed-write batching;
- filter delayed-write batching;
- preset serialization and parser boundaries;
- restore planner and ACK state machine;
- fork namespace isolation and read-only legacy-data import.

The restore planner uses a generated synthetic preset by default. Two legacy
preset paths may be supplied directly to `restore_planner_test` for the optional
known-file compatibility audit; user presets are never included in the source
tree.

The self-contained suite passes 1384/1384 assertions: 496 controller-remap, 740
channel-remap, 41 remap-batching, 31 filter-batching, 23 preset-serialization,
23 restore-planner, and 30 namespace/import assertions. Preset tests use
generated data so no user preset is a release-source or CI input. Known
primary/auxiliary presets can still be supplied explicitly for an optional
compatibility audit.

The legacy test seam constructs `Communicator`, which initializes CoreMIDI even
for offline cases. The tests require a normal macOS user session today, but no
device configuration write is made. Removing that environmental coupling is a
test-infrastructure improvement, not a product behavior change.

## Integration tests

Required before a beta:

- application launch and normal quit;
- `macdeployqt` deployment;
- all Mach-O files arm64;
- no Homebrew load paths;
- declared minimum macOS floor verified for every embedded binary;
- Developer ID signature and Gatekeeper assessment;
- notarization and stapling.

## Hardware tests

Required on an iConnectAUDIO4+ for the first beta:

- discovery and read-only population;
- MIDI editor navigation with no population-time writes;
- preset Save and Restore preview;
- clean shutdown.

The already completed single-object restore/rollback is evidence for the
dispatcher but is not repeated in normal CI. CI must never require hardware.

Arbitrary full-device restore and firmware flashing remain outside the release
test matrix.
