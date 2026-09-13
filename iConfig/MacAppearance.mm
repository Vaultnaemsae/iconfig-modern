/*
;iConfig source code and documentation is released under a GPLv3 license.
;
; A copy is available from the Open Source Initiative site at:
;	https://opensource.org/licenses/gpl-3.0.html
*/

#include "MacAppearance.h"

#include <QApplication>

#import <AppKit/AppKit.h>

MacAppearance macAppearanceFromString(const QString& value) {
  if (value.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0) {
    return MacAppearance::Dark;
  }
  if (value.compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0) {
    return MacAppearance::System;
  }
  return MacAppearance::Light;
}

QString macAppearanceToString(MacAppearance appearance) {
  switch (appearance) {
  case MacAppearance::Dark:
    return QStringLiteral("dark");
  case MacAppearance::System:
    return QStringLiteral("system");
  case MacAppearance::Light:
  default:
    return QStringLiteral("light");
  }
}

void applyMacAppearance(MacAppearance appearance) {
  bool dark = false;

  @autoreleasepool {
    NSApplication* application = [NSApplication sharedApplication];
    switch (appearance) {
    case MacAppearance::Dark:
      application.appearance =
          [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
      dark = true;
      break;
    case MacAppearance::System:
      application.appearance = nil;
      dark = [[application.effectiveAppearance
          bestMatchFromAppearancesWithNames:@[
            NSAppearanceNameAqua, NSAppearanceNameDarkAqua
          ]] isEqualToString:NSAppearanceNameDarkAqua];
      break;
    case MacAppearance::Light:
    default:
      application.appearance =
          [NSAppearance appearanceNamed:NSAppearanceNameAqua];
      break;
    }
  }

  const QString gridColor = dark ? QStringLiteral("#747474")
                                 : QStringLiteral("#929292");
  qApp->setStyleSheet(QStringLiteral(
      "QTableView, QTableWidget { gridline-color: %1; }"
      "QTableView::item, QTableWidget::item { border-right: 1px solid %1; "
      "border-bottom: 1px solid %1; }"
      "QTreeView::item { border-bottom: 1px solid %1; }"
      "QHeaderView::section { border-right: 1px solid %1; "
      "border-bottom: 1px solid %1; }"
      "QTableCornerButton::section { border-right: 1px solid %1; "
      "border-bottom: 1px solid %1; }").arg(gridColor));
}
