# Licensing and provenance audit

This is an engineering provenance record, not legal advice.

| Component | Copyright/provenance evidence | License | Redistribution position | Required action |
| --- | --- | --- | --- | --- |
| iConfig desktop source | Official `iConnectivity/iConfig` repository; legacy About dialog names iKingdom Corp. 2017 | GPLv3 declaration in top-level upstream license and source headers | source and corresponding binaries may be redistributed under GPLv3 terms | preserve notices, publish corresponding maintained source, include GPLv3 text |
| GeneSysLib | same official repository and GPLv3 headers | GPLv3 | same as iConfig | preserve headers and include source in the public repository |
| QtSingleApplication | embedded Qt Solutions headers; Digia Plc, 2013 | BSD 3-Clause | permitted with source/binary notice conditions | retain headers and reproduce notice in binary documentation |
| RtMidi 2.1.1 | upstream tag `2.1.1`, commit `a94e7828f93b9fbf109d3f2d1028ddc097dd20cf`; Gary P. Scavone, 2003–2016 | permissive MIT-style license | permitted with copyright and permission notice | keep the pinned source and notice |
| Qt 6 | external Qt 6.11.2 distribution | GPLv3/open-source terms selected for this GPL application; bundled Qt third-party notices also apply | permitted when the selected Qt license and corresponding-source obligations are met | archive exact Qt provenance, ship license/notices, and make corresponding source available as required |
| Boost | external build dependency | Boost Software License 1.0 | permitted; no Boost binary is presently bundled | document build dependency; re-audit if binary material is ever embedded |
| legacy icons/device images | files from official GPL source snapshot; names and product imagery identify iConnectivity hardware | source-license grant exists, but trademark/endorsement rights are separate | source preservation is supportable; using legacy branding as the new public binary identity is not cleared by GPL alone | retain as historical source; the maintained binary uses a reviewed, original, trademark-neutral connectivity icon |
| iConfig Modern release icon | generated specifically for this maintained release from a user-authorized design brief; no vendor or commercial-product mark is used | original maintained-fork asset distributed under the repository license | suitable for source and binary redistribution with the maintained project | keep the transparent master, standard macOS size set, asset-catalog metadata, and compiled ICNS in release source |
| local Revision 26 protocol PDF | locally supplied file, not byte-identical to the Revision 23 document in the public GPL snapshot | no separate grant located | not cleared for publication by current evidence | excluded to `.release-archive/unlicensed-docs/`; link an official support copy |
| historical iOS trees | official repository plus embedded PureLayout, MMDrawerController, and IOSKnobControl notices | GPLv3 aggregate plus MIT/BSD third-party terms | potentially redistributable if all notices are preserved, but outside this desktop release audit | archived outside the proposed desktop repository; audit separately before publication |

The authoritative original history should come from the actual public upstream
repository. It should not be reconstructed with invented dates or authorship.
