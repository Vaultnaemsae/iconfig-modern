#include "LegacyDataImport.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int assertions = 0;
int failures = 0;

void check(bool condition, const QString &message) {
  ++assertions;
  QTextStream(stdout) << (condition ? "PASS " : "FAIL ") << message
                      << Qt::endl;
  if (!condition) {
    ++failures;
  }
}

QByteArray makePreset(quint8 pid, quint8 version = 1) {
  QByteArray contents("iCM", 3);
  contents.append(static_cast<char>(pid));
  contents.append(static_cast<char>(version));
  if (version == 2) {
    const QByteArray description("namespace test");
    contents.append(static_cast<char>(description.size()));
    contents.append(description);
  }
  contents.append(QCryptographicHash::hash(contents, QCryptographicHash::Md5));
  return contents;
}

bool writeFile(const QString &path, const QByteArray &contents) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  return file.write(contents) == contents.size();
}

QByteArray readFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return QByteArray();
  }
  return file.readAll();
}

QByteArray sha256(const QString &path) {
  return QCryptographicHash::hash(readFile(path), QCryptographicHash::Sha256);
}

void verifyNamespacePaths() {
  const auto modern = LegacyDataImport::activeNamespacePaths();
  const auto legacy = LegacyDataImport::legacyNamespacePaths();
  check(modern.settingsFile != legacy.settingsFile,
        "modern and legacy QSettings files differ");
  check(modern.appDataDirectory != legacy.appDataDirectory,
        "modern and legacy application-data paths differ");
  check(modern.presetDirectory != legacy.presetDirectory,
        "modern and legacy preset paths differ");
  check(!modern.presetDirectory.startsWith(legacy.appDataDirectory + '/'),
        "modern preset path is outside the legacy data tree");
  check(modern.settingsFile.contains("com.vaultnaemsae.iConfig Modern"),
        "modern settings file uses the Vaultnaemsae namespace");
  check(modern.appDataDirectory.endsWith("Vaultnaemsae/iConfig Modern"),
        "modern application data uses the Vaultnaemsae namespace");
  check(legacy.settingsFile.contains("com.iconnectivity.iConnectivity iConfig"),
        "legacy settings path is explicitly identified");
}

void verifyPresetImport() {
  QTemporaryDir temporary;
  check(temporary.isValid(), "preset import temporary root exists");
  const QString source = QDir(temporary.path()).filePath("legacy");
  const QString destination = QDir(temporary.path()).filePath("modern");
  QDir().mkpath(source);
  const quint8 pid = 7;
  const QByteArray primary = makePreset(pid, 2);
  const QByteArray auxiliary = makePreset(pid, 1);
  const QString primaryPath = QDir(source).filePath("Pair.ica4");
  const QString auxiliaryPath = primaryPath + ".aux";
  check(writeFile(primaryPath, primary) && writeFile(auxiliaryPath, auxiliary),
        "legacy primary and auxiliary test files created");
  const QByteArray sourcePrimaryHash = sha256(primaryPath);
  const QByteArray sourceAuxiliaryHash = sha256(auxiliaryPath);

  const auto identicalPaths = LegacyDataImport::importPresets(
      source, source, ".ica4", pid,
      LegacyDataImport::ConflictPolicy::Replace);
  check(identicalPaths.importedPresets == 0 &&
            identicalPaths.rejectedPresets == 1 &&
            sha256(primaryPath) == sourcePrimaryHash &&
            sha256(auxiliaryPath) == sourceAuxiliaryHash,
        "import refuses identical legacy and modern directories");

  auto imported = LegacyDataImport::importPresets(
      source, destination, ".ica4", pid,
      LegacyDataImport::ConflictPolicy::Skip);
  const QString importedPrimary = QDir(destination).filePath("Pair.ica4");
  check(imported.importedPresets == 1 && imported.importedFiles == 2,
        "valid primary and auxiliary pair imports together");
  check(readFile(importedPrimary) == primary &&
            readFile(importedPrimary + ".aux") == auxiliary,
        "imported pair is byte-identical to its legacy source");
  check(sha256(primaryPath) == sourcePrimaryHash &&
            sha256(auxiliaryPath) == sourceAuxiliaryHash,
        "legacy preset hashes remain unchanged after import");

  auto skipped = LegacyDataImport::importPresets(
      source, destination, ".ica4", pid,
      LegacyDataImport::ConflictPolicy::Skip);
  check(skipped.skippedPresets == 1 && skipped.importedPresets == 0,
        "preset conflict defaults safely to Skip");

  auto renamed = LegacyDataImport::importPresets(
      source, destination, ".ica4", pid,
      LegacyDataImport::ConflictPolicy::Rename);
  const QString renamedPrimary =
      QDir(destination).filePath("Pair (Imported).ica4");
  check(renamed.importedPresets == 1 && QFileInfo::exists(renamedPrimary) &&
            QFileInfo::exists(renamedPrimary + ".aux"),
        "Rename keeps a conflicting primary and auxiliary pair coherent");

  check(writeFile(importedPrimary, QByteArray("old")) &&
            writeFile(importedPrimary + ".aux", QByteArray("old aux")),
        "replace-conflict destination prepared");
  auto replaced = LegacyDataImport::importPresets(
      source, destination, ".ica4", pid,
      LegacyDataImport::ConflictPolicy::Replace);
  check(replaced.importedPresets == 1 && readFile(importedPrimary) == primary &&
            readFile(importedPrimary + ".aux") == auxiliary,
        "explicit Replace atomically replaces the modern pair");

  const QString badPrimary = QDir(source).filePath("Bad.ica4");
  check(writeFile(badPrimary, primary) &&
            writeFile(badPrimary + ".aux", QByteArray("bad aux")),
        "invalid auxiliary test pair created");
  auto invalidPair = LegacyDataImport::importPresets(
      source, destination, ".ica4", pid,
      LegacyDataImport::ConflictPolicy::Skip);
  check(invalidPair.rejectedPresets >= 1 &&
            !QFileInfo::exists(QDir(destination).filePath("Bad.ica4")),
        "invalid auxiliary rejects the complete pair before copying");

  const QString wrongPID = QDir(source).filePath("Wrong.ica4");
  check(writeFile(wrongPID, makePreset(pid + 1)),
        "wrong-device preset test file created");
  auto wrongDevice = LegacyDataImport::importPresets(
      source, destination, ".ica4", pid,
      LegacyDataImport::ConflictPolicy::Skip);
  check(wrongDevice.rejectedPresets >= 2 &&
            !QFileInfo::exists(QDir(destination).filePath("Wrong.ica4")),
        "wrong-device preset is rejected");

  const QString orphan = QDir(source).filePath("Orphan.ica4.aux");
  check(writeFile(orphan, auxiliary), "orphan auxiliary test file created");
  auto orphanResult = LegacyDataImport::importPresets(
      source, destination, ".ica4", pid,
      LegacyDataImport::ConflictPolicy::Skip);
  check(orphanResult.rejectedPresets >= 3,
        "orphan auxiliary is reported and never imported alone");

  int validatorCalls = 0;
  const QString callbackSource = QDir(temporary.path()).filePath("callback");
  const QString callbackDestination =
      QDir(temporary.path()).filePath("callback-modern");
  QDir().mkpath(callbackSource);
  writeFile(QDir(callbackSource).filePath("Callback.ica4"), primary);
  const auto rejectedByProductionValidator = LegacyDataImport::importPresets(
      callbackSource, callbackDestination, ".ica4", pid,
      LegacyDataImport::ConflictPolicy::Skip,
      [&validatorCalls](const QByteArray &, QString *error) {
        ++validatorCalls;
        if (error) {
          *error = "production validation rejected test";
        }
        return false;
      });
  check(validatorCalls == 1 &&
            rejectedByProductionValidator.rejectedPresets == 1,
        "production preset validator participates before any copy");
}

void verifyPreferenceImport() {
  QTemporaryDir temporary;
  check(temporary.isValid(), "preference import temporary root exists");
  const QString legacyPath = QDir(temporary.path()).filePath("legacy.ini");
  const QString modernPath = QDir(temporary.path()).filePath("modern.ini");
  QSettings legacy(legacyPath, QSettings::IniFormat);
  legacy.setValue("appearance/mode", "dark");
  legacy.setValue("firmwareCheck/lastCheck", 1234);
  legacy.setValue("mainwindow/geometry-v2", QByteArray("opaque geometry"));
  legacy.sync();
  const QByteArray legacyHash = sha256(legacyPath);

  QSettings modern(modernPath, QSettings::IniFormat);
  const auto result = LegacyDataImport::importWhitelistedPreferences(
      legacy, modern);
  check(result.importedKeys == 1 &&
            result.importedKeyNames == QStringList("appearance/mode"),
        "only the whitelisted appearance preference is imported");
  check(modern.value("appearance/mode").toString() == "dark",
        "appearance value is copied to the modern namespace");
  check(!modern.contains("firmwareCheck/lastCheck") &&
            !modern.contains("mainwindow/geometry-v2"),
        "firmware and opaque geometry settings are not imported");
  check(sha256(legacyPath) == legacyHash,
        "legacy settings source remains byte-identical");

  legacy.setValue("appearance/mode", "invalid");
  legacy.sync();
  QSettings otherModern(QDir(temporary.path()).filePath("other.ini"),
                        QSettings::IniFormat);
  const auto invalid = LegacyDataImport::importWhitelistedPreferences(
      legacy, otherModern);
  check(invalid.rejectedKeys == 1 &&
            !otherModern.contains("appearance/mode"),
        "invalid legacy appearance is rejected without a partial import");
}

}  // namespace

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setOrganizationName("Vaultnaemsae");
  QCoreApplication::setOrganizationDomain("vaultnaemsae.com");
  QCoreApplication::setApplicationName("iConfig Modern");

  verifyNamespacePaths();
  verifyPresetImport();
  verifyPreferenceImport();

  QTextStream(stdout) << "RESULT assertions=" << assertions
                      << " failures=" << failures << Qt::endl;
  return failures == 0 ? 0 : 1;
}
