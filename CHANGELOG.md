# Changelog

## 0.1.0-beta.1 — unreleased

Based on iConnectivity iConfig 4.2.7.

- migrated the maintained macOS build from Qt 4-era assumptions through Qt 5
  validation to Qt 6.11;
- added native Apple Silicon support and current CoreMIDI compatibility;
- added a conservative, trademark-neutral macOS release icon with transparent
  outer padding and complete standard icon sizes;
- repaired communicator shutdown mutex lifetime;
- repaired MIDI Controller Remap and Channel Remap UI/model handling;
- repaired MIDI port/controller filter and remap delayed-write identity;
- validated controller remapping against an iConnectAUDIO4+;
- hardened preset parsing and restore planning with isolated state, setter
  whitelisting, strict ACK matching, and fail-closed sequencing;
- validated one real restore write and exact rollback;
- restored readable table grids, appearance choices, stable toolbar overflow,
  Audio Mixer startup selection, and modern initial sizing;
- removed the obsolete automatic firmware check and hid retired online firmware
  actions;
- added an experimental warning before preset restore;
- separated the fork version, bundle, settings, data, presets, and
  single-instance identity from the legacy application;
- added explicit read-only import of validated legacy preset pairs and the
  whitelisted appearance preference.
- established the maintained branch on the genuine seven-commit iConnectivity
  Git history at upstream revision `e4b804a709dbc7060df765b2a950f12bd2205420`;
- added a pinned official Qt 6.11.2 release toolchain and complete arm64/macOS
  14 runtime-dependency gates.

Known limitation: arbitrary full-device preset restores are not yet validated.
