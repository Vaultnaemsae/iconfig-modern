#!/bin/zsh
set -eu
set -o pipefail

TARGET=${1:?usage: verify-minos.sh APP_OR_BINARY MAXIMUM_MINOS}
MAXIMUM=${2:?usage: verify-minos.sh APP_OR_BINARY MAXIMUM_MINOS}
failed=0

version_le() {
  /usr/bin/awk -v lhs="$1" -v rhs="$2" 'BEGIN {
    split(lhs, a, "."); split(rhs, b, ".");
    for (i = 1; i <= 3; ++i) {
      av = (a[i] == "" ? 0 : a[i]) + 0;
      bv = (b[i] == "" ? 0 : b[i]) + 0;
      if (av < bv) exit 0;
      if (av > bv) exit 1;
    }
    exit 0;
  }'
}

while IFS= read -r -d '' binary; do
  /usr/bin/file "$binary" | /usr/bin/grep -q 'Mach-O' || continue
  minos=$(xcrun vtool -show-build "$binary" 2>/dev/null | \
    /usr/bin/awk '$1 == "minos" && !seen++ { print $2 }')
  [[ -n "$minos" ]] || continue
  print "$minos\t${binary#$TARGET/}"
  if ! version_le "$minos" "$MAXIMUM"; then
    failed=1
  fi
done < <(find "$TARGET" -type f -print0)

exit $failed
