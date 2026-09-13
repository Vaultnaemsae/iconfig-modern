#!/bin/zsh
set -eu
set -o pipefail

ROOT_DIR=${0:A:h:h}
APP_PATH=${1:-}
SUPPORTED_FLOOR=${2:-14.0}
UPSTREAM_BASE=e4b804a709dbc7060df765b2a950f12bd2205420
LEGACY_ICON_SHA256=7aee7145ee2fce65ddfa27b2cc506497c88247b1e3ab3c5067ccd4bc6bb98085
failures=0
project_version=$(/usr/bin/awk '/^#define ICONFIG_VERSION / { gsub(/"/, "", $3); print $3 }' \
  "$ROOT_DIR/iConfig/Version.h")
bundle_short_version=${project_version%%-*}

pass() { print "PASS $*"; }
fail() { print -u2 "FAIL $*"; failures=$((failures + 1)); }

if git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  pass "Git repository exists"
else
  fail "Git repository has not been initialized"
fi

if git -C "$ROOT_DIR" rev-parse --verify HEAD >/dev/null 2>&1; then
  pass "HEAD is committed"
else
  fail "HEAD is not committed"
fi

if git -C "$ROOT_DIR" cat-file -e "$UPSTREAM_BASE^{commit}" 2>/dev/null; then
  pass "genuine upstream base commit exists"
else
  fail "genuine upstream base commit is missing"
fi

if git -C "$ROOT_DIR" merge-base --is-ancestor "$UPSTREAM_BASE" HEAD 2>/dev/null; then
  pass "HEAD descends from the genuine upstream base"
else
  fail "HEAD does not descend from the genuine upstream base"
fi

if [[ -z $(git -C "$ROOT_DIR" status --porcelain) ]]; then
  pass "source tree is clean"
else
  fail "source tree is not clean"
fi

if [[ -z $(git -C "$ROOT_DIR" ls-files --others --exclude-standard) ]]; then
  pass "no release-source files are untracked"
else
  fail "untracked release-source files remain"
fi

if git -C "$ROOT_DIR" check-ignore -q .release-archive/; then
  pass ".release-archive is ignored"
else
  fail ".release-archive is not ignored"
fi

for doc_path in README.md LICENSE LICENSE.md THIRD_PARTY_NOTICES.md PROVENANCE.md CHANGELOG.md docs/BUILDING.md docs/TESTING.md; do
  [[ -f "$ROOT_DIR/$doc_path" ]] && pass "$doc_path present" || fail "$doc_path missing"
done

if find "$ROOT_DIR" -path "$ROOT_DIR/.release-archive" -prune -o -path "$ROOT_DIR/build" -prune -o \
     -type f \( -name '*.original' -o -name '*.phase*.original' -o \
       -name '*.phase*-original' -o -name '*.pro.user' \) -print | grep -q .; then
  fail "backup or IDE-user files remain in the release tree"
else
  pass "no backup or IDE-user files in release tree"
fi

if rg -n '/Users/|/private/tmp|Phase [0-9]|phase[0-9]' \
     "$ROOT_DIR/GeneSysLib" "$ROOT_DIR/iConfig" "$ROOT_DIR/tests" \
     "$ROOT_DIR/dev" >/dev/null || \
   rg -n '/Users/|/private/tmp|Phase [0-9]|phase[0-9]' \
     "$ROOT_DIR/scripts" --glob '!release-check.sh' >/dev/null; then
  fail "phase markers or local-machine paths remain in release source"
else
  pass "no phase markers or local-machine paths in release source"
fi

if rg -n 'qt@5|Qt 5 qmake|/opt/homebrew/opt/qt@5' \
     "$ROOT_DIR/GeneSysLib" "$ROOT_DIR/iConfig" "$ROOT_DIR/tests" \
     "$ROOT_DIR/dev" >/dev/null || \
   rg -n 'qt@5|Qt 5 qmake|/opt/homebrew/opt/qt@5' \
     "$ROOT_DIR/scripts" --glob '!release-check.sh' >/dev/null; then
  fail "active build source still refers to the Qt 5 toolchain"
else
  pass "active build source selects Qt 6 only"
fi

if [[ "$project_version" == 0.1.0-beta.1 ]]; then
  pass "application version is $project_version"
else
  fail "application version header is inconsistent"
fi

if grep -q 'setOrganizationName("Vaultnaemsae")' "$ROOT_DIR/iConfig/Main.cpp" && \
   grep -q 'setOrganizationDomain("vaultnaemsae.com")' "$ROOT_DIR/iConfig/Main.cpp" && \
   grep -q 'setApplicationName("iConfig Modern")' "$ROOT_DIR/iConfig/Main.cpp"; then
  pass "fork organization, domain, and application namespace are explicit"
else
  fail "active application metadata does not use the complete modern namespace"
fi

if rg -n 'Library/Application Support/iConnectivity|iConnectivity/iConnectivity iConfig' \
     "$ROOT_DIR/iConfig" --glob '!LegacyDataImport.cpp' >/dev/null; then
  fail "hard-coded legacy storage path remains outside import-specific code"
else
  pass "legacy storage paths are confined to the read-only importer"
fi

if grep -q 'activeNamespacePaths' "$ROOT_DIR/tests/namespace-import/namespace_import_test.cpp" && \
   grep -q 'modern and legacy QSettings files differ' "$ROOT_DIR/tests/namespace-import/namespace_import_test.cpp" && \
   grep -q 'modern preset path is outside the legacy data tree' "$ROOT_DIR/tests/namespace-import/namespace_import_test.cpp"; then
  pass "release test suite gates resolved settings and preset path isolation"
else
  fail "resolved namespace-isolation release assertions are missing"
fi

if grep -q 'Preset Restore is experimental' "$ROOT_DIR/iConfig/MainWindow.cpp"; then
  pass "experimental Restore warning is present"
else
  fail "experimental Restore warning is missing"
fi

if grep -q 'actionUpgrade_Firmware->setVisible(false)' \
     "$ROOT_DIR/iConfig/MainWindow.cpp"; then
  pass "retired online firmware action is hidden"
else
  fail "retired online firmware action is exposed"
fi

if [[ -d "$ROOT_DIR/.git" ]]; then
  exact_tag=$(git -C "$ROOT_DIR" describe --tags --exact-match HEAD 2>/dev/null || true)
  [[ "$exact_tag" == "v$project_version" ]] && \
    pass "Git tag matches v$project_version" || \
    fail "HEAD is not tagged v$project_version"
fi

icon_source="$ROOT_DIR/iConfig/Icon.icns"
icon_master="$ROOT_DIR/iConfig/Assets/AppIconMaster.png"
iconset="$ROOT_DIR/iConfig/Assets/AppIcon.iconset"
if [[ -f "$icon_source" ]]; then
  icon_sha256=$(/usr/bin/shasum -a 256 "$icon_source" | /usr/bin/awk '{print $1}')
  [[ "$icon_sha256" != "$LEGACY_ICON_SHA256" ]] && \
    pass "active release icon differs from the known opaque legacy icon" || \
    fail "active release icon still equals the known opaque legacy icon"
else
  fail "active release icon is missing"
fi

if [[ -f "$icon_master" ]] && \
   [[ $(/usr/bin/sips -g pixelWidth "$icon_master" 2>/dev/null | /usr/bin/awk '/pixelWidth/ {print $2}') == 1024 ]] && \
   [[ $(/usr/bin/sips -g pixelHeight "$icon_master" 2>/dev/null | /usr/bin/awk '/pixelHeight/ {print $2}') == 1024 ]] && \
   [[ $(/usr/bin/sips -g hasAlpha "$icon_master" 2>/dev/null | /usr/bin/awk '/hasAlpha/ {print $2}') == yes ]]; then
  pass "1024px release-icon master has an alpha channel"
else
  fail "release-icon master is missing, incorrectly sized, or lacks alpha"
fi

icon_sizes=(
  icon_16x16.png:16 icon_16x16@2x.png:32
  icon_32x32.png:32 icon_32x32@2x.png:64
  icon_128x128.png:128 icon_128x128@2x.png:256
  icon_256x256.png:256 icon_256x256@2x.png:512
  icon_512x512.png:512 icon_512x512@2x.png:1024
)
iconset_valid=true
for icon_spec in $icon_sizes; do
  icon_file="$iconset/${icon_spec%%:*}"
  expected_size=${icon_spec##*:}
  actual_width=$(/usr/bin/sips -g pixelWidth "$icon_file" 2>/dev/null | /usr/bin/awk '/pixelWidth/ {print $2}' || true)
  actual_height=$(/usr/bin/sips -g pixelHeight "$icon_file" 2>/dev/null | /usr/bin/awk '/pixelHeight/ {print $2}' || true)
  [[ -f "$icon_file" && "$actual_width" == "$expected_size" && \
     "$actual_height" == "$expected_size" ]] || iconset_valid=false
done
if $iconset_valid; then
  pass "release icon set contains every required macOS size"
else
  fail "release icon set is missing required sizes or dimensions"
fi

if rg -n ':/Icon/Icons\.ico' "$ROOT_DIR/iConfig/Resources.qrc" \
     "$ROOT_DIR/iConfig/MainWindow.ui" \
     "$ROOT_DIR/iConfig/DeviceSelectionDialog.ui" >/dev/null; then
  fail "legacy Windows icon remains active in a macOS/Qt icon surface"
else
  pass "legacy Windows icon is not referenced by active macOS/Qt icon surfaces"
fi

if [[ ! -d "$APP_PATH" ]]; then
  fail "packaged app missing: $APP_PATH"
else
  executable="$APP_PATH/Contents/MacOS/iConnectivity iConfig"
  bundle_icon="$APP_PATH/Contents/Resources/Icon.icns"
  bundle_icon_name=$(/usr/bin/plutil -extract CFBundleIconFile raw -o - \
    "$APP_PATH/Contents/Info.plist" 2>/dev/null || true)
  if [[ "$bundle_icon_name" == Icon.icns && -f "$bundle_icon" && \
       $(/usr/bin/shasum -a 256 "$bundle_icon" | /usr/bin/awk '{print $1}') == "$icon_sha256" ]]; then
    pass "bundle Info.plist and icon resource use the reviewed release icon"
  else
    fail "bundle icon binding or packaged icon hash is inconsistent"
  fi
  file "$executable" | grep -q 'arm64' && pass "main executable is arm64" || fail "main executable is not arm64"
  if "$ROOT_DIR/scripts/verify-runtime-deps.sh" "$APP_PATH"; then
    pass "packaged Mach-O load commands are relocatable and complete"
  else
    fail "packaged Mach-O files contain local or unresolved runtime references"
  fi
  architecture_failures=0
  while IFS= read -r -d '' binary; do
    /usr/bin/file "$binary" | /usr/bin/grep -q 'Mach-O' || continue
    architectures=$(/usr/bin/lipo -archs "$binary")
    if [[ "$architectures" != arm64 ]]; then
      print -u2 "Unexpected architectures $architectures in: $binary"
      architecture_failures=1
    fi
  done < <(/usr/bin/find "$APP_PATH" -type f -print0)
  if (( architecture_failures )); then
    fail "packaged bundle is not arm64-only"
  else
    pass "all packaged Mach-O files are arm64-only"
  fi
  if "$ROOT_DIR/scripts/verify-minos.sh" "$APP_PATH" "$SUPPORTED_FLOOR"; then
    pass "all packaged Mach-O files meet macOS floor $SUPPORTED_FLOOR"
  else
    fail "one or more packaged Mach-O files exceed macOS floor $SUPPORTED_FLOOR"
  fi
  identifier=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' \
    "$APP_PATH/Contents/Info.plist" 2>/dev/null || true)
  short_version=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' \
    "$APP_PATH/Contents/Info.plist" 2>/dev/null || true)
  [[ "$identifier" == com.vaultnaemsae.iconfig-modern ]] && \
    pass "fork-specific bundle identifier is consistent" || \
    fail "unexpected bundle identifier: $identifier"
  [[ "$short_version" == "$bundle_short_version" ]] && \
    pass "bundle short version matches $bundle_short_version" || \
    fail "unexpected bundle short version: $short_version"
  /usr/bin/codesign --verify --deep --strict "$APP_PATH" 2>/dev/null && \
    pass "code signature verifies" || fail "code signature is absent or invalid"
  /usr/sbin/spctl --assess --type execute "$APP_PATH" 2>/dev/null && \
    pass "Gatekeeper assessment passes" || fail "Gatekeeper assessment does not pass"
  xcrun stapler validate "$APP_PATH" >/dev/null 2>&1 && \
    pass "notarization ticket validates" || fail "notarization ticket does not validate"
fi

if grep -q 'qt.qt6.6112.clang_64' "$ROOT_DIR/docs/BUILDING.md" && \
   grep -q '9592f84f7e26d532c5c56824d1da7c9214a766cb0a17beb5af71022bcfbcd271' \
     "$ROOT_DIR/docs/BUILDING.md"; then
  pass "pinned official Qt release provenance is documented"
else
  fail "pinned Qt release provenance is incomplete"
fi

if (( failures > 0 )); then
  print -u2 "RELEASE CHECK FAILED: $failures blocker(s)"
  exit 1
fi
print "RELEASE CHECK PASSED"
