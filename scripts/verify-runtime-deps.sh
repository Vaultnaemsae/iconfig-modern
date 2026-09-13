#!/bin/zsh
set -eu
set -o pipefail

APP_PATH=${1:?usage: verify-runtime-deps.sh APP}
EXECUTABLE_DIR="$APP_PATH/Contents/MacOS"
failed=0

fail() {
  print -u2 "FAIL $*"
  failed=1
}

resolve_dependency() {
  local binary=$1 dependency=$2 relative candidate
  case "$dependency" in
    /System/Library/*|/usr/lib/*)
      return 0
      ;;
    @rpath/*)
      relative=${dependency#@rpath/}
      candidate="$APP_PATH/Contents/Frameworks/$relative"
      [[ -e "$candidate" ]] || fail "unresolved @rpath dependency $dependency in ${binary#$APP_PATH/}"
      ;;
    @loader_path/*)
      relative=${dependency#@loader_path/}
      candidate="${binary:h}/$relative"
      [[ -e "$candidate" ]] || fail "unresolved @loader_path dependency $dependency in ${binary#$APP_PATH/}"
      ;;
    @executable_path/*)
      relative=${dependency#@executable_path/}
      candidate="$EXECUTABLE_DIR/$relative"
      [[ -e "$candidate" ]] || fail "unresolved @executable_path dependency $dependency in ${binary#$APP_PATH/}"
      ;;
    /*)
      fail "machine-local absolute dependency $dependency in ${binary#$APP_PATH/}"
      ;;
    *)
      fail "unsupported relative dependency $dependency in ${binary#$APP_PATH/}"
      ;;
  esac
}

while IFS= read -r -d '' binary; do
  /usr/bin/file "$binary" | /usr/bin/grep -q 'Mach-O' || continue
  while IFS= read -r dependency; do
    [[ -n "$dependency" ]] || continue
    resolve_dependency "$binary" "$dependency"
  done < <(/usr/bin/otool -L "$binary" | /usr/bin/awk '/^[[:space:]]/ { print $1 }')

  while IFS= read -r rpath; do
    [[ -n "$rpath" ]] || continue
    case "$rpath" in
      @loader_path*|@executable_path*) ;;
      *) fail "machine-local or unsupported LC_RPATH $rpath in ${binary#$APP_PATH/}" ;;
    esac
  done < <(/usr/bin/otool -l "$binary" | /usr/bin/awk '
    $1 == "cmd" { is_rpath = ($2 == "LC_RPATH"); next }
    is_rpath && $1 == "path" { print $2; is_rpath = 0 }
  ')

  dylib_id=$(/usr/bin/otool -D "$binary" 2>/dev/null | \
    /usr/bin/awk 'NR > 1 && $1 ~ /^(@|\/)/ && !seen++ { print $1 }')
  if [[ -n "$dylib_id" ]]; then
    case "$dylib_id" in
      @rpath/*|@loader_path/*) ;;
      *) fail "non-relocatable dylib ID $dylib_id in ${binary#$APP_PATH/}" ;;
    esac
  fi
done < <(/usr/bin/find "$APP_PATH" -type f -print0)

if (( failed )); then
  exit 1
fi
print "All Mach-O dependencies are embedded-relative or Apple system paths."
