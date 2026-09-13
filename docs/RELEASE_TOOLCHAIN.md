# Pinned macOS release toolchain

The public arm64 package uses the official Qt 6.11.2 macOS `qtbase` component,
not the Homebrew Qt bottle. Homebrew remains acceptable for local development.

## Qt provenance

- Vendor: The Qt Company, official Qt online repository
- Package ID: `qt.qt6.6112.clang_64`
- Package version: `6.11.2-0-202608131016`
- Component: `qtbase` for macOS, Universal x86_64/arm64
- Archive: `6.11.2-0-202608131016qtbase-MacOS-MacOS_15-Clang-MacOS-MacOS_15-X86_64-ARM64.7z`
- SHA-256: `9592f84f7e26d532c5c56824d1da7c9214a766cb0a17beb5af71022bcfbcd271`
- SHA-1: `898a61de33218d55538721ce35c61f973768cb07`
- Repository: <https://download.qt.io/online/qtsdkrepository/mac_x64/desktop/qt6_6112/qt6_6112/qt.qt6.6112.clang_64/>

Install or verify the pinned component without placing Qt binaries in this
repository:

```sh
./scripts/install-release-qt.sh
./scripts/install-release-qt.sh --verify
```

The default prefix is `~/Developer/Qt/6.11.2/macos`. Override it consistently
with `ICONFIG_RELEASE_QT_ROOT`.

## Validated build inputs

- Qt 6.11.2 official `qtbase` component above
- Xcode 26.6 / Apple clang 21 / macOS 26.5 SDK
- `MACOSX_DEPLOYMENT_TARGET=14.0`
- arm64 application build
- Boost 1.92.0 headers from Homebrew; the used Boost facilities are header-only
  and add no packaged dylib
- vendored RtMidi 2.1.1 and GeneSysLib source

The official Qt frameworks and the Cocoa/macOS-style plugins have a macOS 13.0
minimum and depend only on Qt or Apple system libraries. `macdeployqt` copies
their Universal 2 binaries; the release packaging step deterministically thins
each copied Mach-O to arm64. The installed Qt prefix remains unchanged.

## Release commands

```sh
ICONFIG_RELEASE_QT_ROOT="$HOME/Developer/Qt/6.11.2/macos" ./dev status
ICONFIG_RELEASE_QT_ROOT="$HOME/Developer/Qt/6.11.2/macos" ./dev package
ICONFIG_RELEASE_QT_ROOT="$HOME/Developer/Qt/6.11.2/macos" ./dev release-check
```

`package` and `release-check` reject Homebrew/MacPorts-style release Qt roots,
require exactly Qt 6.11.2, use the isolated `build/qt6-release-arm64` tree by
default, and audit the resulting bundle. Developer ID signing and notarization
are later release stages.
