/*
;iConfig source code and documentation is released under a GPLv3 license.
;
; A copy is available from the Open Source Initiative site at:
;	https://opensource.org/licenses/gpl-3.0.html
*/

#ifndef LEGACYDATAIMPORT_H
#define LEGACYDATAIMPORT_H

#include <QDateTime>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <functional>

namespace LegacyDataImport {

struct NamespacePaths {
  QString settingsFile;
  QString appDataDirectory;
  QString presetDirectory;
  QString appConfigDirectory;
  QString cacheDirectory;
};

enum class ConflictPolicy {
  Skip,
  Rename,
  Replace
};

struct PresetImportResult {
  int importedPresets = 0;
  int importedFiles = 0;
  int skippedPresets = 0;
  int rejectedPresets = 0;
  QStringList importedPrimaryPaths;
  QStringList messages;
};

struct PreferenceImportResult {
  int importedKeys = 0;
  int rejectedKeys = 0;
  QStringList importedKeyNames;
  QStringList messages;
};

using PresetValidator =
    std::function<bool(const QByteArray &contents, QString *error)>;

NamespacePaths activeNamespacePaths();
NamespacePaths legacyNamespacePaths();

bool validatePresetEnvelope(const QByteArray &contents, quint8 expectedPID,
                            QString *error = nullptr);

PresetImportResult importPresets(const QString &legacyPresetDirectory,
                                 const QString &modernPresetDirectory,
                                 const QString &extension,
                                 quint8 expectedPID,
                                 ConflictPolicy conflictPolicy,
                                 const PresetValidator &validator = {});

QStringList whitelistedPreferenceKeys();
PreferenceImportResult importWhitelistedPreferences(QSettings &legacySettings,
                                                     QSettings &modernSettings);

}  // namespace LegacyDataImport

#endif  // LEGACYDATAIMPORT_H
