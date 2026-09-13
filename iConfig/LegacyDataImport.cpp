/*
;iConfig source code and documentation is released under a GPLv3 license.
;
; A copy is available from the Open Source Initiative site at:
;	https://opensource.org/licenses/gpl-3.0.html
*/

#include "LegacyDataImport.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace LegacyDataImport {
namespace {

const char kLegacyOrganization[] = "iConnectivity";
const char kLegacyApplication[] = "iConnectivity iConfig";

struct FileSnapshot {
  bool existed = false;
  bool readable = true;
  QByteArray contents;
  QDateTime modified;
};

bool readFile(const QString &path, QByteArray *contents, QString *error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error) {
      *error = QStringLiteral("Cannot read %1: %2")
                   .arg(path, file.errorString());
    }
    return false;
  }
  *contents = file.readAll();
  if (file.error() != QFileDevice::NoError) {
    if (error) {
      *error = QStringLiteral("Cannot completely read %1: %2")
                   .arg(path, file.errorString());
    }
    return false;
  }
  return true;
}

bool setModificationTime(const QString &path, const QDateTime &modified) {
  if (!modified.isValid()) {
    return true;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadWrite)) {
    return false;
  }
  const bool result =
      file.setFileTime(modified, QFileDevice::FileModificationTime);
  file.close();
  return result;
}

bool writeAtomically(const QString &path, const QByteArray &contents,
                     const QDateTime &modified, QString *error) {
  QSaveFile output(path);
  if (!output.open(QIODevice::WriteOnly)) {
    if (error) {
      *error = QStringLiteral("Cannot create %1: %2")
                   .arg(path, output.errorString());
    }
    return false;
  }
  if (output.write(contents) != contents.size() || !output.commit()) {
    if (error) {
      *error = QStringLiteral("Cannot complete %1: %2")
                   .arg(path, output.errorString());
    }
    return false;
  }
  if (!setModificationTime(path, modified)) {
    if (error) {
      *error = QStringLiteral("Imported %1, but could not preserve its timestamp.")
                   .arg(path);
    }
    return false;
  }
  return true;
}

FileSnapshot snapshot(const QString &path) {
  FileSnapshot result;
  QFileInfo info(path);
  result.existed = info.exists();
  result.modified = info.lastModified();
  if (result.existed) {
    QString ignored;
    result.readable = readFile(path, &result.contents, &ignored);
  }
  return result;
}

void restoreSnapshot(const QString &path, const FileSnapshot &saved) {
  if (!saved.existed) {
    QFile::remove(path);
    return;
  }
  QString ignored;
  writeAtomically(path, saved.contents, saved.modified, &ignored);
}

QString renamedPrimaryPath(const QString &destinationDirectory,
                           const QString &fileName,
                           const QString &extension) {
  QString stem = fileName;
  if (stem.endsWith(extension, Qt::CaseInsensitive)) {
    stem.chop(extension.size());
  }
  for (int index = 1;; ++index) {
    const QString suffix = index == 1
        ? QStringLiteral(" (Imported)")
        : QStringLiteral(" (Imported %1)").arg(index);
    const QString candidate =
        QDir(destinationDirectory).filePath(stem + suffix + extension);
    if (!QFileInfo::exists(candidate) &&
        !QFileInfo::exists(candidate + QStringLiteral(".aux"))) {
      return candidate;
    }
  }
}

bool validateContents(const QByteArray &contents, quint8 expectedPID,
                      const PresetValidator &validator, QString *error) {
  if (!validatePresetEnvelope(contents, expectedPID, error)) {
    return false;
  }
  return !validator || validator(contents, error);
}

}  // namespace

NamespacePaths activeNamespacePaths() {
  QSettings settings;
  NamespacePaths result;
  result.settingsFile = settings.fileName();
  result.appDataDirectory =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  result.presetDirectory =
      QDir(result.appDataDirectory).filePath(QStringLiteral("presets"));
  result.appConfigDirectory =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  result.cacheDirectory =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  return result;
}

NamespacePaths legacyNamespacePaths() {
  QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                     QString::fromLatin1(kLegacyOrganization),
                     QString::fromLatin1(kLegacyApplication));
  const QString dataRoot =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  const QString configRoot =
      QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
  const QString cacheRoot =
      QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
  const QString relative = QStringLiteral("iConnectivity/iConnectivity iConfig");

  NamespacePaths result;
  result.settingsFile = settings.fileName();
  result.appDataDirectory = QDir(dataRoot).filePath(relative);
  result.presetDirectory =
      QDir(result.appDataDirectory).filePath(QStringLiteral("presets"));
  result.appConfigDirectory = QDir(configRoot).filePath(relative);
  result.cacheDirectory = QDir(cacheRoot).filePath(relative);
  return result;
}

bool validatePresetEnvelope(const QByteArray &contents, quint8 expectedPID,
                            QString *error) {
  const auto fail = [error](const QString &message) {
    if (error) {
      *error = message;
    }
    return false;
  };

  if (contents.size() < 21 || contents.left(3) != QByteArrayLiteral("iCM")) {
    return fail(QStringLiteral("Invalid preset signature or truncated envelope."));
  }
  if (static_cast<quint8>(contents.at(3)) != expectedPID) {
    return fail(QStringLiteral("Preset is for a different device model."));
  }
  const quint8 version = static_cast<quint8>(contents.at(4));
  if (version != 1 && version != 2) {
    return fail(QStringLiteral("Unsupported preset version."));
  }
  if (version == 2) {
    if (contents.size() < 22) {
      return fail(QStringLiteral("Truncated version-2 preset header."));
    }
    const int descriptionSize = static_cast<quint8>(contents.at(5));
    if (6 + descriptionSize > contents.size() - 16) {
      return fail(QStringLiteral("Preset description exceeds the envelope."));
    }
  }
  const QByteArray expectedHash = contents.right(16);
  const QByteArray actualHash = QCryptographicHash::hash(
      contents.left(contents.size() - 16), QCryptographicHash::Md5);
  if (expectedHash != actualHash) {
    return fail(QStringLiteral("Preset integrity hash does not match."));
  }
  return true;
}

PresetImportResult importPresets(const QString &legacyPresetDirectory,
                                 const QString &modernPresetDirectory,
                                 const QString &extension,
                                 quint8 expectedPID,
                                 ConflictPolicy conflictPolicy,
                                 const PresetValidator &validator) {
  PresetImportResult result;
  QDir sourceDirectory(legacyPresetDirectory);
  if (!sourceDirectory.exists()) {
    result.messages << QStringLiteral("Legacy preset directory does not exist: %1")
                           .arg(legacyPresetDirectory);
    return result;
  }
  const QString sourcePath =
      QDir::cleanPath(QFileInfo(legacyPresetDirectory).absoluteFilePath());
  const QString destinationPath =
      QDir::cleanPath(QFileInfo(modernPresetDirectory).absoluteFilePath());
  if (sourcePath == destinationPath) {
    result.messages << QStringLiteral(
        "Refusing to import because the legacy and modern preset directories "
        "are identical.");
    ++result.rejectedPresets;
    return result;
  }
  if (!QDir().mkpath(modernPresetDirectory)) {
    result.messages << QStringLiteral("Cannot create modern preset directory: %1")
                           .arg(modernPresetDirectory);
    ++result.rejectedPresets;
    return result;
  }

  const QStringList primaryNames = sourceDirectory.entryList(
      QStringList() << (QStringLiteral("*") + extension),
      QDir::Files | QDir::Readable, QDir::Name);
  for (const QString &primaryName : primaryNames) {
    const QString sourcePrimary = sourceDirectory.filePath(primaryName);
    const QString sourceAuxiliary = sourcePrimary + QStringLiteral(".aux");
    const bool hasAuxiliary = QFileInfo::exists(sourceAuxiliary);
    QByteArray primaryContents;
    QByteArray auxiliaryContents;
    QString error;

    if (!readFile(sourcePrimary, &primaryContents, &error) ||
        !validateContents(primaryContents, expectedPID, validator, &error)) {
      ++result.rejectedPresets;
      result.messages << QStringLiteral("Rejected %1: %2").arg(primaryName, error);
      continue;
    }
    if (hasAuxiliary &&
        (!readFile(sourceAuxiliary, &auxiliaryContents, &error) ||
         !validateContents(auxiliaryContents, expectedPID, validator, &error))) {
      ++result.rejectedPresets;
      result.messages << QStringLiteral("Rejected pair %1: %2")
                             .arg(primaryName, error);
      continue;
    }

    QString destinationPrimary =
        QDir(modernPresetDirectory).filePath(primaryName);
    const bool conflict = QFileInfo::exists(destinationPrimary) ||
                          QFileInfo::exists(destinationPrimary +
                                            QStringLiteral(".aux"));
    if (conflict && conflictPolicy == ConflictPolicy::Skip) {
      ++result.skippedPresets;
      result.messages << QStringLiteral("Skipped existing preset %1")
                             .arg(primaryName);
      continue;
    }
    if (conflict && conflictPolicy == ConflictPolicy::Rename) {
      destinationPrimary = renamedPrimaryPath(
          modernPresetDirectory, primaryName, extension);
    }
    const QString destinationAuxiliary =
        destinationPrimary + QStringLiteral(".aux");

    const FileSnapshot oldPrimary = snapshot(destinationPrimary);
    const FileSnapshot oldAuxiliary = snapshot(destinationAuxiliary);
    if ((oldPrimary.existed && !oldPrimary.readable) ||
        (oldAuxiliary.existed && !oldAuxiliary.readable)) {
      ++result.rejectedPresets;
      result.messages << QStringLiteral(
          "Cannot safely replace %1 because the existing modern preset pair "
          "could not be read for rollback.")
                             .arg(primaryName);
      continue;
    }
    const QDateTime primaryModified = QFileInfo(sourcePrimary).lastModified();
    const QDateTime auxiliaryModified =
        hasAuxiliary ? QFileInfo(sourceAuxiliary).lastModified() : QDateTime();

    if (!writeAtomically(destinationPrimary, primaryContents, primaryModified,
                         &error)) {
      restoreSnapshot(destinationPrimary, oldPrimary);
      ++result.rejectedPresets;
      result.messages << error;
      continue;
    }

    bool pairComplete = true;
    if (hasAuxiliary) {
      pairComplete = writeAtomically(destinationAuxiliary, auxiliaryContents,
                                     auxiliaryModified, &error);
    } else if (oldAuxiliary.existed &&
               conflictPolicy == ConflictPolicy::Replace) {
      pairComplete = QFile::remove(destinationAuxiliary);
      if (!pairComplete) {
        error = QStringLiteral("Could not remove obsolete auxiliary file %1")
                    .arg(destinationAuxiliary);
      }
    }

    if (!pairComplete) {
      restoreSnapshot(destinationPrimary, oldPrimary);
      restoreSnapshot(destinationAuxiliary, oldAuxiliary);
      ++result.rejectedPresets;
      result.messages << error;
      continue;
    }

    ++result.importedPresets;
    result.importedFiles += hasAuxiliary ? 2 : 1;
    result.importedPrimaryPaths << destinationPrimary;
  }

  const QStringList auxiliaryNames = sourceDirectory.entryList(
      QStringList() << (QStringLiteral("*") + extension +
                        QStringLiteral(".aux")),
      QDir::Files | QDir::Readable, QDir::Name);
  for (const QString &auxiliaryName : auxiliaryNames) {
    QString primaryName = auxiliaryName;
    primaryName.chop(4);
    if (!QFileInfo::exists(sourceDirectory.filePath(primaryName))) {
      ++result.rejectedPresets;
      result.messages << QStringLiteral("Rejected orphan auxiliary file %1")
                             .arg(auxiliaryName);
    }
  }

  return result;
}

QStringList whitelistedPreferenceKeys() {
  return QStringList() << QStringLiteral("appearance/mode");
}

PreferenceImportResult importWhitelistedPreferences(
    QSettings &legacySettings, QSettings &modernSettings) {
  PreferenceImportResult result;
  const QString key = QStringLiteral("appearance/mode");
  if (!legacySettings.contains(key)) {
    result.messages << QStringLiteral("Legacy appearance preference was not present.");
    return result;
  }

  const QString mode = legacySettings.value(key).toString().toLower();
  if (mode != QStringLiteral("light") && mode != QStringLiteral("dark") &&
      mode != QStringLiteral("system")) {
    ++result.rejectedKeys;
    result.messages << QStringLiteral("Legacy appearance preference was invalid.");
    return result;
  }

  modernSettings.setValue(key, mode);
  modernSettings.sync();
  if (modernSettings.status() != QSettings::NoError) {
    ++result.rejectedKeys;
    result.messages << QStringLiteral("Could not write the modern appearance preference.");
    return result;
  }

  result.importedKeys = 1;
  result.importedKeyNames << key;
  return result;
}

}  // namespace LegacyDataImport
