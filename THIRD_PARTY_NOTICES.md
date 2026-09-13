# Third-party notices

This file records components used by the maintained desktop application. It is
not a substitute for the complete license texts shipped by each dependency.

## Qt 6

- Project: Qt
- Source: <https://www.qt.io/>
- Copyright: The Qt Company Ltd. and other contributors
- License used by this GPLv3 application: GPLv3-compatible open-source terms
  offered by Qt; individual Qt modules and bundled third-party components may
  have additional notices
- Distribution requirement: ship the corresponding Qt license/notices and make
  the exact Qt source used for a binary release available as required

Qt is not vendored in this source tree. Release bundles contain dynamically
linked Qt frameworks copied by `macdeployqt`.

## QtSingleApplication (Qt Solutions)

- Location: `iConfig/qtsinglapplication/`
- Copyright: Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies)
- License: BSD 3-Clause, embedded in the source headers

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the copyright notice, this list
   of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.
3. Neither the name of Digia Plc and its subsidiary(-ies) nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

## RtMidi 2.1.1

- Location: `rtmidi-2.1.1/`
- Upstream: <https://github.com/thestk/rtmidi>
- Tag: `2.1.1`
- Tag object: `4561f63915e49af88c70e0c1ed8946047dddefc8`
- Commit: `a94e7828f93b9fbf109d3f2d1028ddc097dd20cf`
- Copyright: Copyright (c) 2003-2016 Gary P. Scavone
- License: permissive MIT-style license reproduced in `rtmidi-2.1.1/readme`,
  `RtMidi.h`, and `RtMidi.cpp`

The license permits use, copying, modification, merging, publishing,
distribution, sublicensing, and sale, subject to retaining its copyright and
permission notice. Its request that modifications be sent to the developer is
explicitly non-binding.

## Boost

- Project: Boost C++ Libraries
- Source: <https://www.boost.org/>
- License: Boost Software License 1.0
- Use: build dependency; Boost binaries are not expected in the application
  bundle

Boost is not vendored. If a future release embeds Boost binary material, its
license and architecture must be re-audited.

## Historical iOS source

The archived, non-release iOS trees include at least PureLayout (MIT),
MMDrawerController (MIT), and IOSKnobControl (BSD-style). They are not part of
the maintained macOS release tree. If those projects are ever republished, their
complete embedded notices must be preserved and audited separately.

## Original resources and documentation

Legacy icons, device images, and product names came from the original upstream
tree. GPL source publication does not itself grant trademark rights. The modern
binary uses a new trademark-neutral connectivity icon; it does not repurpose an
iConnectivity or Vaultnaemsae commercial-product logo. The local Revision 26
protocol PDF is excluded pending separate redistribution permission.
