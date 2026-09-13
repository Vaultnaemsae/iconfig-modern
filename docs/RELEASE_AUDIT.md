# Release audit

## Current deployment floor evidence

The migrated app and Qt frameworks were built for macOS 14.0, but the Homebrew
Qt deployment copied 21 support libraries built for macOS 26.0. Therefore that
bundle's effective minimum is 26.0.

| Binary group | Count | minOS | Source |
| --- | ---: | ---: | --- |
| Main executable | 1 | 14.0 | iConfig qmake build |
| QtCore, QtDBus, QtGui, QtNetwork, QtWidgets | 5 | 14.0 | Homebrew Qt frameworks |
| qcocoa and qmacstyle plugins | 2 | 14.0 | Homebrew Qt plugins |
| Homebrew support dylibs listed below | 21 | 26.0 | Homebrew dependency bottles |

The 21 blockers are:

```text
libb2.1.dylib
libbrotlicommon.1.dylib
libbrotlidec.1.dylib
libcrypto.3.dylib
libdbus-1.3.dylib
libdouble-conversion.3.dylib
libfreetype.6.dylib
libglib-2.0.0.dylib
libgraphite2.3.dylib
libgthread-2.0.0.dylib
libharfbuzz.0.dylib
libicudata.78.dylib
libicui18n.78.dylib
libicuuc.78.dylib
libintl.8.dylib
libmd4c.0.dylib
libpcre2-16.0.dylib
libpcre2-8.0.dylib
libpng16.16.dylib
libssl.3.dylib
libzstd.1.dylib
```

## Decision

Target Apple Silicon and macOS 14 or later for the first beta. Do not ship the
Homebrew package as macOS 14-compatible.

The official Qt 6.11.2 `qtbase` package was tested without source changes. Its
five required frameworks and two required plugins have a macOS 13.0 minimum,
link only to other Qt or Apple system libraries, and introduce no support
dylibs. After packaging and deterministic arm64 thinning, the candidate audit
is:

| Binary group | Count | Maximum minOS | Runtime source |
| --- | ---: | ---: | --- |
| Main executable | 1 | 14.0 | iConfig qmake build |
| QtCore, QtDBus, QtGui, QtNetwork, QtWidgets | 5 | 13.0 | Official Qt 6.11.2 |
| qcocoa and qmacstyle plugins | 2 | 13.0 | Official Qt 6.11.2 |
| Non-Qt support dylibs | 0 | N/A | None |
| **Total** | **8** | **14.0** | Self-contained app |

All packaged Mach-O files are arm64-only. Load commands contain no Homebrew,
user-home, build-directory, or Qt developer-prefix paths. See
[RELEASE_TOOLCHAIN](RELEASE_TOOLCHAIN.md) for the pinned archive and checksums.

This establishes the binary metadata floor. A launch on an actual macOS 14
system remains a separate release acceptance test and must not be inferred from
load-command metadata alone.

Universal 2 is deferred. It requires separate x86_64 Qt, Boost, RtMidi,
GeneSysLib, and application builds plus hardware and UI regression under
Rosetta/native Intel.

## Bundle and data identity

The generated bundle and single-instance key use
`com.vaultnaemsae.iconfig-modern`. Qt persistence now uses organization
`Vaultnaemsae`, domain `vaultnaemsae.com`, and application `iConfig Modern`.
On the validation Mac this resolves to:

```text
Settings: ~/Library/Preferences/com.vaultnaemsae.iConfig Modern.plist
Data:     ~/Library/Application Support/Vaultnaemsae/iConfig Modern
Presets:  ~/Library/Application Support/Vaultnaemsae/iConfig Modern/presets
```

The legacy paths remain separate:

```text
Settings: ~/Library/Preferences/com.iconnectivity.iConnectivity iConfig.plist
Data:     ~/Library/Application Support/iConnectivity/iConnectivity iConfig
Presets:  ~/Library/Application Support/iConnectivity/iConnectivity iConfig/presets
```

Legacy access is confined to an explicit read-only importer. Real paired
presets were copied with matching SHA-256 hashes and timestamps while the
legacy files and settings plist retained their original hashes. Only
`appearance/mode` is eligible for preference import.
