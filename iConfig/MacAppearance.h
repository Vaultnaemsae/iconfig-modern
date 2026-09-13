/*
;iConfig source code and documentation is released under a GPLv3 license.
;
; A copy is available from the Open Source Initiative site at:
;	https://opensource.org/licenses/gpl-3.0.html
*/

#ifndef MACAPPEARANCE_H
#define MACAPPEARANCE_H

#include <QString>

enum class MacAppearance {
  Light,
  Dark,
  System
};

const int kMacAppearanceRestartExitCode = 42;

MacAppearance macAppearanceFromString(const QString& value);
QString macAppearanceToString(MacAppearance appearance);
void applyMacAppearance(MacAppearance appearance);

#endif  // MACAPPEARANCE_H
