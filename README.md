# iConfig Modern

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
- Release icon: fork-specific, trademark-neutral connectivity symbol
- Toolchain used for rehabilitation: Qt 6.11.2, Apple clang 21, C++17
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
The component-by-component evidence and exclusions are recorded in the
[licensing audit](docs/LICENSING_AUDIT.md).

Names, logos, product images, and trademarks remain the property of their
respective owners. Their presence in the historical source is not a claim of
endorsement.

## Community maintenance

Vaultnaemsae maintains this compatibility fork as a legacy/community project.
Support and release language should use the umbrella description:

> Community-maintained tools for legacy music hardware.

Keep iConfig Modern and LF+ in separate repositories and separate releases.
