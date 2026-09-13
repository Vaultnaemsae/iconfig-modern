# Provenance

## Original source

The recovered tree was compared with the public repository:

- Upstream: <https://github.com/iConnectivity/iConfig>
- Branch: `master`
- Upstream revision: `e4b804a709dbc7060df765b2a950f12bd2205420`
- Upstream commit message: `Revision 23. First version uploaded to GitHub.`
- Legacy runtime version: 4.2.7

The upstream repository contains the original desktop iConfig, GeneSysLib, and
iOS source trees and declares GPLv3 in its top-level license file and source
headers. Git authorship and dates from the upstream repository must be preserved
as upstream history; they must not be recreated or attributed to Vaultnaemsae.

## Maintained fork

The maintained desktop tree adds the minimum compatibility and reliability work
needed for current macOS, Apple Silicon, Qt 6, safe MIDI editing, and fail-closed
preset handling. The fork version is independent of the former vendor sequence:
`0.1.0-beta.1`, based on iConfig 4.2.7.

The maintained `modern` branch descends directly from upstream commit
`e4b804a709dbc7060df765b2a950f12bd2205420`. The seven genuine upstream
commits retain their original authors, timestamps, and objects. Maintained-fork
commits use current, truthful Vaultnaemsae authorship and dates; no upstream
history has been recreated, squashed, or backdated.

The upstream remote remains named `upstream` and points to the public
iConnectivity repository. No public fork remote is configured yet.

## Local-only material excluded from publication

- phase backups and generated build products;
- frozen Qt 5/Qt 6 reference bundles;
- local IDE metadata;
- the locally supplied Revision 26 protocol PDF, whose checksum differs from
  the Revision 23 PDF in the GPL upstream snapshot and whose separate
  redistribution permission has not been established;
- the historical iOS projects, which are outside the maintained desktop scope
  and carry additional third-party notices;
- obsolete Qt 4 project snapshots and local IDE metadata, which remain
  recoverable from genuine upstream history.

The current protocol document should be linked from iConnectivity's official
support site rather than copied into a public release without permission.

## Branding

The public repository and binary describe this as a community-maintained
compatibility fork. iConnectivity names and device images identify compatible
hardware; they do not imply current endorsement. The maintained app icon is an
original, trademark-neutral three-point connectivity symbol and does not use an
iConnectivity or Vaultnaemsae commercial-product mark. The opaque legacy icon
remains available in genuine upstream history and in the local release archive;
it is not used by the maintained application bundle or Qt window surfaces.
