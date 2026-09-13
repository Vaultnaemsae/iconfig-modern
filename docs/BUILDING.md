# Building on macOS

## Supported build configuration

- Apple Silicon Mac
- Xcode and its command-line tools
- Qt 6.11.2
- Boost
- C++17
- qmake (CMake is intentionally not introduced)
- deployment target 14.0 for the application and GeneSysLib

Qt 5 is retained only as a frozen behavioral reference and must not be selected
by the maintained build wrapper. Homebrew Qt 6 may be used for development, but
it must not be used to create a public release package.

## Commands

```sh
./dev status
./dev build
./dev test
```

For a specific development Qt distribution:

```sh
QT_ROOT=/path/to/Qt/6.11.2/macos \
BOOST_ROOT=/path/to/boost \
MACOSX_DEPLOYMENT_TARGET=14.0 \
./dev build
```

The wrapper creates:

```text
build/qt6-release/genesyslib/libGeneSysLib.a
build/qt6-release/iconfig/iConnectivity iConfig.app
```

It prints the selected Qt, compiler, SDK, dependency roots, architecture, and
deployment target and fails if qmake is not Qt 6.

## Packaging

Install or verify the pinned official release Qt first:

```sh
./scripts/install-release-qt.sh
./scripts/install-release-qt.sh --verify
./dev package
./dev release-check
```

`package` ignores the development `QT_ROOT`. It uses
`ICONFIG_RELEASE_QT_ROOT` (default `~/Developer/Qt/6.11.2/macos`), requires
exactly Qt 6.11.2, and refuses Homebrew/MacPorts-style package-manager prefixes.
It creates a self-contained, arm64-only, **unsigned** staging app under
`build/qt6-release-arm64/package/`. Override that build location with
`ICONFIG_RELEASE_BUILD_ROOT`.

`release-check` intentionally fails until all release gates—including the
approved modern icon, Developer ID signature, notarization ticket, clean Git
state, architecture, load paths, and minimum OS—are satisfied.

## macOS floor

Setting `MACOSX_DEPLOYMENT_TARGET` only controls code built locally. Every
embedded framework, plugin, and dylib must also have `minos` at or below the
declared floor. Use:

```sh
scripts/verify-minos.sh 'path/to/iConfig Modern.app' 14.0
```

The Homebrew deployment is not a release candidate: its 21 Qt support dylibs
were built with macOS 26. The pinned official Qt 6.11.2 `qtbase` distribution
has been verified at macOS 13.0 and packages without those support dylibs. See
[RELEASE_TOOLCHAIN](RELEASE_TOOLCHAIN.md) for exact provenance and hashes.
