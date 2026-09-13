# Source tree classification

## Proposed public repository

```text
.
├── GeneSysLib/                 original protocol/device library source
├── iConfig/                    maintained Qt 6 desktop application source
├── rtmidi-2.1.1/               pinned third-party source dependency
├── tests/                      offline regression suites
├── scripts/                    release and binary-audit helpers
├── docs/                       build, test, audit, release, and publication copy
├── dev                         project-owned build/test/package entry point
├── README.md
├── CHANGELOG.md
├── PROVENANCE.md
├── THIRD_PARTY_NOTICES.md
├── LICENSE
├── LICENSE.md
└── .gitignore
```

## Classification

| Material | Classification | Public-tree treatment |
| --- | --- | --- |
| `GeneSysLib/` | release source | keep, excluding generated/user files and historical qmake snapshot |
| `iConfig/` | release source | keep, excluding backups and historical qmake snapshot |
| `rtmidi-2.1.1/` | third-party dependency | keep pinned with provenance and license notice |
| `tests/` | release tests | keep; no private preset fixtures |
| `scripts/`, `dev` | build/release workflow | keep |
| root Markdown and `docs/` | documentation | keep, except the unlicensed local protocol PDF |
| Qt 4-named `.pro` files | historical reference | archive outside the public tree |
| `iOSiConfig4Audio/`, `iOSiConnectivityiConfig/` | historical reference | archive; outside maintained desktop scope |
| `.phase*.original`, `.original` | local backup | archive outside public tree |
| `build/`, Makefiles, moc/uic/rcc output, `.qmake.stash` | generated | ignore; never commit |
| `.pro.user`, `.DS_Store`, `Icon\r` | machine-specific metadata | archive or discard; never commit |
| `.release-archive/` | local preservation area | ignore; never publish |
| local Revision 26 protocol PDF | documentation with unproven redistribution permission | archive; link official source instead |
| frozen Qt 5/Qt 6 apps and manifests | local reference artifacts | retain under ignored archive only |

The legacy opaque icon remains preserved in genuine upstream history and the
local release archive, but it is not active in the maintained binary. The
reviewed replacement is transparent, correctly padded, trademark-neutral, and
stored with its complete standard macOS size set under `iConfig/Assets/`.
