# iConfig Modern
<img width="256" height="256" alt="AppIconMaster" src="https://github.com/user-attachments/assets/4eb66073-1c7d-4e00-b165-21b8647ede8c" />

iConfig Modern is a community-maintained macOS rebuild of iConnectivity's
open-source iConfig 4.2.7 application. It exists to keep supported legacy
iConnectivity hardware usable on current Macs. It is not an official current
iConnectivity product and is not presented as an original Vaultnaemsae product.

The maintained desktop source builds with Qt 6 and retains the original Qt
Widgets/qmake architecture, GeneSysLib protocol model, and RtMidi 2.1.1
CoreMIDI backend.

## Status

- Project version: **0.1.0-beta.1**, based on iConfig 4.2.7
- Validated architecture: Apple Silicon (`arm64`)
- Validated device: iConnectAUDIO4+, firmware 2.0.5
- Maintained toolchain: Qt 6.11.2, Apple clang 21, C++17
- Preset Restore: **experimental**; one isolated object was restored and rolled
  back successfully, but arbitrary full-device restores have not been validated
- Firmware flashing: not validated; retired online firmware actions are hidden
- Settings and presets use the independent `Vaultnaemsae` / `iConfig Modern`
  namespace; legacy presets and the whitelisted appearance preference can be
  copied explicitly with **File > Import Legacy iConfig Data...**. The importer
  never writes to the legacy source.

Homebrew Qt is a development toolchain only: its packaged support libraries set
an effective macOS 26 minimum. Public packages use the pinned official Qt
6.11.2 distribution, whose required frameworks/plugins have a macOS 13 floor
and no third-party runtime dylibs. The arm64 app targets macOS 14 and the whole
bundle must pass the minimum-version and relocatable-dependency audits described
in [BUILDING](docs/BUILDING.md).

## Download

The current signed and notarized beta is available from the
[GitHub releases page](https://github.com/Vaultnaemsae/iconfig-modern/releases/tag/v0.1.0-beta.1).
The first beta is Apple Silicon only and targets macOS 14 or later; launch on a
physical macOS 14 system has not yet been directly validated.

## Build

Install Xcode command-line tools, Qt 6, and Boost, then point the wrapper at the
dependency prefixes if they are not discoverable through Homebrew:

```sh
QT_ROOT=/path/to/Qt/6.11.2/macos \
BOOST_ROOT=/path/to/boost \
./dev status

./dev build
./dev test
```

Builds are out of tree under `build/qt6-release/`. The wrapper refuses a Qt 5
qmake. See [BUILDING](docs/BUILDING.md) and [TESTING](docs/TESTING.md).

## Hardware safety

This software can change device configuration. Back up the current device state
before intentional writes. Do not perform firmware updates, resets, or broad
preset restores without a recovery plan. Preset Restore presents an experimental
warning and defaults to Cancel.

## Provenance and licensing

The recovered source was matched to the public iConnectivity/iConfig repository
at upstream commit `e4b804a709dbc7060df765b2a950f12bd2205420` and then repaired
for modern macOS and Qt 6. See [PROVENANCE](PROVENANCE.md).

The original source declares GPLv3. The maintained source is distributed under
GPLv3; see [LICENSE](LICENSE). Bundled and build-time dependencies have their own
terms documented in [THIRD_PARTY_NOTICES](THIRD_PARTY_NOTICES.md).

Names, logos, product images, and trademarks remain the property of their
respective owners. Their presence in the historical source is not a claim of
endorsement.

## Community maintenance

Vaultnaemsae maintains this compatibility fork as a legacy/community project.
Issues and contributions are welcome in this repository.

## Support

iConfig Modern is free and open source.

If it has been useful to you and you'd like to support future development, you can buy me a coffee:

☕ https://buymeacoffee.com/vaultnaemsae

Thank you for your support!

Learn more: https://www.vaultnaemsae.com
