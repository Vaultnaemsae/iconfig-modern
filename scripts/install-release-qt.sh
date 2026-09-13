#!/bin/zsh
set -eu
set -o pipefail

QT_VERSION=6.11.2
QT_PACKAGE=qt.qt6.6112.clang_64
QT_ARCHIVE_URL='https://download.qt.io/online/qtsdkrepository/mac_x64/desktop/qt6_6112/qt6_6112/qt.qt6.6112.clang_64/6.11.2-0-202608131016qtbase-MacOS-MacOS_15-Clang-MacOS-MacOS_15-X86_64-ARM64.7z'
QT_ARCHIVE_SHA256=9592f84f7e26d532c5c56824d1da7c9214a766cb0a17beb5af71022bcfbcd271
INSTALL_ROOT=${ICONFIG_RELEASE_QT_ROOT:-$HOME/Developer/Qt/$QT_VERSION/macos}
MODE=${1:-install}

verify_installation() {
  local qmake="$INSTALL_ROOT/bin/qmake"
  local qtcore="$INSTALL_ROOT/lib/QtCore.framework/Versions/A/QtCore"
  [[ -x "$qmake" ]] || { print -u2 "Missing qmake: $qmake"; return 1; }
  [[ -f "$qtcore" ]] || { print -u2 "Missing QtCore: $qtcore"; return 1; }
  [[ $("$qmake" -query QT_VERSION) == "$QT_VERSION" ]] || {
    print -u2 "Expected Qt $QT_VERSION at $INSTALL_ROOT"
    return 1
  }
  [[ " $(/usr/bin/lipo -archs "$qtcore") " == *" arm64 "* ]] || {
    print -u2 "Official QtCore has no arm64 slice"
    return 1
  }
  print "Verified official Qt release toolchain"
  print "Package: $QT_PACKAGE"
  print "Version: $QT_VERSION"
  print "Prefix: $INSTALL_ROOT"
  print "qmake: $qmake"
  print "QtCore architectures: $(/usr/bin/lipo -archs "$qtcore")"
  xcrun vtool -show-build "$qtcore" | /usr/bin/awk '
    ($1 == "minos" || $1 == "sdk") && !seen[$1]++ { print "QtCore " $1 ": " $2 }
  '
}

case "$MODE" in
  --verify)
    verify_installation
    exit
    ;;
  install)
    ;;
  *)
    print -u2 "usage: scripts/install-release-qt.sh [install|--verify]"
    exit 2
    ;;
esac

if [[ -x "$INSTALL_ROOT/bin/qmake" ]]; then
  print "Release Qt already exists; verifying without modifying it."
  verify_installation
  exit
fi

if [[ -e "$INSTALL_ROOT" ]] && [[ -n $(/bin/ls -A "$INSTALL_ROOT" 2>/dev/null) ]]; then
  print -u2 "Refusing to overwrite non-empty prefix: $INSTALL_ROOT"
  exit 1
fi

work_dir=$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/iconfig-release-qt.XXXXXX")
trap '/bin/rm -rf "$work_dir"' EXIT
archive="$work_dir/qtbase.7z"
extract="$work_dir/extract"
/bin/mkdir -p "$extract" "$INSTALL_ROOT"
/usr/bin/curl --fail --location --output "$archive" "$QT_ARCHIVE_URL"
actual_hash=$(/usr/bin/shasum -a 256 "$archive" | /usr/bin/awk '{print $1}')
[[ "$actual_hash" == "$QT_ARCHIVE_SHA256" ]] || {
  print -u2 "Qt archive SHA-256 mismatch: $actual_hash"
  exit 1
}
/usr/bin/bsdtar -xf "$archive" -C "$extract"
/usr/bin/ditto "$extract" "$INSTALL_ROOT"
verify_installation
