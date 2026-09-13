# Release process

## Identity

- Project: iConfig Modern
- Version: `0.1.0-beta.1`
- Based on: iConnectivity iConfig 4.2.7
- Bundle identifier: `com.vaultnaemsae.iconfig-modern`
- Artifact: `iConfig-Modern-0.1.0-beta.1-macos-arm64.zip`
- Suggested repository: `iconfig-modern`

The fork-specific bundle and single-instance identifiers prevent Launch
Services and process-identity collisions. Qt persistence uses organization
`Vaultnaemsae`, domain `vaultnaemsae.com`, and application `iConfig Modern`, so
the modern application cannot write the legacy settings or preset tree.

Legacy data is available only through **File > Import Legacy iConfig Data...**.
The importer opens legacy presets/settings read-only, validates complete
`.ica4/.aux` pairs, defaults conflicts to Skip, copies with atomic destination
writes, and whitelists only the appearance preference. It never performs an
automatic migration.

## Restore policy

Preset Restore remains enabled but Experimental. A warning appears immediately
before the first dispatch, states that many settings may change, mentions an
auxiliary stage, and defaults to Cancel. The release notes must repeat this
limitation. Full-device restore is unsuitable as a general safety claim until a
snapshotted full restore has been validated on representative hardware.

## Signing order

After deployment and minimum-OS verification, sign with a Developer ID
Application identity, hardened runtime, secure timestamp, and no invented
entitlements:

1. embedded non-framework dylibs;
2. Qt plugins and helper executables;
3. each framework's executable/framework bundle;
4. the main executable;
5. the outer application bundle.

Use `codesign --force --options runtime --timestamp --sign` at each required
level, then `codesign --verify --deep --strict --verbose=4`. If hardened-runtime
hardware discovery fails, diagnose the exact restriction before adding any
entitlement. Do not use `--deep` as a substitute for explicit signing.

## Notarization and artifact

1. create a ZIP for submission with `ditto -c -k --keepParent`;
2. submit with `xcrun notarytool submit ... --wait` using a Keychain profile;
3. inspect the notarization log if rejected;
4. staple the accepted ticket to the app and validate it;
5. run `spctl --assess --type execute` and the full release check;
6. create the final ZIP from the stapled app.

A ZIP is sufficient for the first beta. A decorative DMG adds no safety and is
deferred.

## Public Git history

Preserve the actual upstream repository history, then add one maintained-fork
commit containing provenance, notices, build/test workflow, and Qt 6 source.
Add further commits only where they represent reviewable release preparation.
Do not fabricate original authorship or dates.

## GitHub beta release notes

### iConfig Modern 0.1.0-beta.1

Community-maintained macOS compatibility release based on iConnectivity iConfig
4.2.7.

Highlights:

- native Apple Silicon and Qt 6 support;
- current macOS launch, appearance, window, and navigation fixes;
- repaired MIDI Controller Remap, Channel Remap, and filter/remap batching;
- safer preset saving and isolated, fail-closed preset restore dispatch;
- obsolete automatic/online firmware checking removed from the UI.

Tested hardware: iConnectAUDIO4+, firmware 2.0.5. Architecture: arm64. Minimum
macOS: 14.0 only after the final embedded-binary audit passes.

Preset Restore is experimental. One isolated object was restored and rolled
back successfully; arbitrary full-device restores have not been validated.
Firmware flashing is not validated. Back up device configuration before writes.

## Release toolchain

Development builds may use Homebrew Qt. Public packages must use the pinned
official Qt 6.11.2 `qtbase` component documented in
[RELEASE_TOOLCHAIN](RELEASE_TOOLCHAIN.md). `./dev package` selects that prefix
explicitly, rejects a package-manager Qt prefix, deploys only the Cocoa and
macOS-style plugins plus required frameworks, and thins copied Universal 2 code
to arm64. Every Mach-O must be at or below macOS 14.0 and use only embedded or
Apple system runtime paths.

This is not an official current iConnectivity product.

## Website and LF+ coordination

Place the project under `Software / Legacy Hardware Tools`, visually distinct
from paid products. Include provenance, tested device/firmware, beta status,
requirements, limitations, GitHub link, signed release download, and disclaimer.

iConfig Modern and LF+ should use the shared umbrella wording “Community-
maintained tools for legacy music hardware,” consistent README structure and
support language, but remain separate repositories, downloads, and releases.

## Finite path to first beta

1. verify the reviewed transparent, padded, trademark-neutral app icon and its
   source/iconset hashes;
2. initialize from the real upstream Git history and commit the audited overlay;
3. run `./dev test` and the hardware read-only regression;
4. use the pinned release Qt toolchain, run `./dev package`, and verify every
   embedded binary's architecture, load paths, and minimum OS;
5. review the exact release commit and generated candidate;
6. run the license/notices and version consistency review;
7. sign inside-out with Developer ID Application and hardened runtime;
8. notarize and staple the accepted app;
9. tag the exact reviewed commit `v0.1.0-beta.1`, run `./dev release-check`,
   and create the GitHub prerelease with the ZIP and notes;
10. download the published ZIP on a clean test account/Mac, verify signature,
    Gatekeeper, launch, discovery, and checksum;
11. publish the Legacy Hardware Tools website page and verify its GitHub and
    release-download links.

End state:

```text
GitHub release published
→ website updated
→ download verified
```

`./dev release-check` reruns the full public offline suite before auditing the
already packaged, signed, stapled candidate. It deliberately does not rebuild or
redeploy that candidate, because doing so would invalidate its signatures and
notarization ticket.
